#include "CXNetworkGML.h"
#include "CXNetworkNodeLinkJSON.h"

#include "CXNetwork.h"
#include "CXDictionary.h"

#include <errno.h>
#include <stdbool.h>

typedef enum {
	InterchangeScopeGraph = 0,
	InterchangeScopeNode = 1,
	InterchangeScopeEdge = 2
} InterchangeScope;

typedef struct {
	char *message;
} InterchangeWarningState;

static InterchangeWarningState gInterchangeWarning = { NULL };

static void InterchangeWarningClear(void) {
	if (gInterchangeWarning.message) {
		free(gInterchangeWarning.message);
		gInterchangeWarning.message = NULL;
	}
}

static void InterchangeWarningAppend(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *segment = NULL;
	vasprintf(&segment, fmt, args);
	va_end(args);
	if (!segment) {
		return;
	}
	if (!gInterchangeWarning.message) {
		gInterchangeWarning.message = segment;
		return;
	}
	char *combined = CXNewStringFromFormat("%s; %s", gInterchangeWarning.message, segment);
	free(segment);
	if (!combined) {
		return;
	}
	free(gInterchangeWarning.message);
	gInterchangeWarning.message = combined;
}

const char* CXNetworkSerializationLastWarningMessage(void) {
	return gInterchangeWarning.message ? gInterchangeWarning.message : "";
}

static CXBool InterchangeCategoryDecodeId(const void *data, int32_t *outId) {
	if (!outId) {
		return CXFalse;
	}
	uintptr_t raw = (uintptr_t)data;
	if (raw == 0) {
		return CXFalse;
	}
	if (raw == 1u) {
		*outId = -1;
		return CXTrue;
	}
	*outId = (int32_t)(uint32_t)(raw - 2u);
	return CXTrue;
}

static const char* InterchangeCategoryLabelForId(CXAttributeRef attr, int32_t id) {
	if (!attr || !attr->categoricalDictionary) {
		return NULL;
	}
	CXStringDictionaryFOR(entry, attr->categoricalDictionary) {
		int32_t entryId = 0;
		if (InterchangeCategoryDecodeId(entry->data, &entryId) && entryId == id) {
			return entry->key;
		}
	}
	return NULL;
}

static const char* InterchangeScopeLabel(InterchangeScope scope) {
	switch (scope) {
		case InterchangeScopeGraph:
			return "graph";
		case InterchangeScopeNode:
			return "node";
		case InterchangeScopeEdge:
			return "edge";
		default:
			return "unknown";
	}
}

static CXBool InterchangeIsReservedGMLKey(InterchangeScope scope, const char *name) {
	if (!name) {
		return CXTrue;
	}
	if (strcmp(name, "graph") == 0 || strcmp(name, "node") == 0 || strcmp(name, "edge") == 0) {
		return CXTrue;
	}
	if (scope == InterchangeScopeGraph) {
		return strcmp(name, "directed") == 0;
	}
	if (scope == InterchangeScopeNode) {
		return strcmp(name, "id") == 0;
	}
	if (scope == InterchangeScopeEdge) {
		return strcmp(name, "source") == 0 || strcmp(name, "target") == 0;
	}
	return CXFalse;
}

static char* InterchangeSanitizeGMLKey(
	const char *name,
	InterchangeScope scope,
	CXStringDictionaryRef usedNames,
	CXBool *outChanged
) {
	if (outChanged) {
		*outChanged = CXFalse;
	}
	if (!name || !name[0]) {
		if (outChanged) {
			*outChanged = CXTrue;
		}
		name = "attr";
	}
	size_t len = strlen(name);
	char *sanitized = calloc(len + 16, sizeof(char));
	if (!sanitized) {
		return NULL;
	}
	size_t out = 0;
	for (size_t i = 0; i < len; i += 1) {
		unsigned char ch = (unsigned char)name[i];
		if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_' || (out > 0 && ch >= '0' && ch <= '9')) {
			sanitized[out++] = (char)ch;
		} else {
			sanitized[out++] = '_';
			if (outChanged) {
				*outChanged = CXTrue;
			}
		}
	}
	if (out == 0 || !((sanitized[0] >= 'A' && sanitized[0] <= 'Z') || (sanitized[0] >= 'a' && sanitized[0] <= 'z') || sanitized[0] == '_')) {
		memmove(sanitized + 5, sanitized, out + 1);
		memcpy(sanitized, "attr_", 5);
		out += 5;
		if (outChanged) {
			*outChanged = CXTrue;
		}
	}
	if (InterchangeIsReservedGMLKey(scope, sanitized)) {
		memmove(sanitized + 5, sanitized, out + 1);
		memcpy(sanitized, "attr_", 5);
		out += 5;
		if (outChanged) {
			*outChanged = CXTrue;
		}
	}
	if (usedNames) {
		char *candidate = sanitized;
		int suffix = 2;
		while (CXStringDictionaryEntryForKey(usedNames, candidate) != NULL) {
			char *next = CXNewStringFromFormat("%s_%d", sanitized, suffix++);
			if (candidate != sanitized) {
				free(candidate);
			}
			candidate = next;
			if (outChanged) {
				*outChanged = CXTrue;
			}
			if (!candidate) {
				free(sanitized);
				return NULL;
			}
		}
		CXStringDictionarySetEntry(usedNames, candidate, (void *)1);
		if (candidate != sanitized) {
			free(sanitized);
			sanitized = candidate;
		}
	}
	return sanitized;
}

static void InterchangeWriteGMLEscapedString(FILE *file, const char *text) {
	fputc('"', file);
	const unsigned char *cursor = (const unsigned char *)(text ? text : "");
	while (*cursor) {
		switch (*cursor) {
			case '\\':
				fputs("\\\\", file);
				break;
			case '"':
				fputs("\\\"", file);
				break;
			case '\n':
				fputs("\\n", file);
				break;
			case '\r':
				fputs("\\r", file);
				break;
			case '\t':
				fputs("\\t", file);
				break;
			default:
				fputc((int)*cursor, file);
				break;
		}
		cursor++;
	}
	fputc('"', file);
}

static void InterchangeWriteJSONEscapedString(FILE *file, const char *text) {
	fputc('"', file);
	const unsigned char *cursor = (const unsigned char *)(text ? text : "");
	while (*cursor) {
		switch (*cursor) {
			case '\\':
				fputs("\\\\", file);
				break;
			case '"':
				fputs("\\\"", file);
				break;
			case '\b':
				fputs("\\b", file);
				break;
			case '\f':
				fputs("\\f", file);
				break;
			case '\n':
				fputs("\\n", file);
				break;
			case '\r':
				fputs("\\r", file);
				break;
			case '\t':
				fputs("\\t", file);
				break;
			default:
				if (*cursor < 0x20u) {
					fprintf(file, "\\u%04x", (unsigned int)*cursor);
				} else {
					fputc((int)*cursor, file);
				}
				break;
		}
		cursor++;
	}
	fputc('"', file);
}

static void* InterchangeAttributeValuePtr(CXAttributeRef attr, CXIndex index) {
	if (!attr || !attr->data) {
		return NULL;
	}
	return attr->data + ((size_t)index * (size_t)attr->stride);
}

static CXBool InterchangeAttributeSupportedForGML(CXAttributeRef attr) {
	if (!attr) {
		return CXFalse;
	}
	switch (attr->type) {
		case CXBooleanAttributeType:
		case CXFloatAttributeType:
		case CXDoubleAttributeType:
		case CXIntegerAttributeType:
		case CXUnsignedIntegerAttributeType:
		case CXBigIntegerAttributeType:
		case CXUnsignedBigIntegerAttributeType:
		case CXStringAttributeType:
		case CXDataAttributeCategoryType:
			return CXTrue;
		default:
			return CXFalse;
	}
}

static CXBool InterchangeAttributeSupportedForNodeLinkJSON(CXAttributeRef attr) {
	if (!attr) {
		return CXFalse;
	}
	switch (attr->type) {
		case CXBooleanAttributeType:
		case CXFloatAttributeType:
		case CXDoubleAttributeType:
		case CXIntegerAttributeType:
		case CXUnsignedIntegerAttributeType:
		case CXBigIntegerAttributeType:
		case CXUnsignedBigIntegerAttributeType:
		case CXStringAttributeType:
		case CXDataAttributeCategoryType:
		case CXDataAttributeMultiCategoryType:
			return CXTrue;
		default:
			return CXFalse;
	}
}

static CXBool InterchangeWriteGMLScalar(FILE *file, CXAttributeRef attr, CXIndex index) {
	void *ptr = InterchangeAttributeValuePtr(attr, index);
	if (!ptr) {
		return CXFalse;
	}
	switch (attr->type) {
		case CXBooleanAttributeType: {
			uint8_t value = *((uint8_t *)ptr);
			fprintf(file, "%u", value ? 1u : 0u);
			return CXTrue;
		}
		case CXFloatAttributeType:
			fprintf(file, "%.9g", (double)(*((float *)ptr)));
			return CXTrue;
		case CXDoubleAttributeType:
			fprintf(file, "%.17g", *((double *)ptr));
			return CXTrue;
		case CXIntegerAttributeType:
			fprintf(file, "%" PRId32, *((int32_t *)ptr));
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
			fprintf(file, "%" PRIu32, *((uint32_t *)ptr));
			return CXTrue;
		case CXBigIntegerAttributeType:
			fprintf(file, "%" PRId64, *((int64_t *)ptr));
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			fprintf(file, "%" PRIu64, *((uint64_t *)ptr));
			return CXTrue;
		case CXStringAttributeType: {
			CXString value = *((CXString *)ptr);
			if (!value) {
				return CXFalse;
			}
			InterchangeWriteGMLEscapedString(file, value);
			return CXTrue;
		}
		case CXDataAttributeCategoryType: {
			int32_t raw = *((int32_t *)ptr);
			const char *label = InterchangeCategoryLabelForId(attr, raw);
			if (label) {
				InterchangeWriteGMLEscapedString(file, label);
			} else {
				fprintf(file, "%" PRId32, raw);
			}
			return CXTrue;
		}
		default:
			return CXFalse;
	}
}

static CXBool InterchangeWriteGMLAttributeLines(
	FILE *file,
	CXStringDictionaryRef dictionary,
	CXIndex index,
	InterchangeScope scope,
	int indent
) {
	if (!dictionary) {
		return CXTrue;
	}
	CXStringDictionaryRef usedNames = CXNewStringDictionary();
	if (!usedNames) {
		return CXFalse;
	}
	CXStringDictionaryFOR(entry, dictionary) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		if (!attr) {
			continue;
		}
		if (!InterchangeAttributeSupportedForGML(attr)) {
			InterchangeWarningAppend("GML skipped unsupported %s attribute \"%s\"", InterchangeScopeLabel(scope), entry->key);
			continue;
		}
		CXBool renamed = CXFalse;
		char *safeBase = InterchangeSanitizeGMLKey(entry->key, scope, usedNames, &renamed);
		if (!safeBase) {
			CXStringDictionaryDestroy(usedNames);
			return CXFalse;
		}
		if (renamed) {
			InterchangeWarningAppend("GML renamed %s attribute \"%s\" to \"%s\"", InterchangeScopeLabel(scope), entry->key, safeBase);
		}
		size_t dimension = attr->dimension > 0 ? (size_t)attr->dimension : 1u;
		if (dimension == 1u) {
			for (int i = 0; i < indent; i++) {
				fputc(' ', file);
			}
			fprintf(file, "%s ", safeBase);
			if (!InterchangeWriteGMLScalar(file, attr, index)) {
				fputc('\n', file);
				free(safeBase);
				CXStringDictionaryDestroy(usedNames);
				return CXFalse;
			}
			fputc('\n', file);
		} else {
			for (size_t component = 0; component < dimension; component += 1) {
				char *componentName = CXNewStringFromFormat("%s_%zu", safeBase, component);
				if (!componentName) {
					free(safeBase);
					CXStringDictionaryDestroy(usedNames);
					return CXFalse;
				}
				for (int i = 0; i < indent; i++) {
					fputc(' ', file);
				}
				fprintf(file, "%s ", componentName);
				CXAttribute temp = *attr;
				temp.dimension = 1;
				temp.data = ((uint8_t *)InterchangeAttributeValuePtr(attr, index)) + component * attr->elementSize;
				temp.stride = attr->elementSize;
				if (!InterchangeWriteGMLScalar(file, &temp, 0)) {
					free(componentName);
					free(safeBase);
					CXStringDictionaryDestroy(usedNames);
					return CXFalse;
				}
				fputc('\n', file);
				free(componentName);
			}
			InterchangeWarningAppend("GML flattened multi-dimensional %s attribute \"%s\"", InterchangeScopeLabel(scope), entry->key);
		}
		free(safeBase);
	}
	CXStringDictionaryDestroy(usedNames);
	return !ferror(file);
}

static void InterchangeWriteJSONNumber(FILE *file, double value) {
	if (isfinite(value)) {
		fprintf(file, "%.17g", value);
	} else {
		fputs("null", file);
	}
}

static void InterchangeWriteJSONInteger64(FILE *file, int64_t value) {
	fprintf(file, "%" PRId64, value);
}

static void InterchangeWriteJSONUnsigned64(FILE *file, uint64_t value) {
	fprintf(file, "%" PRIu64, value);
}

static CXBool InterchangeWriteNodeLinkJSONValue(FILE *file, CXAttributeRef attr, CXIndex index) {
	if (!attr) {
		return CXFalse;
	}
	if (attr->type == CXDataAttributeMultiCategoryType) {
		if (!attr->multiCategory) {
			fputs("[]", file);
			return CXTrue;
		}
		uint32_t *offsets = attr->multiCategory->offsets;
		uint32_t *ids = attr->multiCategory->ids;
		float *weights = attr->multiCategory->weights;
		CXSize entryCount = attr->multiCategory->entryCount;
		if (!offsets || index + 1 >= attr->capacity) {
			fputs("[]", file);
			return CXTrue;
		}
		uint32_t start = offsets[index];
		uint32_t end = offsets[index + 1];
		if (end < start || end > (uint32_t)entryCount) {
			fputs("[]", file);
			return CXTrue;
		}
		fputc('[', file);
		for (uint32_t cursor = start; cursor < end; cursor += 1) {
			if (cursor > start) {
				fputs(", ", file);
			}
			int32_t categoryId = (int32_t)ids[cursor];
			const char *label = InterchangeCategoryLabelForId(attr, categoryId);
			if (attr->multiCategory->hasWeights && weights) {
				fputs("{\"label\": ", file);
				if (label) {
					InterchangeWriteJSONEscapedString(file, label);
				} else {
					InterchangeWriteJSONInteger64(file, categoryId);
				}
				fputs(", \"weight\": ", file);
				InterchangeWriteJSONNumber(file, weights[cursor]);
				fputc('}', file);
			} else if (label) {
				InterchangeWriteJSONEscapedString(file, label);
			} else {
				InterchangeWriteJSONInteger64(file, categoryId);
			}
		}
		fputc(']', file);
		return CXTrue;
	}
	void *ptr = InterchangeAttributeValuePtr(attr, index);
	if (!ptr) {
		fputs("null", file);
		return CXTrue;
	}
	size_t dimension = attr->dimension > 0 ? (size_t)attr->dimension : 1u;
	if (dimension > 1u) {
		fputc('[', file);
		for (size_t component = 0; component < dimension; component += 1) {
			if (component > 0) {
				fputs(", ", file);
			}
			CXAttribute temp = *attr;
			temp.dimension = 1;
			temp.data = ((uint8_t *)ptr) + component * attr->elementSize;
			temp.stride = attr->elementSize;
			if (!InterchangeWriteNodeLinkJSONValue(file, &temp, 0)) {
				return CXFalse;
			}
		}
		fputc(']', file);
		return CXTrue;
	}
	switch (attr->type) {
		case CXBooleanAttributeType:
			fputs(*((uint8_t *)ptr) ? "true" : "false", file);
			return CXTrue;
		case CXFloatAttributeType:
			InterchangeWriteJSONNumber(file, *((float *)ptr));
			return CXTrue;
		case CXDoubleAttributeType:
			InterchangeWriteJSONNumber(file, *((double *)ptr));
			return CXTrue;
		case CXIntegerAttributeType:
			fprintf(file, "%" PRId32, *((int32_t *)ptr));
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
			fprintf(file, "%" PRIu32, *((uint32_t *)ptr));
			return CXTrue;
		case CXBigIntegerAttributeType:
			InterchangeWriteJSONInteger64(file, *((int64_t *)ptr));
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			InterchangeWriteJSONUnsigned64(file, *((uint64_t *)ptr));
			return CXTrue;
		case CXStringAttributeType: {
			CXString value = *((CXString *)ptr);
			if (!value) {
				fputs("null", file);
			} else {
				InterchangeWriteJSONEscapedString(file, value);
			}
			return CXTrue;
		}
		case CXDataAttributeCategoryType: {
			int32_t raw = *((int32_t *)ptr);
			const char *label = InterchangeCategoryLabelForId(attr, raw);
			if (label) {
				InterchangeWriteJSONEscapedString(file, label);
			} else {
				fprintf(file, "%" PRId32, raw);
			}
			return CXTrue;
		}
		default:
			return CXFalse;
	}
}

static CXBool InterchangeWriteNodeLinkJSONObjectAttributes(
	FILE *file,
	CXStringDictionaryRef dictionary,
	CXIndex index,
	InterchangeScope scope,
	const char **reserved,
	size_t reservedCount
) {
	if (!dictionary) {
		return CXTrue;
	}
	CXBool firstInline = CXTrue;
	CXBool needsReservedObject = CXFalse;
	CXStringDictionaryFOR(entry, dictionary) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		if (!attr) {
			continue;
		}
		if (!InterchangeAttributeSupportedForNodeLinkJSON(attr)) {
			InterchangeWarningAppend("node-link JSON skipped unsupported %s attribute \"%s\"", InterchangeScopeLabel(scope), entry->key);
			continue;
		}
		CXBool isReserved = CXFalse;
		for (size_t r = 0; r < reservedCount; r += 1) {
			if (strcmp(entry->key, reserved[r]) == 0) {
				isReserved = CXTrue;
				break;
			}
		}
		if (isReserved) {
			needsReservedObject = CXTrue;
			continue;
		}
		fputs(", ", file);
		InterchangeWriteJSONEscapedString(file, entry->key);
		fputs(": ", file);
		if (!InterchangeWriteNodeLinkJSONValue(file, attr, index)) {
			return CXFalse;
		}
		firstInline = CXFalse;
		(void)firstInline;
	}
	if (needsReservedObject) {
		fputs(", \"attributes\": {", file);
		CXBool firstReserved = CXTrue;
		CXStringDictionaryFOR(entry, dictionary) {
			CXAttributeRef attr = (CXAttributeRef)entry->data;
			if (!attr || !InterchangeAttributeSupportedForNodeLinkJSON(attr)) {
				continue;
			}
			CXBool isReserved = CXFalse;
			for (size_t r = 0; r < reservedCount; r += 1) {
				if (strcmp(entry->key, reserved[r]) == 0) {
					isReserved = CXTrue;
					break;
				}
			}
			if (!isReserved) {
				continue;
			}
			if (!firstReserved) {
				fputs(", ", file);
			}
			firstReserved = CXFalse;
			InterchangeWriteJSONEscapedString(file, entry->key);
			fputs(": ", file);
			if (!InterchangeWriteNodeLinkJSONValue(file, attr, index)) {
				return CXFalse;
			}
		}
		fputc('}', file);
		InterchangeWarningAppend("node-link JSON moved reserved %s attributes into \"attributes\"", InterchangeScopeLabel(scope));
	}
	return !ferror(file);
}

CXBool CXNetworkWriteNodeLinkJSON(CXNetworkRef network, const char *path) {
	InterchangeWarningClear();
	if (!network || !path) {
		errno = EINVAL;
		return CXFalse;
	}
	FILE *file = fopen(path, "wb");
	if (!file) {
		return CXFalse;
	}
	const char *nodeReserved[] = { "id", "attributes" };
	const char *edgeReserved[] = { "source", "target", "attributes" };
	fputs("{\n  \"directed\": ", file);
	fputs(network->isDirected ? "true" : "false", file);
	fputs(",\n  \"multigraph\": false,\n  \"graph\": {", file);
	CXBool firstGraph = CXTrue;
	if (network->networkAttributes) {
		CXStringDictionaryFOR(entry, network->networkAttributes) {
			CXAttributeRef attr = (CXAttributeRef)entry->data;
			if (!attr) {
				continue;
			}
			if (!InterchangeAttributeSupportedForNodeLinkJSON(attr)) {
				InterchangeWarningAppend("node-link JSON skipped unsupported graph attribute \"%s\"", entry->key);
				continue;
			}
			if (!firstGraph) {
				fputs(", ", file);
			}
			firstGraph = CXFalse;
			InterchangeWriteJSONEscapedString(file, entry->key);
			fputs(": ", file);
			if (!InterchangeWriteNodeLinkJSONValue(file, attr, 0)) {
				fclose(file);
				return CXFalse;
			}
		}
	}
	fputs("},\n  \"nodes\": [", file);
	CXBool firstNode = CXTrue;
	for (CXIndex node = 0; node < network->nodeCapacity; node += 1) {
		if (!network->nodeActive || !network->nodeActive[node]) {
			continue;
		}
		if (!firstNode) {
			fputs(",", file);
		}
		firstNode = CXFalse;
		fprintf(file, "\n    {\"id\": %" PRIu64, (uint64_t)node);
		if (!InterchangeWriteNodeLinkJSONObjectAttributes(file, network->nodeAttributes, node, InterchangeScopeNode, nodeReserved, 2)) {
			fclose(file);
			return CXFalse;
		}
		fputc('}', file);
	}
	fputs("\n  ],\n  \"links\": [", file);
	CXBool firstEdge = CXTrue;
	for (CXIndex edge = 0; edge < network->edgeCapacity; edge += 1) {
		if (!network->edgeActive || !network->edgeActive[edge]) {
			continue;
		}
		if (!firstEdge) {
			fputs(",", file);
		}
		firstEdge = CXFalse;
		fprintf(
			file,
			"\n    {\"source\": %" PRIu64 ", \"target\": %" PRIu64,
			(uint64_t)network->edges[edge].from,
			(uint64_t)network->edges[edge].to
		);
		if (!InterchangeWriteNodeLinkJSONObjectAttributes(file, network->edgeAttributes, edge, InterchangeScopeEdge, edgeReserved, 3)) {
			fclose(file);
			return CXFalse;
		}
		fputc('}', file);
	}
	fputs("\n  ]\n}\n", file);
	CXBool ok = !ferror(file);
	fclose(file);
	return ok;
}

CXBool CXNetworkWriteGML(CXNetworkRef network, const char *path) {
	InterchangeWarningClear();
	if (!network || !path) {
		errno = EINVAL;
		return CXFalse;
	}
	FILE *file = fopen(path, "wb");
	if (!file) {
		return CXFalse;
	}
	fputs("graph [\n", file);
	fprintf(file, "  directed %u\n", network->isDirected ? 1u : 0u);
	if (!InterchangeWriteGMLAttributeLines(file, network->networkAttributes, 0, InterchangeScopeGraph, 2)) {
		fclose(file);
		return CXFalse;
	}
	for (CXIndex node = 0; node < network->nodeCapacity; node += 1) {
		if (!network->nodeActive || !network->nodeActive[node]) {
			continue;
		}
		fputs("  node [\n", file);
		fprintf(file, "    id %" PRIu64 "\n", (uint64_t)node);
		if (!InterchangeWriteGMLAttributeLines(file, network->nodeAttributes, node, InterchangeScopeNode, 4)) {
			fclose(file);
			return CXFalse;
		}
		fputs("  ]\n", file);
	}
	for (CXIndex edge = 0; edge < network->edgeCapacity; edge += 1) {
		if (!network->edgeActive || !network->edgeActive[edge]) {
			continue;
		}
		fputs("  edge [\n", file);
		fprintf(file, "    source %" PRIu64 "\n", (uint64_t)network->edges[edge].from);
		fprintf(file, "    target %" PRIu64 "\n", (uint64_t)network->edges[edge].to);
		if (!InterchangeWriteGMLAttributeLines(file, network->edgeAttributes, edge, InterchangeScopeEdge, 4)) {
			fclose(file);
			return CXFalse;
		}
		fputs("  ]\n", file);
	}
	fputs("]\n", file);
	CXBool ok = !ferror(file);
	fclose(file);
	return ok;
}

typedef enum {
	GMLTokenEOF = 0,
	GMLTokenIdentifier = 1,
	GMLTokenString = 2,
	GMLTokenInteger = 3,
	GMLTokenReal = 4,
	GMLTokenLBracket = 5,
	GMLTokenRBracket = 6
} GMLTokenType;

typedef struct {
	GMLTokenType type;
	char *text;
	int64_t integerValue;
	double realValue;
	size_t line;
	size_t column;
} GMLToken;

typedef struct {
	char *text;
	size_t length;
	size_t offset;
	size_t line;
	size_t column;
	GMLToken current;
	char *error;
} GMLLexer;

typedef enum {
	GMLValueString = 0,
	GMLValueInteger = 1,
	GMLValueReal = 2,
	GMLValueList = 3
} GMLValueType;

typedef struct GMLValue GMLValue;

typedef struct {
	char *key;
	GMLValue *value;
} GMLPair;

typedef struct {
	GMLPair *items;
	size_t count;
	size_t capacity;
} GMLList;

struct GMLValue {
	GMLValueType type;
	union {
		char *stringValue;
		int64_t integerValue;
		double realValue;
		GMLList listValue;
	} as;
};

typedef struct {
	char *idText;
	GMLList attributes;
} GMLParsedNode;

typedef struct {
	char *sourceText;
	char *targetText;
	GMLList attributes;
} GMLParsedEdge;

typedef struct {
	GMLParsedNode *items;
	size_t count;
	size_t capacity;
} GMLParsedNodeList;

typedef struct {
	GMLParsedEdge *items;
	size_t count;
	size_t capacity;
} GMLParsedEdgeList;

typedef struct {
	CXBool directed;
	GMLList graphAttributes;
	GMLParsedNodeList nodes;
	GMLParsedEdgeList edges;
} GMLParsedGraph;

typedef enum {
	GMLInferUnknown = 0,
	GMLInferString = 1,
	GMLInferSignedInt = 2,
	GMLInferUnsignedInt = 3,
	GMLInferReal = 4,
	GMLInferSkip = 5
} GMLInferKind;

typedef struct {
	char *name;
	GMLInferKind kind;
	int64_t minInt;
	uint64_t maxUInt;
	CXBool sawValue;
	CXBool coercedToString;
} GMLAttributeInfer;

static void GMLValueFree(GMLValue *value);

static void GMLTokenDispose(GMLToken *token) {
	if (!token) {
		return;
	}
	free(token->text);
	token->text = NULL;
	token->type = GMLTokenEOF;
	token->integerValue = 0;
	token->realValue = 0.0;
}

static void GMLLexerSetError(GMLLexer *lexer, const char *fmt, ...) {
	if (!lexer || lexer->error) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	char *body = NULL;
	vasprintf(&body, fmt, args);
	va_end(args);
	if (!body) {
		return;
	}
	lexer->error = CXNewStringFromFormat("Line %zu, column %zu: %s", lexer->line, lexer->column, body);
	free(body);
}

static int GMLLexerPeek(GMLLexer *lexer) {
	if (!lexer || lexer->offset >= lexer->length) {
		return EOF;
	}
	return (unsigned char)lexer->text[lexer->offset];
}

static int GMLLexerAdvance(GMLLexer *lexer) {
	if (!lexer || lexer->offset >= lexer->length) {
		return EOF;
	}
	unsigned char ch = (unsigned char)lexer->text[lexer->offset++];
	if (ch == '\n') {
		lexer->line += 1;
		lexer->column = 1;
	} else {
		lexer->column += 1;
	}
	return ch;
}

static void GMLLexerSkipSpace(GMLLexer *lexer) {
	for (;;) {
		int ch = GMLLexerPeek(lexer);
		if (ch == EOF) {
			return;
		}
		if (isspace(ch)) {
			GMLLexerAdvance(lexer);
			continue;
		}
		if (ch == '#') {
			while (ch != EOF && ch != '\n') {
				ch = GMLLexerAdvance(lexer);
			}
			continue;
		}
		return;
	}
}

static char* GMLLexerReadBareToken(GMLLexer *lexer) {
	size_t start = lexer->offset;
	while (lexer->offset < lexer->length) {
		unsigned char ch = (unsigned char)lexer->text[lexer->offset];
		if (isspace(ch) || ch == '[' || ch == ']' || ch == '#') {
			break;
		}
		lexer->offset += 1;
		lexer->column += 1;
	}
	size_t len = lexer->offset - start;
	char *token = calloc(len + 1, sizeof(char));
	if (!token) {
		return NULL;
	}
	memcpy(token, lexer->text + start, len);
	token[len] = '\0';
	return token;
}

static CXBool GMLLexerAppendChar(char **buffer, size_t *length, size_t *capacity, char ch) {
	if (*length + 2 > *capacity) {
		size_t next = *capacity ? (*capacity * 2) : 16u;
		char *grown = realloc(*buffer, next);
		if (!grown) {
			return CXFalse;
		}
		*buffer = grown;
		*capacity = next;
	}
	(*buffer)[(*length)++] = ch;
	(*buffer)[*length] = '\0';
	return CXTrue;
}

static CXBool GMLLexerAppendUTF8(char **buffer, size_t *length, size_t *capacity, uint32_t codepoint) {
	if (codepoint <= 0x7Fu) {
		return GMLLexerAppendChar(buffer, length, capacity, (char)codepoint);
	}
	if (codepoint <= 0x7FFu) {
		return GMLLexerAppendChar(buffer, length, capacity, (char)(0xC0u | ((codepoint >> 6) & 0x1Fu)))
			&& GMLLexerAppendChar(buffer, length, capacity, (char)(0x80u | (codepoint & 0x3Fu)));
	}
	if (codepoint <= 0xFFFFu) {
		return GMLLexerAppendChar(buffer, length, capacity, (char)(0xE0u | ((codepoint >> 12) & 0x0Fu)))
			&& GMLLexerAppendChar(buffer, length, capacity, (char)(0x80u | ((codepoint >> 6) & 0x3Fu)))
			&& GMLLexerAppendChar(buffer, length, capacity, (char)(0x80u | (codepoint & 0x3Fu)));
	}
	return GMLLexerAppendChar(buffer, length, capacity, (char)(0xF0u | ((codepoint >> 18) & 0x07u)))
		&& GMLLexerAppendChar(buffer, length, capacity, (char)(0x80u | ((codepoint >> 12) & 0x3Fu)))
		&& GMLLexerAppendChar(buffer, length, capacity, (char)(0x80u | ((codepoint >> 6) & 0x3Fu)))
		&& GMLLexerAppendChar(buffer, length, capacity, (char)(0x80u | (codepoint & 0x3Fu)));
}

static int GMLHexValue(int ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	}
	if (ch >= 'a' && ch <= 'f') {
		return 10 + (ch - 'a');
	}
	if (ch >= 'A' && ch <= 'F') {
		return 10 + (ch - 'A');
	}
	return -1;
}

static char* GMLLexerReadQuotedString(GMLLexer *lexer) {
	GMLLexerAdvance(lexer);
	char *buffer = NULL;
	size_t length = 0;
	size_t capacity = 0;
	for (;;) {
		int ch = GMLLexerPeek(lexer);
		if (ch == EOF) {
			free(buffer);
			GMLLexerSetError(lexer, "unterminated string literal");
			return NULL;
		}
		GMLLexerAdvance(lexer);
		if (ch == '"') {
			break;
		}
		if (ch == '\\') {
			int escaped = GMLLexerPeek(lexer);
			if (escaped == EOF) {
				free(buffer);
				GMLLexerSetError(lexer, "unterminated escape sequence");
				return NULL;
			}
			GMLLexerAdvance(lexer);
			switch (escaped) {
				case '"':
				case '\\':
				case '/':
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, (char)escaped)) {
						free(buffer);
						return NULL;
					}
					break;
				case 'b':
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, '\b')) {
						free(buffer);
						return NULL;
					}
					break;
				case 'f':
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, '\f')) {
						free(buffer);
						return NULL;
					}
					break;
				case 'n':
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, '\n')) {
						free(buffer);
						return NULL;
					}
					break;
				case 'r':
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, '\r')) {
						free(buffer);
						return NULL;
					}
					break;
				case 't':
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, '\t')) {
						free(buffer);
						return NULL;
					}
					break;
				case 'u': {
					uint32_t codepoint = 0;
					for (int i = 0; i < 4; i += 1) {
						int hex = GMLLexerPeek(lexer);
						int value = GMLHexValue(hex);
						if (value < 0) {
							free(buffer);
							GMLLexerSetError(lexer, "invalid unicode escape");
							return NULL;
						}
						GMLLexerAdvance(lexer);
						codepoint = (codepoint << 4u) | (uint32_t)value;
					}
					if (!GMLLexerAppendUTF8(&buffer, &length, &capacity, codepoint)) {
						free(buffer);
						return NULL;
					}
					break;
				}
				default:
					if (!GMLLexerAppendChar(&buffer, &length, &capacity, (char)escaped)) {
						free(buffer);
						return NULL;
					}
					break;
			}
			continue;
		}
		if (!GMLLexerAppendChar(&buffer, &length, &capacity, (char)ch)) {
			free(buffer);
			return NULL;
		}
	}
	if (!buffer) {
		buffer = CXNewStringFromString("");
	}
	return buffer;
}

static CXBool GMLLexerNext(GMLLexer *lexer) {
	if (!lexer) {
		return CXFalse;
	}
	GMLTokenDispose(&lexer->current);
	GMLLexerSkipSpace(lexer);
	lexer->current.line = lexer->line;
	lexer->current.column = lexer->column;
	int ch = GMLLexerPeek(lexer);
	if (ch == EOF) {
		lexer->current.type = GMLTokenEOF;
		return CXTrue;
	}
	if (ch == '[') {
		GMLLexerAdvance(lexer);
		lexer->current.type = GMLTokenLBracket;
		return CXTrue;
	}
	if (ch == ']') {
		GMLLexerAdvance(lexer);
		lexer->current.type = GMLTokenRBracket;
		return CXTrue;
	}
	if (ch == '"') {
		char *text = GMLLexerReadQuotedString(lexer);
		if (!text && lexer->error) {
			return CXFalse;
		}
		lexer->current.type = GMLTokenString;
		lexer->current.text = text ? text : CXNewStringFromString("");
		return lexer->current.text != NULL;
	}
	char *text = GMLLexerReadBareToken(lexer);
	if (!text) {
		return CXFalse;
	}
	char *endInt = NULL;
	errno = 0;
	long long parsedInt = strtoll(text, &endInt, 10);
	if (errno == 0 && endInt && *endInt == '\0') {
		lexer->current.type = GMLTokenInteger;
		lexer->current.text = text;
		lexer->current.integerValue = (int64_t)parsedInt;
		return CXTrue;
	}
	char *endReal = NULL;
	errno = 0;
	double parsedReal = strtod(text, &endReal);
	if (errno == 0 && endReal && *endReal == '\0' && (strchr(text, '.') || strchr(text, 'e') || strchr(text, 'E'))) {
		lexer->current.type = GMLTokenReal;
		lexer->current.text = text;
		lexer->current.realValue = parsedReal;
		return CXTrue;
	}
	lexer->current.type = GMLTokenIdentifier;
	lexer->current.text = text;
	return CXTrue;
}

static void GMLListFree(GMLList *list) {
	if (!list) {
		return;
	}
	for (size_t i = 0; i < list->count; i += 1) {
		free(list->items[i].key);
		GMLValueFree(list->items[i].value);
		free(list->items[i].value);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void GMLValueFree(GMLValue *value) {
	if (!value) {
		return;
	}
	if (value->type == GMLValueString) {
		free(value->as.stringValue);
	} else if (value->type == GMLValueList) {
		GMLListFree(&value->as.listValue);
	}
}

static CXBool GMLListAppend(GMLList *list, char *key, GMLValue *value) {
	if (!list || !key || !value) {
		return CXFalse;
	}
	if (list->count + 1 > list->capacity) {
		size_t next = list->capacity ? (list->capacity * 2u) : 8u;
		GMLPair *grown = realloc(list->items, next * sizeof(GMLPair));
		if (!grown) {
			return CXFalse;
		}
		list->items = grown;
		list->capacity = next;
	}
	list->items[list->count].key = key;
	list->items[list->count].value = value;
	list->count += 1;
	return CXTrue;
}

static GMLValue* GMLParseValue(GMLLexer *lexer);

static GMLValue* GMLParseListValue(GMLLexer *lexer) {
	if (!lexer || lexer->current.type != GMLTokenLBracket) {
		return NULL;
	}
	GMLValue *value = calloc(1, sizeof(GMLValue));
	if (!value) {
		return NULL;
	}
	value->type = GMLValueList;
	if (!GMLLexerNext(lexer)) {
		free(value);
		return NULL;
	}
	while (lexer->current.type != GMLTokenRBracket) {
		if (lexer->current.type == GMLTokenEOF) {
			GMLValueFree(value);
			free(value);
			GMLLexerSetError(lexer, "unterminated list");
			return NULL;
		}
		if (lexer->current.type != GMLTokenIdentifier && lexer->current.type != GMLTokenString) {
			GMLValueFree(value);
			free(value);
			GMLLexerSetError(lexer, "expected key inside list");
			return NULL;
		}
		char *key = CXNewStringFromString(lexer->current.text ? lexer->current.text : "");
		if (!key) {
			GMLValueFree(value);
			free(value);
			return NULL;
		}
		if (!GMLLexerNext(lexer)) {
			free(key);
			GMLValueFree(value);
			free(value);
			return NULL;
		}
		GMLValue *child = GMLParseValue(lexer);
		if (!child) {
			free(key);
			GMLValueFree(value);
			free(value);
			return NULL;
		}
		if (!GMLListAppend(&value->as.listValue, key, child)) {
			free(key);
			GMLValueFree(child);
			free(child);
			GMLValueFree(value);
			free(value);
			return NULL;
		}
	}
	if (!GMLLexerNext(lexer)) {
		GMLValueFree(value);
		free(value);
		return NULL;
	}
	return value;
}

static GMLValue* GMLParseValue(GMLLexer *lexer) {
	if (!lexer) {
		return NULL;
	}
	GMLValue *value = calloc(1, sizeof(GMLValue));
	if (!value) {
		return NULL;
	}
	switch (lexer->current.type) {
		case GMLTokenString:
		case GMLTokenIdentifier:
			value->type = GMLValueString;
			value->as.stringValue = CXNewStringFromString(lexer->current.text ? lexer->current.text : "");
			if (!value->as.stringValue) {
				free(value);
				return NULL;
			}
			if (!GMLLexerNext(lexer)) {
				GMLValueFree(value);
				free(value);
				return NULL;
			}
			return value;
		case GMLTokenInteger:
			value->type = GMLValueInteger;
			value->as.integerValue = lexer->current.integerValue;
			if (!GMLLexerNext(lexer)) {
				free(value);
				return NULL;
			}
			return value;
		case GMLTokenReal:
			value->type = GMLValueReal;
			value->as.realValue = lexer->current.realValue;
			if (!GMLLexerNext(lexer)) {
				free(value);
				return NULL;
			}
			return value;
		case GMLTokenLBracket:
			free(value);
			return GMLParseListValue(lexer);
		default:
			free(value);
			GMLLexerSetError(lexer, "unexpected token while parsing value");
			return NULL;
	}
}

static const GMLValue* GMLListFindLastValue(const GMLList *list, const char *key) {
	if (!list || !key) {
		return NULL;
	}
	for (size_t i = list->count; i > 0; i -= 1) {
		if (strcmp(list->items[i - 1].key, key) == 0) {
			return list->items[i - 1].value;
		}
	}
	return NULL;
}

static void GMLParsedNodeListFree(GMLParsedNodeList *list) {
	if (!list) {
		return;
	}
	for (size_t i = 0; i < list->count; i += 1) {
		free(list->items[i].idText);
		GMLListFree(&list->items[i].attributes);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void GMLParsedEdgeListFree(GMLParsedEdgeList *list) {
	if (!list) {
		return;
	}
	for (size_t i = 0; i < list->count; i += 1) {
		free(list->items[i].sourceText);
		free(list->items[i].targetText);
		GMLListFree(&list->items[i].attributes);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void GMLParsedGraphFree(GMLParsedGraph *graph) {
	if (!graph) {
		return;
	}
	GMLListFree(&graph->graphAttributes);
	GMLParsedNodeListFree(&graph->nodes);
	GMLParsedEdgeListFree(&graph->edges);
}

static CXBool GMLParsedNodeListAppend(GMLParsedNodeList *list, GMLParsedNode *node) {
	if (list->count + 1 > list->capacity) {
		size_t next = list->capacity ? (list->capacity * 2u) : 8u;
		GMLParsedNode *grown = realloc(list->items, next * sizeof(GMLParsedNode));
		if (!grown) {
			return CXFalse;
		}
		list->items = grown;
		list->capacity = next;
	}
	list->items[list->count++] = *node;
	return CXTrue;
}

static CXBool GMLParsedEdgeListAppend(GMLParsedEdgeList *list, GMLParsedEdge *edge) {
	if (list->count + 1 > list->capacity) {
		size_t next = list->capacity ? (list->capacity * 2u) : 8u;
		GMLParsedEdge *grown = realloc(list->items, next * sizeof(GMLParsedEdge));
		if (!grown) {
			return CXFalse;
		}
		list->items = grown;
		list->capacity = next;
	}
	list->items[list->count++] = *edge;
	return CXTrue;
}

static char* GMLValueToOwnedString(const GMLValue *value) {
	if (!value) {
		return NULL;
	}
	switch (value->type) {
		case GMLValueString:
			return CXNewStringFromString(value->as.stringValue ? value->as.stringValue : "");
		case GMLValueInteger:
			return CXNewStringFromFormat("%" PRId64, value->as.integerValue);
		case GMLValueReal:
			return CXNewStringFromFormat("%.17g", value->as.realValue);
		default:
			return NULL;
	}
}

static CXBool GMLValueToBool(const GMLValue *value, CXBool *outValue) {
	if (!value || !outValue) {
		return CXFalse;
	}
	switch (value->type) {
		case GMLValueInteger:
			*outValue = value->as.integerValue != 0 ? CXTrue : CXFalse;
			return CXTrue;
		case GMLValueReal:
			*outValue = fabs(value->as.realValue) > 0.0 ? CXTrue : CXFalse;
			return CXTrue;
		case GMLValueString:
			if (!value->as.stringValue) {
				return CXFalse;
			}
			if (strcasecmp(value->as.stringValue, "true") == 0 || strcmp(value->as.stringValue, "1") == 0 || strcasecmp(value->as.stringValue, "yes") == 0) {
				*outValue = CXTrue;
				return CXTrue;
			}
			if (strcasecmp(value->as.stringValue, "false") == 0 || strcmp(value->as.stringValue, "0") == 0 || strcasecmp(value->as.stringValue, "no") == 0) {
				*outValue = CXFalse;
				return CXTrue;
			}
			return CXFalse;
		default:
			return CXFalse;
	}
}

static CXBool GMLExtractParsedGraph(const GMLList *root, GMLParsedGraph *outGraph) {
	if (!root || !outGraph) {
		return CXFalse;
	}
	memset(outGraph, 0, sizeof(*outGraph));
	outGraph->directed = CXFalse;
	size_t autoNodeId = 0;
	for (size_t i = 0; i < root->count; i += 1) {
		const GMLPair *pair = &root->items[i];
		if (strcmp(pair->key, "directed") == 0) {
			CXBool directed = CXFalse;
			if (GMLValueToBool(pair->value, &directed)) {
				outGraph->directed = directed;
			}
			continue;
		}
		if (strcmp(pair->key, "node") == 0 && pair->value && pair->value->type == GMLValueList) {
			GMLParsedNode node = {0};
			const GMLValue *idValue = GMLListFindLastValue(&pair->value->as.listValue, "id");
			if (idValue) {
				node.idText = GMLValueToOwnedString(idValue);
			} else {
				node.idText = CXNewStringFromFormat("%zu", autoNodeId);
				InterchangeWarningAppend("GML assigned synthetic node ids to entries without an id");
			}
			autoNodeId += 1;
			if (!node.idText) {
				GMLParsedGraphFree(outGraph);
				return CXFalse;
			}
			for (size_t j = 0; j < pair->value->as.listValue.count; j += 1) {
				const GMLPair *child = &pair->value->as.listValue.items[j];
				if (strcmp(child->key, "id") == 0) {
					continue;
				}
				char *keyCopy = CXNewStringFromString(child->key);
				GMLValue *valueCopy = calloc(1, sizeof(GMLValue));
				if (!keyCopy || !valueCopy) {
					free(keyCopy);
					free(valueCopy);
					free(node.idText);
					GMLParsedGraphFree(outGraph);
					return CXFalse;
				}
				memcpy(valueCopy, child->value, sizeof(GMLValue));
				if (child->value->type == GMLValueString) {
					valueCopy->as.stringValue = CXNewStringFromString(child->value->as.stringValue ? child->value->as.stringValue : "");
				} else if (child->value->type == GMLValueList) {
					memset(&valueCopy->as.listValue, 0, sizeof(GMLList));
					for (size_t k = 0; k < child->value->as.listValue.count; k += 1) {
						const GMLPair *nested = &child->value->as.listValue.items[k];
						char *nestedKey = CXNewStringFromString(nested->key);
						GMLValue *nestedValue = calloc(1, sizeof(GMLValue));
						if (!nestedKey || !nestedValue) {
							free(nestedKey);
							free(nestedValue);
							GMLValueFree(valueCopy);
							free(valueCopy);
							free(keyCopy);
							free(node.idText);
							GMLParsedGraphFree(outGraph);
							return CXFalse;
						}
						memcpy(nestedValue, nested->value, sizeof(GMLValue));
						if (nested->value->type == GMLValueString) {
							nestedValue->as.stringValue = CXNewStringFromString(nested->value->as.stringValue ? nested->value->as.stringValue : "");
						}
						if (!GMLListAppend(&valueCopy->as.listValue, nestedKey, nestedValue)) {
							GMLValueFree(nestedValue);
							free(nestedValue);
							free(nestedKey);
							GMLValueFree(valueCopy);
							free(valueCopy);
							free(keyCopy);
							free(node.idText);
							GMLParsedGraphFree(outGraph);
							return CXFalse;
						}
					}
				}
				if (!GMLListAppend(&node.attributes, keyCopy, valueCopy)) {
					GMLValueFree(valueCopy);
					free(valueCopy);
					free(keyCopy);
					free(node.idText);
					GMLParsedGraphFree(outGraph);
					return CXFalse;
				}
			}
			if (!GMLParsedNodeListAppend(&outGraph->nodes, &node)) {
				free(node.idText);
				GMLListFree(&node.attributes);
				GMLParsedGraphFree(outGraph);
				return CXFalse;
			}
			continue;
		}
		if (strcmp(pair->key, "edge") == 0 && pair->value && pair->value->type == GMLValueList) {
			GMLParsedEdge edge = {0};
			const GMLValue *sourceValue = GMLListFindLastValue(&pair->value->as.listValue, "source");
			const GMLValue *targetValue = GMLListFindLastValue(&pair->value->as.listValue, "target");
			edge.sourceText = GMLValueToOwnedString(sourceValue);
			edge.targetText = GMLValueToOwnedString(targetValue);
			if (!edge.sourceText || !edge.targetText) {
				free(edge.sourceText);
				free(edge.targetText);
				GMLParsedGraphFree(outGraph);
				return CXFalse;
			}
			for (size_t j = 0; j < pair->value->as.listValue.count; j += 1) {
				const GMLPair *child = &pair->value->as.listValue.items[j];
				if (strcmp(child->key, "source") == 0 || strcmp(child->key, "target") == 0) {
					continue;
				}
				char *keyCopy = CXNewStringFromString(child->key);
				GMLValue *valueCopy = calloc(1, sizeof(GMLValue));
				if (!keyCopy || !valueCopy) {
					free(keyCopy);
					free(valueCopy);
					free(edge.sourceText);
					free(edge.targetText);
					GMLParsedGraphFree(outGraph);
					return CXFalse;
				}
				memcpy(valueCopy, child->value, sizeof(GMLValue));
				if (child->value->type == GMLValueString) {
					valueCopy->as.stringValue = CXNewStringFromString(child->value->as.stringValue ? child->value->as.stringValue : "");
				} else if (child->value->type == GMLValueList) {
					memset(&valueCopy->as.listValue, 0, sizeof(GMLList));
				}
				if (!GMLListAppend(&edge.attributes, keyCopy, valueCopy)) {
					GMLValueFree(valueCopy);
					free(valueCopy);
					free(keyCopy);
					free(edge.sourceText);
					free(edge.targetText);
					GMLParsedGraphFree(outGraph);
					return CXFalse;
				}
			}
			if (!GMLParsedEdgeListAppend(&outGraph->edges, &edge)) {
				free(edge.sourceText);
				free(edge.targetText);
				GMLListFree(&edge.attributes);
				GMLParsedGraphFree(outGraph);
				return CXFalse;
			}
			continue;
		}
		char *keyCopy = CXNewStringFromString(pair->key);
		GMLValue *valueCopy = calloc(1, sizeof(GMLValue));
		if (!keyCopy || !valueCopy) {
			free(keyCopy);
			free(valueCopy);
			GMLParsedGraphFree(outGraph);
			return CXFalse;
		}
		memcpy(valueCopy, pair->value, sizeof(GMLValue));
		if (pair->value->type == GMLValueString) {
			valueCopy->as.stringValue = CXNewStringFromString(pair->value->as.stringValue ? pair->value->as.stringValue : "");
		}
		if (!GMLListAppend(&outGraph->graphAttributes, keyCopy, valueCopy)) {
			GMLValueFree(valueCopy);
			free(valueCopy);
			free(keyCopy);
			GMLParsedGraphFree(outGraph);
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool GMLGraphEnsureNodeExists(GMLParsedGraph *graph, const char *idText) {
	if (!graph || !idText) {
		return CXFalse;
	}
	for (size_t i = 0; i < graph->nodes.count; i += 1) {
		if (strcmp(graph->nodes.items[i].idText, idText) == 0) {
			return CXTrue;
		}
	}
	GMLParsedNode node = {0};
	node.idText = CXNewStringFromString(idText);
	if (!node.idText) {
		return CXFalse;
	}
	if (!GMLParsedNodeListAppend(&graph->nodes, &node)) {
		free(node.idText);
		return CXFalse;
	}
	return CXTrue;
}

static CXBool GMLGraphEnsureEdgeEndpoints(GMLParsedGraph *graph) {
	if (!graph) {
		return CXFalse;
	}
	for (size_t i = 0; i < graph->edges.count; i += 1) {
		if (!GMLGraphEnsureNodeExists(graph, graph->edges.items[i].sourceText) ||
		    !GMLGraphEnsureNodeExists(graph, graph->edges.items[i].targetText)) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static GMLAttributeInfer* GMLInferLookup(CXStringDictionaryRef dictionary, const char *name) {
	return dictionary ? (GMLAttributeInfer *)CXStringDictionaryEntryForKey(dictionary, name) : NULL;
}

static GMLAttributeInfer* GMLInferEnsure(CXStringDictionaryRef dictionary, const char *name) {
	if (!dictionary || !name) {
		return NULL;
	}
	GMLAttributeInfer *infer = GMLInferLookup(dictionary, name);
	if (infer) {
		return infer;
	}
	infer = calloc(1, sizeof(GMLAttributeInfer));
	if (!infer) {
		return NULL;
	}
	infer->name = CXNewStringFromString(name);
	infer->kind = GMLInferUnknown;
	infer->minInt = INT64_MAX;
	infer->maxUInt = 0;
	if (!infer->name) {
		free(infer);
		return NULL;
	}
	CXStringDictionarySetEntry(dictionary, infer->name, infer);
	return infer;
}

static CXBool GMLInferObserveValue(GMLAttributeInfer *infer, const GMLValue *value) {
	if (!infer || !value) {
		return CXTrue;
	}
	if (value->type == GMLValueList) {
		infer->kind = GMLInferSkip;
		return CXTrue;
	}
	infer->sawValue = CXTrue;
	if (value->type == GMLValueString) {
		if (infer->kind == GMLInferSignedInt || infer->kind == GMLInferUnsignedInt || infer->kind == GMLInferReal) {
			infer->coercedToString = CXTrue;
		}
		infer->kind = GMLInferString;
		return CXTrue;
	}
	if (value->type == GMLValueReal) {
		if (infer->kind != GMLInferString) {
			infer->kind = GMLInferReal;
		}
		return CXTrue;
	}
	if (value->type == GMLValueInteger) {
		if (infer->kind == GMLInferUnknown) {
			infer->kind = value->as.integerValue < 0 ? GMLInferSignedInt : GMLInferUnsignedInt;
		} else if (infer->kind == GMLInferUnsignedInt && value->as.integerValue < 0) {
			infer->kind = GMLInferSignedInt;
		}
		if (value->as.integerValue < infer->minInt) {
			infer->minInt = value->as.integerValue;
		}
		if ((uint64_t)CXMAX((int64_t)0, value->as.integerValue) > infer->maxUInt) {
			infer->maxUInt = (uint64_t)CXMAX((int64_t)0, value->as.integerValue);
		}
	}
	return CXTrue;
}

static void GMLInferDictionaryDestroy(CXStringDictionaryRef dictionary) {
	if (!dictionary) {
		return;
	}
	CXStringDictionaryFOR(entry, dictionary) {
		GMLAttributeInfer *infer = (GMLAttributeInfer *)entry->data;
		if (infer) {
			free(infer->name);
			free(infer);
		}
	}
	CXStringDictionaryDestroy(dictionary);
}

static CXAttributeType GMLInferResolvedType(const GMLAttributeInfer *infer) {
	if (!infer) {
		return CXUnknownAttributeType;
	}
	switch (infer->kind) {
		case GMLInferString:
			return CXStringAttributeType;
		case GMLInferReal:
			return CXDoubleAttributeType;
		case GMLInferSignedInt:
			if (infer->minInt >= INT32_MIN && infer->maxUInt <= (uint64_t)INT32_MAX) {
				return CXIntegerAttributeType;
			}
			return CXBigIntegerAttributeType;
		case GMLInferUnsignedInt:
			if (infer->maxUInt <= UINT32_MAX) {
				return CXUnsignedIntegerAttributeType;
			}
			return CXUnsignedBigIntegerAttributeType;
		default:
			return CXUnknownAttributeType;
	}
}

static CXBool GMLCollectInferredAttributes(const GMLList *list, CXStringDictionaryRef inferMap) {
	if (!list || !inferMap) {
		return CXTrue;
	}
	for (size_t i = 0; i < list->count; i += 1) {
		GMLAttributeInfer *infer = GMLInferEnsure(inferMap, list->items[i].key);
		if (!infer) {
			return CXFalse;
		}
		if (!GMLInferObserveValue(infer, list->items[i].value)) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool GMLDefineAttributesFromInferMap(CXNetworkRef network, CXAttributeScope scope, CXStringDictionaryRef inferMap) {
	if (!network || !inferMap) {
		return CXTrue;
	}
	CXStringDictionaryFOR(entry, inferMap) {
		GMLAttributeInfer *infer = (GMLAttributeInfer *)entry->data;
		if (!infer || !infer->sawValue) {
			continue;
		}
		if (infer->kind == GMLInferSkip) {
			InterchangeWarningAppend("GML skipped nested %s attribute \"%s\"", InterchangeScopeLabel((InterchangeScope)(scope == CXAttributeScopeNetwork ? InterchangeScopeGraph : (scope == CXAttributeScopeNode ? InterchangeScopeNode : InterchangeScopeEdge))), infer->name);
			continue;
		}
		if (infer->coercedToString) {
			InterchangeWarningAppend("GML coerced mixed-type attribute \"%s\" to string", infer->name);
		}
		CXAttributeType type = GMLInferResolvedType(infer);
		if (type == CXUnknownAttributeType) {
			continue;
		}
		CXBool ok = CXFalse;
		if (scope == CXAttributeScopeNode) {
			ok = CXNetworkDefineNodeAttribute(network, infer->name, type, 1);
		} else if (scope == CXAttributeScopeEdge) {
			ok = CXNetworkDefineEdgeAttribute(network, infer->name, type, 1);
		} else {
			ok = CXNetworkDefineNetworkAttribute(network, infer->name, type, 1);
		}
		if (!ok) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool GMLAssignAttributeValue(CXAttributeRef attr, CXIndex index, const GMLValue *value) {
	if (!attr || !value) {
		return CXTrue;
	}
	void *ptr = InterchangeAttributeValuePtr(attr, index);
	if (!ptr) {
		return CXFalse;
	}
	switch (attr->type) {
		case CXStringAttributeType: {
			char *text = GMLValueToOwnedString(value);
			if (!text) {
				return CXFalse;
			}
			*((CXString *)ptr) = text;
			return CXTrue;
		}
		case CXDoubleAttributeType:
			if (value->type == GMLValueInteger) {
				*((double *)ptr) = (double)value->as.integerValue;
			} else if (value->type == GMLValueReal) {
				*((double *)ptr) = value->as.realValue;
			} else {
				return CXFalse;
			}
			return CXTrue;
		case CXIntegerAttributeType:
			if (value->type != GMLValueInteger) {
				return CXFalse;
			}
			*((int32_t *)ptr) = (int32_t)value->as.integerValue;
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
			if (value->type != GMLValueInteger) {
				return CXFalse;
			}
			*((uint32_t *)ptr) = (uint32_t)CXMAX((int64_t)0, value->as.integerValue);
			return CXTrue;
		case CXBigIntegerAttributeType:
			if (value->type != GMLValueInteger) {
				return CXFalse;
			}
			*((int64_t *)ptr) = value->as.integerValue;
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			if (value->type != GMLValueInteger) {
				return CXFalse;
			}
			*((uint64_t *)ptr) = (uint64_t)CXMAX((int64_t)0, value->as.integerValue);
			return CXTrue;
		default:
			return CXFalse;
	}
}

static CXAttributeRef GMLGetAttributeForScope(CXNetworkRef network, CXAttributeScope scope, const char *name) {
	if (!network || !name) {
		return NULL;
	}
	if (scope == CXAttributeScopeNode) {
		return CXNetworkGetNodeAttribute(network, name);
	}
	if (scope == CXAttributeScopeEdge) {
		return CXNetworkGetEdgeAttribute(network, name);
	}
	return CXNetworkGetNetworkAttribute(network, name);
}

static CXBool GMLAssignAttributesFromList(CXNetworkRef network, CXAttributeScope scope, CXIndex index, const GMLList *list) {
	if (!network || !list) {
		return CXTrue;
	}
	for (size_t i = 0; i < list->count; i += 1) {
		CXAttributeRef attr = GMLGetAttributeForScope(network, scope, list->items[i].key);
		if (!attr) {
			continue;
		}
		if (list->items[i].value->type == GMLValueList) {
			continue;
		}
		if (!GMLAssignAttributeValue(attr, index, list->items[i].value)) {
			return CXFalse;
		}
	}
	return CXTrue;
}

struct CXNetwork* CXNetworkReadGML(const char *path) {
	InterchangeWarningClear();
	if (!path) {
		errno = EINVAL;
		return NULL;
	}
	FILE *file = fopen(path, "rb");
	if (!file) {
		return NULL;
	}
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	if (size < 0) {
		fclose(file);
		return NULL;
	}
	fseek(file, 0, SEEK_SET);
	char *buffer = calloc((size_t)size + 1u, sizeof(char));
	if (!buffer) {
		fclose(file);
		return NULL;
	}
	if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
		free(buffer);
		fclose(file);
		return NULL;
	}
	fclose(file);

	GMLLexer lexer = {
		.text = buffer,
		.length = (size_t)size,
		.offset = 0,
		.line = 1,
		.column = 1,
		.current = {0},
		.error = NULL,
	};
	GMLValue *rootValue = NULL;
	GMLParsedGraph parsed = {0};
	CXNetworkRef network = NULL;
	CXStringDictionaryRef nodeIdMap = NULL;
	CXIndex *nodeIndices = NULL;
	CXEdge *edges = NULL;
	CXIndex *edgeIndices = NULL;

	if (!GMLLexerNext(&lexer)) {
		goto cleanup;
	}
	if ((lexer.current.type == GMLTokenIdentifier || lexer.current.type == GMLTokenString) &&
	    lexer.current.text && strcmp(lexer.current.text, "graph") == 0) {
		if (!GMLLexerNext(&lexer) || lexer.current.type != GMLTokenLBracket) {
			GMLLexerSetError(&lexer, "expected '[' after graph");
			goto cleanup;
		}
		rootValue = GMLParseListValue(&lexer);
	} else if (lexer.current.type == GMLTokenLBracket) {
		rootValue = GMLParseListValue(&lexer);
	} else {
		GMLLexerSetError(&lexer, "expected graph [ ... ]");
		goto cleanup;
	}
	if (!rootValue || rootValue->type != GMLValueList) {
		goto cleanup;
	}
	if (!GMLExtractParsedGraph(&rootValue->as.listValue, &parsed)) {
		goto cleanup;
	}
	if (!GMLGraphEnsureEdgeEndpoints(&parsed)) {
		goto cleanup;
	}
	if (parsed.nodes.count == 0) {
		network = CXNewNetwork(parsed.directed);
		goto cleanup_success;
	}
	network = CXNewNetworkWithCapacity(parsed.directed, parsed.nodes.count, parsed.edges.count > 0 ? parsed.edges.count : 1);
	if (!network) {
		goto cleanup;
	}
	nodeIndices = calloc(parsed.nodes.count, sizeof(CXIndex));
	if (!nodeIndices || !CXNetworkAddNodes(network, (CXSize)parsed.nodes.count, nodeIndices)) {
		goto cleanup;
	}
	nodeIdMap = CXNewStringDictionary();
	if (!nodeIdMap) {
		goto cleanup;
	}
	for (size_t i = 0; i < parsed.nodes.count; i += 1) {
		if (CXStringDictionaryEntryForKey(nodeIdMap, parsed.nodes.items[i].idText)) {
			InterchangeWarningAppend("GML duplicated node id \"%s\"; keeping the first occurrence", parsed.nodes.items[i].idText);
			continue;
		}
		CXStringDictionarySetEntry(nodeIdMap, parsed.nodes.items[i].idText, (void *)(uintptr_t)(nodeIndices[i] + 1u));
	}

	if (!CXNetworkDefineNodeAttribute(network, "_original_ids_", CXStringAttributeType, 1)) {
		goto cleanup;
	}
	CXString *originalIds = (CXString *)CXNetworkGetNodeAttributeBuffer(network, "_original_ids_");
	if (!originalIds) {
		goto cleanup;
	}
	for (size_t i = 0; i < parsed.nodes.count; i += 1) {
		originalIds[nodeIndices[i]] = CXNewStringFromString(parsed.nodes.items[i].idText);
		if (!originalIds[nodeIndices[i]]) {
			goto cleanup;
		}
	}

	CXStringDictionaryRef graphInfer = CXNewStringDictionary();
	CXStringDictionaryRef nodeInfer = CXNewStringDictionary();
	CXStringDictionaryRef edgeInfer = CXNewStringDictionary();
	if (!graphInfer || !nodeInfer || !edgeInfer) {
		GMLInferDictionaryDestroy(graphInfer);
		GMLInferDictionaryDestroy(nodeInfer);
		GMLInferDictionaryDestroy(edgeInfer);
		goto cleanup;
	}
	if (!GMLCollectInferredAttributes(&parsed.graphAttributes, graphInfer)) {
		GMLInferDictionaryDestroy(graphInfer);
		GMLInferDictionaryDestroy(nodeInfer);
		GMLInferDictionaryDestroy(edgeInfer);
		goto cleanup;
	}
	for (size_t i = 0; i < parsed.nodes.count; i += 1) {
		if (!GMLCollectInferredAttributes(&parsed.nodes.items[i].attributes, nodeInfer)) {
			GMLInferDictionaryDestroy(graphInfer);
			GMLInferDictionaryDestroy(nodeInfer);
			GMLInferDictionaryDestroy(edgeInfer);
			goto cleanup;
		}
	}
	for (size_t i = 0; i < parsed.edges.count; i += 1) {
		if (!GMLCollectInferredAttributes(&parsed.edges.items[i].attributes, edgeInfer)) {
			GMLInferDictionaryDestroy(graphInfer);
			GMLInferDictionaryDestroy(nodeInfer);
			GMLInferDictionaryDestroy(edgeInfer);
			goto cleanup;
		}
	}
	if (!GMLDefineAttributesFromInferMap(network, CXAttributeScopeNetwork, graphInfer) ||
	    !GMLDefineAttributesFromInferMap(network, CXAttributeScopeNode, nodeInfer) ||
	    !GMLDefineAttributesFromInferMap(network, CXAttributeScopeEdge, edgeInfer)) {
		GMLInferDictionaryDestroy(graphInfer);
		GMLInferDictionaryDestroy(nodeInfer);
		GMLInferDictionaryDestroy(edgeInfer);
		goto cleanup;
	}
	GMLInferDictionaryDestroy(graphInfer);
	GMLInferDictionaryDestroy(nodeInfer);
	GMLInferDictionaryDestroy(edgeInfer);

	if (!GMLAssignAttributesFromList(network, CXAttributeScopeNetwork, 0, &parsed.graphAttributes)) {
		goto cleanup;
	}
	for (size_t i = 0; i < parsed.nodes.count; i += 1) {
		if (!GMLAssignAttributesFromList(network, CXAttributeScopeNode, nodeIndices[i], &parsed.nodes.items[i].attributes)) {
			goto cleanup;
		}
	}

	if (parsed.edges.count > 0) {
		edges = calloc(parsed.edges.count, sizeof(CXEdge));
		edgeIndices = calloc(parsed.edges.count, sizeof(CXIndex));
		if (!edges || !edgeIndices) {
			goto cleanup;
		}
		for (size_t i = 0; i < parsed.edges.count; i += 1) {
			void *fromEntry = CXStringDictionaryEntryForKey(nodeIdMap, parsed.edges.items[i].sourceText);
			void *toEntry = CXStringDictionaryEntryForKey(nodeIdMap, parsed.edges.items[i].targetText);
			if (!fromEntry || !toEntry) {
				goto cleanup;
			}
			edges[i].from = (CXIndex)((uintptr_t)fromEntry - 1u);
			edges[i].to = (CXIndex)((uintptr_t)toEntry - 1u);
		}
		if (!CXNetworkAddEdges(network, edges, (CXSize)parsed.edges.count, edgeIndices)) {
			goto cleanup;
		}
		for (size_t i = 0; i < parsed.edges.count; i += 1) {
			if (!GMLAssignAttributesFromList(network, CXAttributeScopeEdge, edgeIndices[i], &parsed.edges.items[i].attributes)) {
				goto cleanup;
			}
		}
	}

cleanup_success:
	GMLParsedGraphFree(&parsed);
	if (nodeIdMap) {
		CXStringDictionaryDestroy(nodeIdMap);
	}
	free(nodeIndices);
	free(edges);
	free(edgeIndices);
	GMLValueFree(rootValue);
	free(rootValue);
	GMLTokenDispose(&lexer.current);
	free(lexer.error);
	free(buffer);
	return network;

cleanup:
	if (lexer.error) {
		InterchangeWarningAppend("%s", lexer.error);
	}
	if (network) {
		CXFreeNetwork(network);
		network = NULL;
	}
	GMLParsedGraphFree(&parsed);
	if (nodeIdMap) {
		CXStringDictionaryDestroy(nodeIdMap);
	}
	free(nodeIndices);
	free(edges);
	free(edgeIndices);
	GMLValueFree(rootValue);
	free(rootValue);
	GMLTokenDispose(&lexer.current);
	free(lexer.error);
	free(buffer);
	return NULL;
}
