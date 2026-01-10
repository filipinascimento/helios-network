#include "CXNetworkXNet.h"

#include "CXNetwork.h"
#include "CXDictionary.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XNET_VERSION_STRING "1.0.0"
#define XNET_HEADER_LINE "#XNET " XNET_VERSION_STRING

typedef enum {
	XNetBaseFloat,
	XNetBaseInt32,
	XNetBaseUInt32,
	XNetBaseInt64,
	XNetBaseUInt64,
	XNetBaseString
} XNetBaseType;

typedef enum {
	XNetScopeNode,
	XNetScopeEdge,
	XNetScopeGraph
} XNetAttributeScope;

typedef struct {
	char *text;
	size_t line;
	CXBool valid;
} XNetPendingLine;

typedef struct {
	char *name;
	XNetBaseType base;
	CXSize dimension;
	CXSize count;
	union {
		float *asFloat;
		int32_t *asInt32;
		uint32_t *asUInt32;
		int64_t *asInt64;
		uint64_t *asUInt64;
		char **asString;
	} values;
} XNetAttributeBlock;

typedef struct {
	XNetAttributeBlock *items;
	size_t count;
	size_t capacity;
} XNetAttributeList;

typedef struct {
	CXEdge *items;
	size_t count;
	size_t capacity;
} XNetEdgeList;

typedef struct {
	float *items;
	size_t count;
	size_t capacity;
} XNetFloatList;

typedef struct {
	char *message;
} XNetError;

typedef struct {
	FILE *file;
	size_t line;
	XNetPendingLine pending;
	CXBool legacy;
	CXBool headerSeen;
	CXBool verticesSeen;
	CXBool edgesSeen;
	CXBool directed;
	CXSize vertexCount;
	XNetEdgeList edges;
	XNetFloatList legacyWeights;
	XNetAttributeList vertexAttributes;
	XNetAttributeList edgeAttributes;
	XNetAttributeList graphAttributes;
	XNetAttributeBlock legacyLabels;
	CXBool hasLegacyLabels;
} XNetParser;

static void XNetErrorSet(XNetError *error, size_t line, const char *fmt, ...) {
	if (!error || error->message) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	if (line > 0) {
		char *prefix = CXNewStringFromFormat("Line %zu: ", line);
		char *body = NULL;
		vasprintf(&body, fmt, args);
		if (prefix && body) {
			error->message = CXNewStringFromFormat("%s%s", prefix, body);
		} else if (body) {
			error->message = body;
			body = NULL;
		}
		free(prefix);
		free(body);
	} else {
		vasprintf(&error->message, fmt, args);
	}
	va_end(args);
}

static void XNetAttributeListInit(XNetAttributeList *list) {
	if (!list) {
		return;
	}
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void XNetFloatListInit(XNetFloatList *list) {
	if (!list) {
		return;
	}
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void XNetEdgeListInit(XNetEdgeList *list) {
	if (!list) {
		return;
	}
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void XNetAttributeBlockReset(XNetAttributeBlock *block) {
	if (!block) {
		return;
	}
	block->name = NULL;
	block->base = XNetBaseFloat;
	block->dimension = 1;
	block->count = 0;
	memset(&block->values, 0, sizeof(block->values));
}

static void XNetAttributeBlockFree(XNetAttributeBlock *block) {
	if (!block) {
		return;
	}
	if (block->name) {
		free(block->name);
		block->name = NULL;
	}
	if (block->base == XNetBaseString) {
		if (block->values.asString) {
			for (CXSize i = 0; i < block->count; i++) {
				if (block->values.asString[i]) {
					free(block->values.asString[i]);
				}
			}
			free(block->values.asString);
		}
	} else if (block->base == XNetBaseFloat) {
		free(block->values.asFloat);
	} else if (block->base == XNetBaseInt32) {
		free(block->values.asInt32);
	} else if (block->base == XNetBaseUInt32) {
		free(block->values.asUInt32);
	} else if (block->base == XNetBaseInt64) {
		free(block->values.asInt64);
	} else if (block->base == XNetBaseUInt64) {
		free(block->values.asUInt64);
	}
	memset(&block->values, 0, sizeof(block->values));
	block->count = 0;
	block->dimension = 1;
}

static void XNetAttributeListFree(XNetAttributeList *list) {
	if (!list || !list->items) {
		return;
	}
	for (size_t i = 0; i < list->count; i++) {
		XNetAttributeBlockFree(&list->items[i]);
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void XNetEdgeListFree(XNetEdgeList *list) {
	if (!list) {
		return;
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void XNetFloatListFree(XNetFloatList *list) {
	if (!list) {
		return;
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static CXBool XNetAttributeListEnsureCapacity(XNetAttributeList *list, size_t required) {
	if (!list) {
		return CXFalse;
	}
	if (list->capacity >= required) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 4;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}
	XNetAttributeBlock *items = realloc(list->items, newCapacity * sizeof(XNetAttributeBlock));
	if (!items) {
		return CXFalse;
	}
	for (size_t i = list->capacity; i < newCapacity; i++) {
		XNetAttributeBlockReset(&items[i]);
	}
	list->items = items;
	list->capacity = newCapacity;
	return CXTrue;
}

static CXBool XNetEdgeListEnsureCapacity(XNetEdgeList *list, size_t required) {
	if (!list) {
		return CXFalse;
	}
	if (list->capacity >= required) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 8;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}
	CXEdge *items = realloc(list->items, newCapacity * sizeof(CXEdge));
	if (!items) {
		return CXFalse;
	}
	list->items = items;
	list->capacity = newCapacity;
	return CXTrue;
}

static CXBool XNetFloatListEnsureCapacity(XNetFloatList *list, size_t required) {
	if (!list) {
		return CXFalse;
	}
	if (list->capacity >= required) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 8;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}
	float *items = realloc(list->items, newCapacity * sizeof(float));
	if (!items) {
		return CXFalse;
	}
	list->items = items;
	list->capacity = newCapacity;
	return CXTrue;
}

static CXBool XNetAttributeListHasName(const XNetAttributeList *list, const char *name) {
	if (!list || !name) {
		return CXFalse;
	}
	for (size_t i = 0; i < list->count; i++) {
		if (list->items[i].name && strcmp(list->items[i].name, name) == 0) {
			return CXTrue;
		}
	}
	return CXFalse;
}

static char* XNetReadLine(FILE *file) {
	if (!file) {
		return NULL;
	}
	return CXNewStringReadingLine(file);
}

static char* XNetTrimTrailing(char *line) {
	if (!line) {
		return NULL;
	}
	size_t len = strlen(line);
	while (len > 0) {
		unsigned char ch = (unsigned char)line[len - 1];
		if (ch == '\n' || ch == '\r') {
			line[--len] = '\0';
			continue;
		}
		if (isspace(ch)) {
			line[--len] = '\0';
			continue;
		}
		break;
	}
	return line;
}

static char* XNetSkipWhitespace(char *line) {
	if (!line) {
		return NULL;
	}
	while (*line && isspace((unsigned char)*line)) {
		line++;
	}
	return line;
}

static CXBool XNetIsComment(const char *line) {
	if (!line) {
		return CXFalse;
	}
	return line[0] == '#' && line[1] == '#';
}

static CXBool XNetIsBlank(const char *line) {
	if (!line) {
		return CXTrue;
	}
	for (const char *ptr = line; *ptr; ptr++) {
		if (!isspace((unsigned char)*ptr)) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool XNetGetLine(XNetParser *parser, char **outLine, size_t *outLineNumber) {
	if (!parser || !outLine || !outLineNumber) {
		return CXFalse;
	}
	if (parser->pending.valid) {
		*outLine = parser->pending.text;
		*outLineNumber = parser->pending.line;
		parser->pending.valid = CXFalse;
		parser->pending.text = NULL;
		parser->pending.line = 0;
		return CXTrue;
	}
	char *line = XNetReadLine(parser->file);
	if (!line) {
		return CXFalse;
	}
	parser->line++;
	*outLine = line;
	*outLineNumber = parser->line;
	return CXTrue;
}

static void XNetUnreadLine(XNetParser *parser, char *line, size_t lineNumber) {
	if (!parser || !line) {
		return;
	}
	if (parser->pending.valid) {
		free(parser->pending.text);
		parser->pending.text = NULL;
	}
	parser->pending.valid = CXTrue;
	parser->pending.text = line;
	parser->pending.line = lineNumber;
}

static CXBool XNetAllocateAttributeValues(XNetAttributeBlock *block) {
	if (!block) {
		return CXFalse;
	}
	size_t total = (size_t)block->count * (size_t)block->dimension;
	if (block->base == XNetBaseString) {
		block->values.asString = calloc(block->count, sizeof(char *));
		return block->values.asString != NULL;
	}
	if (block->base == XNetBaseFloat) {
		block->values.asFloat = calloc(total, sizeof(float));
		return block->values.asFloat != NULL;
	}
	if (block->base == XNetBaseInt32) {
		block->values.asInt32 = calloc(total, sizeof(int32_t));
		return block->values.asInt32 != NULL;
	}
	if (block->base == XNetBaseUInt32) {
		block->values.asUInt32 = calloc(total, sizeof(uint32_t));
		return block->values.asUInt32 != NULL;
	}
	if (block->base == XNetBaseInt64) {
		block->values.asInt64 = calloc(total, sizeof(int64_t));
		return block->values.asInt64 != NULL;
	}
	if (block->base == XNetBaseUInt64) {
		block->values.asUInt64 = calloc(total, sizeof(uint64_t));
		return block->values.asUInt64 != NULL;
	}
	return CXFalse;
}

static CXBool XNetDecodeString(const char *input, CXBool quoted, char **outValue, XNetError *error, size_t lineNumber) {
	if (!outValue) {
		return CXFalse;
	}
	if (!input) {
		*outValue = NULL;
		return CXTrue;
	}
	size_t len = strlen(input);
	char *result = malloc(len + 1);
	if (!result) {
		XNetErrorSet(error, lineNumber, "Out of memory while decoding string");
		return CXFalse;
	}
	size_t write = 0;
	for (size_t i = 0; i < len; i++) {
		char ch = input[i];
		if (quoted && ch == '\\') {
			if (i + 1 >= len) {
				free(result);
				XNetErrorSet(error, lineNumber, "Invalid escape sequence at end of string");
				return CXFalse;
			}
			char next = input[++i];
			switch (next) {
				case 'n': result[write++] = '\n'; break;
				case 't': result[write++] = '\t'; break;
				case 'r': result[write++] = '\r'; break;
				case '\\': result[write++] = '\\'; break;
				case '"': result[write++] = '"'; break;
				default:
					free(result);
					XNetErrorSet(error, lineNumber, "Unsupported escape sequence \\%c", next);
					return CXFalse;
			}
		} else {
			result[write++] = ch;
		}
	}
	result[write] = '\0';
	*outValue = result;
	return CXTrue;
}

static CXBool XNetParseStringValue(const char *line, CXBool legacy, char **out, XNetError *error, size_t lineNumber) {
	if (!line || !out) {
		return CXFalse;
	}
	const char *trimmed = line;
	while (isspace((unsigned char)*trimmed)) {
		trimmed++;
	}
	size_t len = strlen(trimmed);
	while (len > 0 && isspace((unsigned char)trimmed[len - 1])) {
		len--;
	}
	if (len == 0) {
		*out = CXNewStringFromString("");
		return CXTrue;
	}
	if (trimmed[0] == '"') {
		if (len < 2 || trimmed[len - 1] != '"') {
			XNetErrorSet(error, lineNumber, "Unterminated quoted string");
			return CXFalse;
		}
		char *payload = malloc(len - 1);
		if (!payload) {
			XNetErrorSet(error, lineNumber, "Out of memory decoding string");
			return CXFalse;
		}
		memcpy(payload, trimmed + 1, len - 2);
		payload[len - 2] = '\0';
		char *decoded = NULL;
		CXBool ok = CXFalse;
		if (legacy) {
			ok = XNetDecodeString(payload, CXTrue, &decoded, NULL, lineNumber);
			if (!ok) {
				*out = CXNewStringFromString(payload);
				free(payload);
				return *out != NULL;
			}
		} else {
			ok = XNetDecodeString(payload, CXTrue, &decoded, error, lineNumber);
		}
		free(payload);
		if (!ok) {
			return CXFalse;
		}
		*out = decoded;
		return CXTrue;
	}
	if (trimmed[0] == '#') {
		XNetErrorSet(error, lineNumber, "Unquoted string values may not start with '#'");
		return CXFalse;
	}
	char *payload = malloc(len + 1);
	if (!payload) {
		XNetErrorSet(error, lineNumber, "Out of memory decoding string");
		return CXFalse;
	}
	memcpy(payload, trimmed, len);
	payload[len] = '\0';
	char *decoded = NULL;
	CXBool ok = XNetDecodeString(payload, CXFalse, &decoded, error, lineNumber);
	free(payload);
	if (!ok) {
		return CXFalse;
	}
	*out = decoded;
	return CXTrue;
}

static CXBool XNetParseInteger(const char *text, CXBool unsignedMode, int base, int64_t *outSigned, uint64_t *outUnsigned) {
	if (!text) {
		return CXFalse;
	}
	errno = 0;
	char *end = NULL;
	if (unsignedMode) {
		unsigned long long value = strtoull(text, &end, base);
		if (errno || end == text) {
			return CXFalse;
		}
		while (*end && isspace((unsigned char)*end)) {
			end++;
		}
		if (*end) {
			return CXFalse;
		}
		if (outUnsigned) {
			*outUnsigned = (uint64_t)value;
		}
		return CXTrue;
	}
	long long value = strtoll(text, &end, base);
	if (errno || end == text) {
		return CXFalse;
	}
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end) {
		return CXFalse;
	}
	if (outSigned) {
		*outSigned = (int64_t)value;
	}
	return CXTrue;
}

static CXBool XNetParseFloatLine(const char *line, CXSize dimension, float *dest, XNetError *error, size_t lineNumber) {
	if (!line || !dest || dimension == 0) {
		return CXFalse;
	}
	const char *cursor = line;
	for (CXSize i = 0; i < dimension; i++) {
		while (*cursor && isspace((unsigned char)*cursor)) {
			cursor++;
		}
		if (*cursor == '\0') {
			XNetErrorSet(error, lineNumber, "Expected %zu float values, found %zu", (size_t)dimension, (size_t)i);
			return CXFalse;
		}
		errno = 0;
		char *end = NULL;
		double value = strtod(cursor, &end);
		if (errno || end == cursor) {
			XNetErrorSet(error, lineNumber, "Invalid float value");
			return CXFalse;
		}
		dest[i] = (float)value;
		cursor = end;
	}
	while (*cursor && isspace((unsigned char)*cursor)) {
		cursor++;
	}
	if (*cursor != '\0') {
		XNetErrorSet(error, lineNumber, "Unexpected trailing characters in float vector");
		return CXFalse;
	}
	return CXTrue;
}

static CXBool XNetParseIntLine(const char *line, CXSize dimension, CXBool unsignedMode, int bits, void *dest, XNetError *error, size_t lineNumber) {
	if (!line || !dest || dimension == 0) {
		return CXFalse;
	}
	const char *cursor = line;
	for (CXSize i = 0; i < dimension; i++) {
		while (*cursor && isspace((unsigned char)*cursor)) {
			cursor++;
		}
		if (*cursor == '\0') {
			XNetErrorSet(error, lineNumber, "Expected %zu integer values, found %zu", (size_t)dimension, (size_t)i);
			return CXFalse;
		}
		char *end = NULL;
		errno = 0;
		if (unsignedMode) {
			unsigned long long value = strtoull(cursor, &end, 10);
			if (errno || end == cursor) {
				XNetErrorSet(error, lineNumber, "Invalid unsigned integer value");
				return CXFalse;
			}
			if (bits == 32) {
				if (value > UINT32_MAX) {
					XNetErrorSet(error, lineNumber, "Unsigned integer value out of range");
					return CXFalse;
				}
				((uint32_t *)dest)[i] = (uint32_t)value;
			} else {
				if ((uint64_t)value > UINT64_MAX) {
					XNetErrorSet(error, lineNumber, "Unsigned integer value out of range");
					return CXFalse;
				}
				((uint64_t *)dest)[i] = (uint64_t)value;
			}
		} else {
			long long value = strtoll(cursor, &end, 10);
			if (errno || end == cursor) {
				XNetErrorSet(error, lineNumber, "Invalid integer value");
				return CXFalse;
			}
			if (bits == 32) {
				if (value < INT32_MIN || value > INT32_MAX) {
					XNetErrorSet(error, lineNumber, "Integer value out of range");
					return CXFalse;
				}
				((int32_t *)dest)[i] = (int32_t)value;
			} else {
				if (value < LLONG_MIN || value > LLONG_MAX) {
					XNetErrorSet(error, lineNumber, "Integer value out of range");
					return CXFalse;
				}
				((int64_t *)dest)[i] = (int64_t)value;
			}
		}
		cursor = end;
	}
	while (*cursor && isspace((unsigned char)*cursor)) {
		cursor++;
	}
	if (*cursor != '\0') {
		XNetErrorSet(error, lineNumber, "Unexpected trailing characters in integer vector");
		return CXFalse;
	}
	return CXTrue;
}

static CXBool XNetParseTypeToken(const char *token, CXBool legacy, XNetBaseType *outBase, CXSize *outDimension, XNetError *error, size_t lineNumber) {
	if (!token || !outBase || !outDimension) {
		return CXFalse;
	}
	if (legacy) {
		if (strcmp(token, "s") == 0) {
			*outBase = XNetBaseString;
			*outDimension = 1;
			return CXTrue;
		}
		if (strcmp(token, "n") == 0) {
			*outBase = XNetBaseFloat;
			*outDimension = 1;
			return CXTrue;
		}
		if (strcmp(token, "v2") == 0) {
			*outBase = XNetBaseFloat;
			*outDimension = 2;
			return CXTrue;
		}
		if (strcmp(token, "v3") == 0) {
			*outBase = XNetBaseFloat;
			*outDimension = 3;
			return CXTrue;
		}
		XNetErrorSet(error, lineNumber, "Unsupported legacy type '%s'", token);
		return CXFalse;
	}
	if (strlen(token) == 0) {
		XNetErrorSet(error, lineNumber, "Missing type token");
		return CXFalse;
	}
	char kind = token[0];
	if (kind == 's') {
		if (token[1] != '\0') {
			XNetErrorSet(error, lineNumber, "Strings cannot be vectorized");
			return CXFalse;
		}
		*outBase = XNetBaseString;
		*outDimension = 1;
		return CXTrue;
	}
	if (kind != 'f' && kind != 'i' && kind != 'u' && kind != 'I' && kind != 'U') {
		XNetErrorSet(error, lineNumber, "Unsupported type '%s'", token);
		return CXFalse;
	}
	if (token[1] == '\0') {
		*outDimension = 1;
	} else {
		long dim = strtol(token + 1, NULL, 10);
		if (dim < 2) {
			XNetErrorSet(error, lineNumber, "Vector dimension must be >= 2");
			return CXFalse;
		}
		*outDimension = (CXSize)dim;
	}
	if (kind == 'f') {
		*outBase = XNetBaseFloat;
	} else if (kind == 'i') {
		*outBase = XNetBaseInt32;
	} else {
		*outBase = (kind == 'u') ? XNetBaseUInt32 : (kind == 'I' ? XNetBaseInt64 : XNetBaseUInt64);
	}
	return CXTrue;
}

static CXBool XNetParseQuotedName(const char *line, char **out, XNetError *error, size_t lineNumber) {
	if (!line || !out) {
		return CXFalse;
	}
	const char *start = strchr(line, '"');
	if (!start) {
		XNetErrorSet(error, lineNumber, "Expected quoted name");
		return CXFalse;
	}
	start++;
	const char *end = strchr(start, '"');
	if (!end) {
		XNetErrorSet(error, lineNumber, "Unterminated quoted name");
		return CXFalse;
	}
	size_t len = (size_t)(end - start);
	char *buffer = malloc(len + 1);
	if (!buffer) {
		XNetErrorSet(error, lineNumber, "Out of memory while reading name");
		return CXFalse;
	}
	memcpy(buffer, start, len);
	buffer[len] = '\0';
	char *decoded = NULL;
	if (!XNetDecodeString(buffer, CXTrue, &decoded, error, lineNumber)) {
		free(buffer);
		return CXFalse;
	}
	free(buffer);
	*out = decoded;
	return CXTrue;
}

static CXBool XNetParseVertices(XNetParser *parser, const char *directiveLine, CXBool legacy, XNetError *error, size_t lineNumber) {
	if (!parser || !directiveLine) {
		return CXFalse;
	}
	if (parser->verticesSeen) {
		XNetErrorSet(error, lineNumber, "Duplicate #vertices section");
		return CXFalse;
	}
	const char *cursor = directiveLine + strlen("#vertices");
	while (*cursor && isspace((unsigned char)*cursor)) {
		cursor++;
	}
	if (*cursor == '\0') {
		XNetErrorSet(error, lineNumber, "Missing vertex count");
		return CXFalse;
	}
	char *end = NULL;
	errno = 0;
	long long count = strtoll(cursor, &end, 10);
	if (errno || end == cursor || count < 0) {
		XNetErrorSet(error, lineNumber, "Invalid vertex count");
		return CXFalse;
	}
	while (*end && isspace((unsigned char)*end)) {
		end++;
	}
	if (*end != '\0') {
		if (!legacy) {
			XNetErrorSet(error, lineNumber, "Unexpected trailing characters in #vertices directive");
			return CXFalse;
		}
		while (*end) {
			while (*end && isspace((unsigned char)*end)) {
				end++;
			}
			if (*end == '\0') {
				break;
			}
			const char *tokenStart = end;
			while (*end && !isspace((unsigned char)*end)) {
				end++;
			}
			size_t tokenLen = (size_t)(end - tokenStart);
			char token[32];
			if (tokenLen == 0) {
				continue;
			}
			if (tokenLen >= sizeof(token)) {
				XNetErrorSet(error, lineNumber, "Invalid token in #vertices directive");
				return CXFalse;
			}
			memcpy(token, tokenStart, tokenLen);
			token[tokenLen] = '\0';
			if (strcmp(token, "weighted") == 0 ||
			    strcmp(token, "nonweighted") == 0 ||
			    strcmp(token, "directed") == 0 ||
			    strcmp(token, "undirected") == 0) {
				continue;
			}
			XNetErrorSet(error, lineNumber, "Unknown token '%s' in #vertices directive", token);
			return CXFalse;
		}
	}
	parser->vertexCount = (CXSize)count;
	parser->verticesSeen = CXTrue;
	return CXTrue;
}

static CXBool XNetParseEdgesDirective(XNetParser *parser, const char *line, CXBool legacy, CXBool *outWeighted, XNetError *error, size_t lineNumber) {
	if (!parser || !line) {
		return CXFalse;
	}
	if (!parser->verticesSeen) {
		XNetErrorSet(error, lineNumber, "#edges encountered before #vertices");
		return CXFalse;
	}
	if (parser->edgesSeen) {
		XNetErrorSet(error, lineNumber, "Duplicate #edges section");
		return CXFalse;
	}
	parser->edgesSeen = CXTrue;
	*outWeighted = CXFalse;

	const char *cursor = line + strlen("#edges");
	while (*cursor && isspace((unsigned char)*cursor)) {
		cursor++;
	}
	if (!legacy) {
		if (strncmp(cursor, "directed", 8) == 0 && (cursor[8] == '\0' || isspace((unsigned char)cursor[8]))) {
			parser->directed = CXTrue;
			cursor += 8;
		} else if (strncmp(cursor, "undirected", 10) == 0 && (cursor[10] == '\0' || isspace((unsigned char)cursor[10]))) {
			parser->directed = CXFalse;
			cursor += 10;
		} else {
			XNetErrorSet(error, lineNumber, "Expected 'directed' or 'undirected' after #edges");
			return CXFalse;
		}
		while (*cursor && isspace((unsigned char)*cursor)) {
			cursor++;
		}
		if (*cursor != '\0') {
			XNetErrorSet(error, lineNumber, "Unexpected trailing characters in #edges directive");
			return CXFalse;
		}
		return CXTrue;
	}

	// Legacy: tokens optional
	CXBool directedSet = CXFalse;
	CXBool weightedSet = CXFalse;
	while (*cursor) {
		while (*cursor && isspace((unsigned char)*cursor)) {
			cursor++;
		}
		if (*cursor == '\0') {
			break;
		}
		const char *tokenStart = cursor;
		while (*cursor && !isspace((unsigned char)*cursor)) {
			cursor++;
		}
		size_t len = (size_t)(cursor - tokenStart);
		char token[32];
		if (len >= sizeof(token)) {
			XNetErrorSet(error, lineNumber, "Invalid token in #edges directive");
			return CXFalse;
		}
		memcpy(token, tokenStart, len);
		token[len] = '\0';
		if (strcmp(token, "weighted") == 0) {
			*outWeighted = CXTrue;
			weightedSet = CXTrue;
		} else if (strcmp(token, "nonweighted") == 0) {
			*outWeighted = CXFalse;
			weightedSet = CXTrue;
		} else if (strcmp(token, "directed") == 0) {
			parser->directed = CXTrue;
			directedSet = CXTrue;
		} else if (strcmp(token, "undirected") == 0) {
			parser->directed = CXFalse;
			directedSet = CXTrue;
		} else {
			XNetErrorSet(error, lineNumber, "Unknown token '%s' in #edges directive", token);
			return CXFalse;
		}
	}
	if (!directedSet) {
		parser->directed = CXFalse;
	}
	if (!weightedSet) {
		*outWeighted = CXFalse;
	}
	return CXTrue;
}

static CXBool XNetConsumeLegacyLabels(XNetParser *parser, XNetError *error) {
	if (!parser || !parser->legacy || !parser->verticesSeen) {
		return CXTrue;
	}
	if (parser->vertexCount == 0) {
		return CXTrue;
	}
	parser->legacyLabels.base = XNetBaseString;
	parser->legacyLabels.dimension = 1;
	parser->legacyLabels.count = parser->vertexCount;
	if (!XNetAllocateAttributeValues(&parser->legacyLabels)) {
		XNetErrorSet(error, parser->line, "Failed to allocate legacy label storage");
		return CXFalse;
	}
	parser->legacyLabels.name = CXNewStringFromString("Label");
	if (!parser->legacyLabels.name) {
		XNetErrorSet(error, parser->line, "Failed to allocate legacy label name");
		return CXFalse;
	}

	for (CXSize idx = 0; idx < parser->vertexCount; idx++) {
		char *line = NULL;
		size_t lineNumber = 0;
		if (!XNetGetLine(parser, &line, &lineNumber)) {
			XNetErrorSet(error, parser->line, "Unexpected EOF while reading legacy labels");
			return CXFalse;
		}
		char *trimLeading = XNetSkipWhitespace(line);
		if (trimLeading[0] == '#') {
			if (idx == 0) {
				XNetUnreadLine(parser, line, lineNumber);
				parser->legacyLabels.count = 0;
				free(parser->legacyLabels.values.asString);
				parser->legacyLabels.values.asString = NULL;
				return CXTrue;
			}
			XNetErrorSet(error, lineNumber, "Legacy label block ended early");
			free(line);
			return CXFalse;
		}
		XNetTrimTrailing(line);
		char *value = NULL;
		if (!XNetParseStringValue(line, parser->legacy, &value, error, lineNumber)) {
			free(line);
			return CXFalse;
		}
		parser->legacyLabels.values.asString[idx] = value;
		free(line);
	}
	parser->hasLegacyLabels = CXTrue;
	return CXTrue;
}

static CXBool XNetParseVertexAttribute(XNetParser *parser, const char *line, XNetError *error, size_t lineNumber) {
	if (!parser || !line) {
		return CXFalse;
	}
	if (!parser->verticesSeen) {
		XNetErrorSet(error, lineNumber, "Vertex attribute encountered before #vertices");
		return CXFalse;
	}
	char *name = NULL;
	if (!XNetParseQuotedName(line, &name, error, lineNumber)) {
		return CXFalse;
	}
	if (XNetAttributeListHasName(&parser->vertexAttributes, name)) {
		XNetErrorSet(error, lineNumber, "Duplicate vertex attribute '%s'", name);
		free(name);
		return CXFalse;
	}
	const char *typeStart = strrchr(line, '"');
	if (!typeStart) {
		free(name);
		XNetErrorSet(error, lineNumber, "Malformed vertex attribute header");
		return CXFalse;
	}
	typeStart++;
	while (*typeStart && isspace((unsigned char)*typeStart)) {
		typeStart++;
	}
	if (*typeStart == '\0') {
		free(name);
		XNetErrorSet(error, lineNumber, "Missing type token in vertex attribute header");
		return CXFalse;
	}
	XNetBaseType base;
	CXSize dimension = 1;
	if (!XNetParseTypeToken(typeStart, parser->legacy, &base, &dimension, error, lineNumber)) {
		free(name);
		return CXFalse;
	}
	if (!XNetAttributeListEnsureCapacity(&parser->vertexAttributes, parser->vertexAttributes.count + 1)) {
		free(name);
		XNetErrorSet(error, lineNumber, "Failed to grow vertex attribute list");
		return CXFalse;
	}
	XNetAttributeBlock *block = &parser->vertexAttributes.items[parser->vertexAttributes.count];
	XNetAttributeBlockReset(block);
	block->name = name;
	block->base = base;
	block->dimension = dimension;
	block->count = parser->vertexCount;
	if (!XNetAllocateAttributeValues(block)) {
		XNetErrorSet(error, lineNumber, "Failed to allocate vertex attribute storage");
		return CXFalse;
	}

	for (CXSize idx = 0; idx < parser->vertexCount; idx++) {
		char *valueLine = NULL;
		size_t valueLineNumber = 0;
		if (!XNetGetLine(parser, &valueLine, &valueLineNumber)) {
			XNetErrorSet(error, lineNumber, "Unexpected EOF in vertex attribute '%s'", name);
			return CXFalse;
		}
		if (XNetIsComment(valueLine)) {
			XNetErrorSet(error, valueLineNumber, "Comments are not allowed inside attribute blocks");
			free(valueLine);
			return CXFalse;
		}
		if (XNetIsBlank(valueLine)) {
			XNetErrorSet(error, valueLineNumber, "Empty lines are not allowed inside attribute blocks");
			free(valueLine);
			return CXFalse;
		}

		CXBool ok = CXFalse;
		if (block->base == XNetBaseString) {
			char *decoded = NULL;
			ok = XNetParseStringValue(valueLine, parser->legacy, &decoded, error, valueLineNumber);
			if (ok) {
				block->values.asString[idx] = decoded;
			}
		} else if (block->base == XNetBaseFloat) {
			ok = XNetParseFloatLine(valueLine, block->dimension, block->values.asFloat + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseInt32) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXFalse, 32, block->values.asInt32 + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseUInt32) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXTrue, 32, block->values.asUInt32 + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseInt64) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXFalse, 64, block->values.asInt64 + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseUInt64) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXTrue, 64, block->values.asUInt64 + (size_t)idx * block->dimension, error, valueLineNumber);
		}
		free(valueLine);
		if (!ok) {
			return CXFalse;
		}
	}

	parser->vertexAttributes.count++;
	return CXTrue;
}

static CXBool XNetParseEdgeAttribute(XNetParser *parser, const char *line, XNetError *error, size_t lineNumber) {
	if (!parser || !line) {
		return CXFalse;
	}
	if (!parser->edgesSeen) {
		XNetErrorSet(error, lineNumber, "Edge attribute encountered before #edges");
		return CXFalse;
	}
	char *name = NULL;
	if (!XNetParseQuotedName(line, &name, error, lineNumber)) {
		return CXFalse;
	}
	if (XNetAttributeListHasName(&parser->edgeAttributes, name)) {
		XNetErrorSet(error, lineNumber, "Duplicate edge attribute '%s'", name);
		free(name);
		return CXFalse;
	}
	const char *typeStart = strrchr(line, '"');
	if (!typeStart) {
		free(name);
		XNetErrorSet(error, lineNumber, "Malformed edge attribute header");
		return CXFalse;
	}
	typeStart++;
	while (*typeStart && isspace((unsigned char)*typeStart)) {
		typeStart++;
	}
	if (*typeStart == '\0') {
		free(name);
		XNetErrorSet(error, lineNumber, "Missing type token in edge attribute header");
		return CXFalse;
	}
	XNetBaseType base;
	CXSize dimension = 1;
	if (!XNetParseTypeToken(typeStart, parser->legacy, &base, &dimension, error, lineNumber)) {
		free(name);
		return CXFalse;
	}
	if (!XNetAttributeListEnsureCapacity(&parser->edgeAttributes, parser->edgeAttributes.count + 1)) {
		free(name);
		XNetErrorSet(error, lineNumber, "Failed to grow edge attribute list");
		return CXFalse;
	}
	XNetAttributeBlock *block = &parser->edgeAttributes.items[parser->edgeAttributes.count];
	XNetAttributeBlockReset(block);
	block->name = name;
	block->base = base;
	block->dimension = dimension;
	block->count = parser->edges.count;
	if (!XNetAllocateAttributeValues(block)) {
		XNetErrorSet(error, lineNumber, "Failed to allocate edge attribute storage");
		return CXFalse;
	}

	for (CXSize idx = 0; idx < parser->edges.count; idx++) {
		char *valueLine = NULL;
		size_t valueLineNumber = 0;
		if (!XNetGetLine(parser, &valueLine, &valueLineNumber)) {
			XNetErrorSet(error, lineNumber, "Unexpected EOF in edge attribute '%s'", name);
			return CXFalse;
		}
		if (XNetIsComment(valueLine)) {
			XNetErrorSet(error, valueLineNumber, "Comments are not allowed inside attribute blocks");
			free(valueLine);
			return CXFalse;
		}
		if (XNetIsBlank(valueLine)) {
			XNetErrorSet(error, valueLineNumber, "Empty lines are not allowed inside attribute blocks");
			free(valueLine);
			return CXFalse;
		}

		CXBool ok = CXFalse;
		if (block->base == XNetBaseString) {
			char *decoded = NULL;
			ok = XNetParseStringValue(valueLine, parser->legacy, &decoded, error, valueLineNumber);
			if (ok) {
				block->values.asString[idx] = decoded;
			}
		} else if (block->base == XNetBaseFloat) {
			ok = XNetParseFloatLine(valueLine, block->dimension, block->values.asFloat + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseInt32) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXFalse, 32, block->values.asInt32 + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseUInt32) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXTrue, 32, block->values.asUInt32 + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseInt64) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXFalse, 64, block->values.asInt64 + (size_t)idx * block->dimension, error, valueLineNumber);
		} else if (block->base == XNetBaseUInt64) {
			ok = XNetParseIntLine(valueLine, block->dimension, CXTrue, 64, block->values.asUInt64 + (size_t)idx * block->dimension, error, valueLineNumber);
		}
		free(valueLine);
		if (!ok) {
			return CXFalse;
		}
	}

	parser->edgeAttributes.count++;
	return CXTrue;
}

static CXBool XNetParseGraphAttribute(XNetParser *parser, const char *line, XNetError *error, size_t lineNumber) {
	if (!parser || !line) {
		return CXFalse;
	}
	if (parser->legacy) {
		XNetErrorSet(error, lineNumber, "Graph attributes are not supported in legacy XNET files");
		return CXFalse;
	}
	char *name = NULL;
	if (!XNetParseQuotedName(line, &name, error, lineNumber)) {
		return CXFalse;
	}
	if (XNetAttributeListHasName(&parser->graphAttributes, name)) {
		XNetErrorSet(error, lineNumber, "Duplicate graph attribute '%s'", name);
		free(name);
		return CXFalse;
	}
	const char *typeStart = strrchr(line, '"');
	if (!typeStart) {
		free(name);
		XNetErrorSet(error, lineNumber, "Malformed graph attribute header");
		return CXFalse;
	}
	typeStart++;
	while (*typeStart && isspace((unsigned char)*typeStart)) {
		typeStart++;
	}
	if (*typeStart == '\0') {
		free(name);
		XNetErrorSet(error, lineNumber, "Missing type token in graph attribute header");
		return CXFalse;
	}
	XNetBaseType base;
	CXSize dimension = 1;
	if (!XNetParseTypeToken(typeStart, parser->legacy, &base, &dimension, error, lineNumber)) {
		free(name);
		return CXFalse;
	}
	if (!XNetAttributeListEnsureCapacity(&parser->graphAttributes, parser->graphAttributes.count + 1)) {
		free(name);
		XNetErrorSet(error, lineNumber, "Failed to grow graph attribute list");
		return CXFalse;
	}
	XNetAttributeBlock *block = &parser->graphAttributes.items[parser->graphAttributes.count];
	XNetAttributeBlockReset(block);
	block->name = name;
	block->base = base;
	block->dimension = dimension;
	block->count = 1;
	if (!XNetAllocateAttributeValues(block)) {
		XNetErrorSet(error, lineNumber, "Failed to allocate graph attribute storage");
		return CXFalse;
	}

	char *valueLine = NULL;
	size_t valueLineNumber = 0;
	if (!XNetGetLine(parser, &valueLine, &valueLineNumber)) {
		XNetErrorSet(error, lineNumber, "Unexpected EOF reading graph attribute '%s'", name);
		return CXFalse;
	}
	if (XNetIsComment(valueLine)) {
		XNetErrorSet(error, valueLineNumber, "Comments are not allowed inside attribute blocks");
		free(valueLine);
		return CXFalse;
	}
	if (XNetIsBlank(valueLine)) {
		XNetErrorSet(error, valueLineNumber, "Empty line encountered in graph attribute");
		free(valueLine);
		return CXFalse;
	}

	CXBool ok = CXFalse;
	if (block->base == XNetBaseString) {
		char *decoded = NULL;
		ok = XNetParseStringValue(valueLine, CXFalse, &decoded, error, valueLineNumber);
		if (ok) {
			block->values.asString[0] = decoded;
		}
	} else if (block->base == XNetBaseFloat) {
		ok = XNetParseFloatLine(valueLine, block->dimension, block->values.asFloat, error, valueLineNumber);
	} else if (block->base == XNetBaseInt32) {
		ok = XNetParseIntLine(valueLine, block->dimension, CXFalse, 32, block->values.asInt32, error, valueLineNumber);
	} else if (block->base == XNetBaseUInt32) {
		ok = XNetParseIntLine(valueLine, block->dimension, CXTrue, 32, block->values.asUInt32, error, valueLineNumber);
	} else if (block->base == XNetBaseInt64) {
		ok = XNetParseIntLine(valueLine, block->dimension, CXFalse, 64, block->values.asInt64, error, valueLineNumber);
	} else if (block->base == XNetBaseUInt64) {
		ok = XNetParseIntLine(valueLine, block->dimension, CXTrue, 64, block->values.asUInt64, error, valueLineNumber);
	}
	free(valueLine);
	if (!ok) {
		return CXFalse;
	}

	parser->graphAttributes.count++;
	return CXTrue;
}

static CXBool XNetParseEdges(XNetParser *parser, CXBool weighted, XNetError *error) {
	if (!parser) {
		return CXFalse;
	}
	while (1) {
		char *line = NULL;
		size_t lineNumber = 0;
		if (!XNetGetLine(parser, &line, &lineNumber)) {
			break;
		}
		char *trimmedLeading = XNetSkipWhitespace(line);
		if (*trimmedLeading == '#') {
			if (XNetIsComment(trimmedLeading)) {
				XNetErrorSet(error, lineNumber, "Comments are not allowed inside edge lists");
				free(line);
				return CXFalse;
			}
			XNetUnreadLine(parser, line, lineNumber);
			break;
		}
		if (XNetIsBlank(trimmedLeading)) {
			free(line);
			continue;
		}
		if (!XNetEdgeListEnsureCapacity(&parser->edges, parser->edges.count + 1)) {
			XNetErrorSet(error, lineNumber, "Failed to grow edge list");
			free(line);
			return CXFalse;
		}
		CXIndex fromIndex = 0;
		CXIndex toIndex = 0;
		float weightValue = 0.0f;

		char *cursor = trimmedLeading;
		char *end = NULL;
		errno = 0;
		long long from = strtoll(cursor, &end, 10);
		if (errno || end == cursor || from < 0) {
			XNetErrorSet(error, lineNumber, "Invalid source vertex index");
			free(line);
			return CXFalse;
		}
		cursor = end;
		long long to = strtoll(cursor, &end, 10);
		if (errno || end == cursor || to < 0) {
			XNetErrorSet(error, lineNumber, "Invalid destination vertex index");
			free(line);
			return CXFalse;
		}
		cursor = end;
		if (weighted) {
			errno = 0;
			float weight = (float)strtod(cursor, &end);
			if (errno || end == cursor) {
				XNetErrorSet(error, lineNumber, "Invalid edge weight");
				free(line);
				return CXFalse;
			}
			weightValue = weight;
			cursor = end;
		}
		while (*cursor && isspace((unsigned char)*cursor)) {
			cursor++;
		}
		if (*cursor != '\0') {
			XNetErrorSet(error, lineNumber, "Unexpected trailing characters in edge line");
			free(line);
			return CXFalse;
		}
		if ((uint64_t)from >= parser->vertexCount || (uint64_t)to >= parser->vertexCount) {
			XNetErrorSet(error, lineNumber, "Edge references vertex outside of range");
			free(line);
			return CXFalse;
		}
		fromIndex = (CXIndex)from;
		toIndex = (CXIndex)to;
		parser->edges.items[parser->edges.count].from = fromIndex;
		parser->edges.items[parser->edges.count].to = toIndex;

		if (weighted) {
			if (!XNetFloatListEnsureCapacity(&parser->legacyWeights, parser->legacyWeights.count + 1)) {
				XNetErrorSet(error, lineNumber, "Failed to allocate legacy weight buffer");
				free(line);
				return CXFalse;
			}
			parser->legacyWeights.items[parser->legacyWeights.count] = weightValue;
			parser->legacyWeights.count++;
		}
		parser->edges.count++;
		free(line);
	}
	return CXTrue;
}

static void XNetParserInit(XNetParser *parser, FILE *file) {
	if (!parser) {
		return;
	}
	memset(parser, 0, sizeof(*parser));
	parser->file = file;
	parser->directed = CXFalse;
	parser->legacy = CXFalse;
	parser->pending.text = NULL;
	parser->pending.valid = CXFalse;
	parser->pending.line = 0;
	XNetAttributeListInit(&parser->vertexAttributes);
	XNetAttributeListInit(&parser->edgeAttributes);
	XNetAttributeListInit(&parser->graphAttributes);
	XNetEdgeListInit(&parser->edges);
	XNetFloatListInit(&parser->legacyWeights);
	XNetAttributeBlockReset(&parser->legacyLabels);
}

static void XNetParserDestroy(XNetParser *parser) {
	if (!parser) {
		return;
	}
	XNetAttributeListFree(&parser->vertexAttributes);
	XNetAttributeListFree(&parser->edgeAttributes);
	XNetAttributeListFree(&parser->graphAttributes);
	XNetEdgeListFree(&parser->edges);
	XNetFloatListFree(&parser->legacyWeights);
	XNetAttributeBlockFree(&parser->legacyLabels);
	if (parser->pending.valid && parser->pending.text) {
		free(parser->pending.text);
		parser->pending.text = NULL;
	}
	parser->pending.valid = CXFalse;
}

static CXBool XNetParserRun(XNetParser *parser, XNetError *error) {
	if (!parser) {
		return CXFalse;
	}
	CXBool continueParsing = CXTrue;
	CXBool legacyWeighted = CXFalse;

	while (continueParsing) {
		char *line = NULL;
		size_t lineNumber = 0;
		if (!XNetGetLine(parser, &line, &lineNumber)) {
			break;
		}
		char *trimLeading = XNetSkipWhitespace(line);
		XNetTrimTrailing(trimLeading);
		if (*trimLeading == '\0') {
			free(line);
			continue;
		}
		if (XNetIsComment(trimLeading)) {
			free(line);
			continue;
		}
		if (!parser->headerSeen) {
			if (strncmp(trimLeading, "#XNET", 5) == 0) {
				if (strcmp(trimLeading, XNET_HEADER_LINE) != 0) {
					XNetErrorSet(error, lineNumber, "Unsupported XNET version, expected %s", XNET_HEADER_LINE);
					free(line);
					return CXFalse;
				}
				parser->headerSeen = CXTrue;
				free(line);
				continue;
			}
			if (strncmp(trimLeading, "#vertices", 9) == 0) {
				parser->legacy = CXTrue;
				parser->headerSeen = CXTrue;
				if (!XNetParseVertices(parser, trimLeading, CXTrue, error, lineNumber)) {
					free(line);
					return CXFalse;
				}
				free(line);
				if (!XNetConsumeLegacyLabels(parser, error)) {
					return CXFalse;
				}
				continue;
			}
			XNetErrorSet(error, lineNumber, "Unexpected first directive '%s'", trimLeading);
			free(line);
			return CXFalse;
		}

		if (strncmp(trimLeading, "#vertices", 9) == 0) {
			if (!XNetParseVertices(parser, trimLeading, parser->legacy, error, lineNumber)) {
				free(line);
				return CXFalse;
			}
			free(line);
			if (parser->legacy && !XNetConsumeLegacyLabels(parser, error)) {
				return CXFalse;
			}
			continue;
		}
		if (strncmp(trimLeading, "#edges", 6) == 0) {
			if (!XNetParseEdgesDirective(parser, trimLeading, parser->legacy, &legacyWeighted, error, lineNumber)) {
				free(line);
				return CXFalse;
			}
			free(line);
			if (!XNetParseEdges(parser, legacyWeighted, error)) {
				return CXFalse;
			}
			continue;
		}
		if (strncmp(trimLeading, "#v ", 3) == 0) {
			if (!parser->vertexCount && !parser->legacy) {
				XNetErrorSet(error, lineNumber, "Vertex attribute encountered before #vertices");
				free(line);
				return CXFalse;
			}
			if (!XNetParseVertexAttribute(parser, trimLeading, error, lineNumber)) {
				free(line);
				return CXFalse;
			}
			free(line);
			continue;
		}
		if (strncmp(trimLeading, "#e ", 3) == 0) {
			if (!parser->edgesSeen) {
				XNetErrorSet(error, lineNumber, "Edge attribute encountered before #edges");
				free(line);
				return CXFalse;
			}
			if (!XNetParseEdgeAttribute(parser, trimLeading, error, lineNumber)) {
				free(line);
				return CXFalse;
			}
			free(line);
			continue;
		}
		if (strncmp(trimLeading, "#g ", 3) == 0) {
			if (!XNetParseGraphAttribute(parser, trimLeading, error, lineNumber)) {
				free(line);
				return CXFalse;
			}
			free(line);
			continue;
		}

		XNetErrorSet(error, lineNumber, "Unknown directive '%s'", trimLeading);
		free(line);
		return CXFalse;
	}

	if (!parser->verticesSeen) {
		XNetErrorSet(error, parser->line, "Missing #vertices section");
		return CXFalse;
	}
	if (!parser->edgesSeen && !parser->legacy) {
		XNetErrorSet(error, parser->line, "Missing #edges section");
		return CXFalse;
	}
	return CXTrue;
}

static CXAttributeType XNetAttributeTypeForBase(XNetBaseType base) {
	switch (base) {
		case XNetBaseFloat:
			return CXFloatAttributeType;
		case XNetBaseInt32:
			return CXIntegerAttributeType;
		case XNetBaseUInt32:
			return CXUnsignedIntegerAttributeType;
		case XNetBaseInt64:
			return CXBigIntegerAttributeType;
		case XNetBaseUInt64:
			return CXUnsignedBigIntegerAttributeType;
		case XNetBaseString:
			return CXStringAttributeType;
		default:
			return CXUnknownAttributeType;
	}
}

static CXBool XNetPopulateAttribute(CXNetworkRef network, XNetAttributeScope scope, const XNetAttributeBlock *block) {
	if (!network || !block) {
		return CXFalse;
	}
	CXAttributeType attributeType = XNetAttributeTypeForBase(block->base);
	if (attributeType == CXUnknownAttributeType) {
		return CXFalse;
	}
	CXBool defined = CXFalse;
	switch (scope) {
		case XNetScopeNode:
			defined = CXNetworkDefineNodeAttribute(network, block->name, attributeType, block->dimension);
			break;
		case XNetScopeEdge:
			defined = CXNetworkDefineEdgeAttribute(network, block->name, attributeType, block->dimension);
			break;
		case XNetScopeGraph:
			defined = CXNetworkDefineNetworkAttribute(network, block->name, attributeType, block->dimension);
			break;
		default:
			return CXFalse;
	}
	if (!defined) {
		return CXFalse;
	}

	CXAttributeRef attr = NULL;
	switch (scope) {
		case XNetScopeNode:
			attr = CXNetworkGetNodeAttribute(network, block->name);
			break;
		case XNetScopeEdge:
			attr = CXNetworkGetEdgeAttribute(network, block->name);
			break;
		case XNetScopeGraph:
			attr = CXNetworkGetNetworkAttribute(network, block->name);
			break;
	}
	if (!attr || !attr->data) {
		return CXFalse;
	}

	if (block->base == XNetBaseString) {
		char **dest = (char **)attr->data;
		for (CXSize i = 0; i < block->count; i++) {
			dest[i] = block->values.asString[i];
			((XNetAttributeBlock *)block)->values.asString[i] = NULL;
		}
		return CXTrue;
	}
	size_t bytes = (size_t)block->count * (size_t)block->dimension;
	if (block->base == XNetBaseFloat) {
		memcpy(attr->data, block->values.asFloat, bytes * sizeof(float));
		return CXTrue;
	}
	if (block->base == XNetBaseInt32) {
		memcpy(attr->data, block->values.asInt32, bytes * sizeof(int32_t));
		return CXTrue;
	}
	if (block->base == XNetBaseUInt32) {
		memcpy(attr->data, block->values.asUInt32, bytes * sizeof(uint32_t));
		return CXTrue;
	}
	if (block->base == XNetBaseInt64) {
		memcpy(attr->data, block->values.asInt64, bytes * sizeof(int64_t));
		return CXTrue;
	}
	if (block->base == XNetBaseUInt64) {
		memcpy(attr->data, block->values.asUInt64, bytes * sizeof(uint64_t));
		return CXTrue;
	}
	return CXFalse;
}

static CXNetworkRef XNetBuildNetwork(const XNetParser *parser, XNetError *error) {
	if (!parser) {
		return NULL;
	}
	CXNetworkRef network = CXNewNetworkWithCapacity(parser->directed ? CXTrue : CXFalse, parser->vertexCount > 0 ? parser->vertexCount : 1, parser->edges.count > 0 ? parser->edges.count : 1);
	if (!network) {
		XNetErrorSet(error, parser->line, "Failed to allocate network");
		return NULL;
	}
	if (parser->vertexCount > 0) {
		if (!CXNetworkAddNodes(network, parser->vertexCount, NULL)) {
			XNetErrorSet(error, parser->line, "Failed to add nodes to network");
			CXFreeNetwork(network);
			return NULL;
		}
	}
	if (parser->edges.count > 0) {
		if (!CXNetworkAddEdges(network, parser->edges.items, parser->edges.count, NULL)) {
			XNetErrorSet(error, parser->line, "Failed to add edges to network");
			CXFreeNetwork(network);
			return NULL;
		}
	}

	// Populate node attributes
	for (size_t i = 0; i < parser->vertexAttributes.count; i++) {
		if (!XNetPopulateAttribute(network, XNetScopeNode, &parser->vertexAttributes.items[i])) {
			XNetErrorSet(error, parser->line, "Failed to populate vertex attribute '%s'", parser->vertexAttributes.items[i].name);
			CXFreeNetwork(network);
			return NULL;
		}
	}

	// Legacy labels
	if (parser->hasLegacyLabels) {
		if (!XNetPopulateAttribute(network, XNetScopeNode, &parser->legacyLabels)) {
			XNetErrorSet(error, parser->line, "Failed to populate legacy label attribute");
			CXFreeNetwork(network);
			return NULL;
		}
	}

	// Edge attributes
	for (size_t i = 0; i < parser->edgeAttributes.count; i++) {
		if (!XNetPopulateAttribute(network, XNetScopeEdge, &parser->edgeAttributes.items[i])) {
			XNetErrorSet(error, parser->line, "Failed to populate edge attribute '%s'", parser->edgeAttributes.items[i].name);
			CXFreeNetwork(network);
			return NULL;
		}
	}

	// Legacy weight attribute
	if (parser->legacyWeights.count > 0) {
		XNetAttributeBlock weightBlock;
		XNetAttributeBlockReset(&weightBlock);
		weightBlock.name = CXNewStringFromString("weight");
		weightBlock.base = XNetBaseFloat;
		weightBlock.dimension = 1;
		weightBlock.count = parser->legacyWeights.count;
		weightBlock.values.asFloat = malloc(sizeof(float) * parser->legacyWeights.count);
		if (!weightBlock.values.asFloat || !weightBlock.name) {
			XNetAttributeBlockFree(&weightBlock);
			XNetErrorSet(error, parser->line, "Failed to allocate legacy weight attribute");
			CXFreeNetwork(network);
			return NULL;
		}
		memcpy(weightBlock.values.asFloat, parser->legacyWeights.items, sizeof(float) * parser->legacyWeights.count);
		if (!XNetPopulateAttribute(network, XNetScopeEdge, &weightBlock)) {
			XNetAttributeBlockFree(&weightBlock);
			XNetErrorSet(error, parser->line, "Failed to populate legacy weight attribute");
			CXFreeNetwork(network);
			return NULL;
		}
		XNetAttributeBlockFree(&weightBlock);
	}

	// Graph attributes
	for (size_t i = 0; i < parser->graphAttributes.count; i++) {
		if (!XNetPopulateAttribute(network, XNetScopeGraph, &parser->graphAttributes.items[i])) {
			XNetErrorSet(error, parser->line, "Failed to populate graph attribute '%s'", parser->graphAttributes.items[i].name);
			CXFreeNetwork(network);
			return NULL;
		}
	}

	return network;
}

struct CXNetwork* CXNetworkReadXNet(const char *path) {
	if (!path) {
		return NULL;
	}
	FILE *file = fopen(path, "rb");
	if (!file) {
		return NULL;
	}
	XNetParser parserState;
	XNetParserInit(&parserState, file);
	XNetError error = {0};
	CXBool ok = XNetParserRun(&parserState, &error);
	fclose(file);

	if (!ok) {
		if (error.message) {
			fprintf(stderr, "XNET parse error: %s\n", error.message);
			free(error.message);
		}
		XNetParserDestroy(&parserState);
		return NULL;
	}

	struct CXNetwork *network = XNetBuildNetwork(&parserState, &error);
	XNetParserDestroy(&parserState);
	if (!network) {
		if (error.message) {
			fprintf(stderr, "XNET build error: %s\n", error.message);
			free(error.message);
		}
	}
	return network;
}

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

typedef struct {
	char *name;
	CXAttributeRef attribute;
	XNetBaseType base;
	CXSize dimension;
} XNetAttributeView;

typedef struct {
	XNetAttributeView *items;
	size_t count;
	size_t capacity;
} XNetAttributeViewList;

static void XNetAttributeViewListInit(XNetAttributeViewList *list) {
	if (!list) {
		return;
	}
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void XNetAttributeViewListFree(XNetAttributeViewList *list) {
	if (!list) {
		return;
	}
	if (list->items) {
		for (size_t i = 0; i < list->count; i++) {
			free(list->items[i].name);
		}
		free(list->items);
	}
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static CXBool XNetAttributeViewListEnsure(XNetAttributeViewList *list, size_t required) {
	if (!list) {
		return CXFalse;
	}
	if (list->capacity >= required) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 4;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}
	XNetAttributeView *items = realloc(list->items, newCapacity * sizeof(XNetAttributeView));
	if (!items) {
		return CXFalse;
	}
	for (size_t i = list->capacity; i < newCapacity; i++) {
		items[i].name = NULL;
		items[i].base = XNetBaseFloat;
		items[i].dimension = 1;
		items[i].attribute = NULL;
	}
	list->items = items;
	list->capacity = newCapacity;
	return CXTrue;
}

static int XNetAttributeNameCompare(const void *lhs, const void *rhs) {
	const XNetAttributeView *a = (const XNetAttributeView *)lhs;
	const XNetAttributeView *b = (const XNetAttributeView *)rhs;
	return strcmp(a->name, b->name);
}

static const char* XNetTypeCodeForAttribute(const XNetAttributeView *view, char buffer[16]) {
	if (!view || !buffer) {
		return NULL;
	}
	switch (view->base) {
		case XNetBaseString:
			strcpy(buffer, "s");
			return buffer;
		case XNetBaseFloat:
			if (view->dimension == 1) {
				strcpy(buffer, "f");
			} else {
				snprintf(buffer, 16, "f%zu", (size_t)view->dimension);
			}
			return buffer;
		case XNetBaseInt32:
			if (view->dimension == 1) {
				strcpy(buffer, "i");
			} else {
				snprintf(buffer, 16, "i%zu", (size_t)view->dimension);
			}
			return buffer;
		case XNetBaseUInt32:
			if (view->dimension == 1) {
				strcpy(buffer, "u");
			} else {
				snprintf(buffer, 16, "u%zu", (size_t)view->dimension);
			}
			return buffer;
		case XNetBaseInt64:
			if (view->dimension == 1) {
				strcpy(buffer, "I");
			} else {
				snprintf(buffer, 16, "I%zu", (size_t)view->dimension);
			}
			return buffer;
		case XNetBaseUInt64:
			if (view->dimension == 1) {
				strcpy(buffer, "U");
			} else {
				snprintf(buffer, 16, "U%zu", (size_t)view->dimension);
			}
			return buffer;
		default:
			return NULL;
	}
}

static CXBool XNetAttributeSupportedForWrite(const CXAttributeRef attribute, XNetBaseType *outBase) {
	if (!attribute || !outBase) {
		return CXFalse;
	}
	switch (attribute->type) {
		case CXFloatAttributeType:
			*outBase = XNetBaseFloat;
			return CXTrue;
		case CXIntegerAttributeType:
			*outBase = XNetBaseInt32;
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
			*outBase = XNetBaseUInt32;
			return CXTrue;
		case CXBigIntegerAttributeType:
			*outBase = XNetBaseInt64;
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			*outBase = XNetBaseUInt64;
			return CXTrue;
		case CXStringAttributeType:
			if (attribute->dimension != 1) {
				return CXFalse;
			}
			*outBase = XNetBaseString;
			return CXTrue;
		default:
			return CXFalse;
	}
}

static CXBool XNetCollectAttributes(CXStringDictionaryRef dictionary, XNetAttributeViewList *outList, const char *skipName) {
	if (!outList) {
		return CXFalse;
	}
	size_t count = CXStringDictionaryCount(dictionary);
	if (!XNetAttributeViewListEnsure(outList, outList->count + count)) {
		return CXFalse;
	}
	CXStringDictionaryFOR(entry, dictionary) {
		if (skipName && strcmp(entry->key, skipName) == 0) {
			continue;
		}
		XNetBaseType base = XNetBaseFloat;
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		if (!XNetAttributeSupportedForWrite(attribute, &base)) {
			return CXFalse;
		}
		if (outList->count >= outList->capacity) {
			return CXFalse;
		}
		XNetAttributeView *view = &outList->items[outList->count++];
		view->name = CXNewStringFromString(entry->key);
		if (!view->name) {
			return CXFalse;
		}
		view->attribute = attribute;
		view->base = base;
		view->dimension = attribute->dimension > 0 ? attribute->dimension : 1;
	}
	if (outList->count > 1) {
		qsort(outList->items, outList->count, sizeof(XNetAttributeView), XNetAttributeNameCompare);
	}
	return CXTrue;
}

static void XNetWriteEscapedString(FILE *file, const char *value) {
	if (!value) {
		fputs("\"\"", file);
		return;
	}
	CXBool needsQuote = CXFalse;
	if (*value == '\0' || value[0] == '#') {
		needsQuote = CXTrue;
	} else {
		for (const char *ptr = value; *ptr; ptr++) {
			unsigned char ch = (unsigned char)*ptr;
			if (isspace(ch) || ch == '"' || ch == '\\' || ch < 0x20 || ch == 0x7F) {
				needsQuote = CXTrue;
				break;
			}
		}
		if (!needsQuote) {
			size_t len = strlen(value);
			if (len > 0 && isspace((unsigned char)value[len - 1])) {
				needsQuote = CXTrue;
			}
		}
	}
	if (!needsQuote) {
		fputs(value, file);
		return;
	}
	fputc('"', file);
	for (const char *ptr = value; *ptr; ptr++) {
		char ch = *ptr;
		switch (ch) {
			case '\\': fputs("\\\\", file); break;
			case '"': fputs("\\\"", file); break;
			case '\n': fputs("\\n", file); break;
			case '\t': fputs("\\t", file); break;
			case '\r': fputs("\\r", file); break;
			default:
				if ((unsigned char)ch < 0x20 || (unsigned char)ch == 0x7F) {
					// control character, escape
					fprintf(file, "\\x%02X", (unsigned char)ch);
				} else {
					fputc(ch, file);
				}
				break;
		}
	}
	fputc('"', file);
}

static CXBool XNetWriteVertexAttributes(FILE *file, const XNetAttributeViewList *attrs, const CXIndex *activeNodes, CXSize nodeCount) {
	for (size_t i = 0; i < attrs->count; i++) {
		const XNetAttributeView *view = &attrs->items[i];
		char typeCode[16];
		const char *typeStr = XNetTypeCodeForAttribute(view, typeCode);
		if (!typeStr) {
			return CXFalse;
		}
		fprintf(file, "#v \"%s\" %s\n", view->name, typeStr);
		if (view->base == XNetBaseString) {
			CXString *values = (CXString *)view->attribute->data;
			for (CXSize idx = 0; idx < nodeCount; idx++) {
				CXIndex original = activeNodes[idx];
				CXString value = values ? values[original] : NULL;
				XNetWriteEscapedString(file, value ? value : "");
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseFloat) {
			float *values = (float *)view->attribute->data;
			for (CXSize idx = 0; idx < nodeCount; idx++) {
				CXIndex original = activeNodes[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					float value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%.9g", value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseInt32) {
			int32_t *values = (int32_t *)view->attribute->data;
			for (CXSize idx = 0; idx < nodeCount; idx++) {
				CXIndex original = activeNodes[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					int32_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRId32, value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseUInt32) {
			uint32_t *values = (uint32_t *)view->attribute->data;
			for (CXSize idx = 0; idx < nodeCount; idx++) {
				CXIndex original = activeNodes[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					uint32_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRIu32, value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseInt64) {
			int64_t *values = (int64_t *)view->attribute->data;
			for (CXSize idx = 0; idx < nodeCount; idx++) {
				CXIndex original = activeNodes[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					int64_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRId64, value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseUInt64) {
			uint64_t *values = (uint64_t *)view->attribute->data;
			for (CXSize idx = 0; idx < nodeCount; idx++) {
				CXIndex original = activeNodes[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					uint64_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRIu64, value);
				}
				fputc('\n', file);
			}
		}
	}
	return CXTrue;
}

static CXBool XNetWriteEdgeAttributes(FILE *file, const XNetAttributeViewList *attrs, const CXIndex *edgeOrder, CXSize edgeCount) {
	for (size_t i = 0; i < attrs->count; i++) {
		const XNetAttributeView *view = &attrs->items[i];
		char typeCode[16];
		const char *typeStr = XNetTypeCodeForAttribute(view, typeCode);
		if (!typeStr) {
			return CXFalse;
		}
		fprintf(file, "#e \"%s\" %s\n", view->name, typeStr);
		if (view->base == XNetBaseString) {
			CXString *values = (CXString *)view->attribute->data;
			for (CXSize idx = 0; idx < edgeCount; idx++) {
				CXIndex original = edgeOrder[idx];
				CXString value = values ? values[original] : NULL;
				XNetWriteEscapedString(file, value ? value : "");
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseFloat) {
			float *values = (float *)view->attribute->data;
			for (CXSize idx = 0; idx < edgeCount; idx++) {
				CXIndex original = edgeOrder[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					float value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%.9g", value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseInt32) {
			int32_t *values = (int32_t *)view->attribute->data;
			for (CXSize idx = 0; idx < edgeCount; idx++) {
				CXIndex original = edgeOrder[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					int32_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRId32, value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseUInt32) {
			uint32_t *values = (uint32_t *)view->attribute->data;
			for (CXSize idx = 0; idx < edgeCount; idx++) {
				CXIndex original = edgeOrder[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					uint32_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRIu32, value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseInt64) {
			int64_t *values = (int64_t *)view->attribute->data;
			for (CXSize idx = 0; idx < edgeCount; idx++) {
				CXIndex original = edgeOrder[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					int64_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRId64, value);
				}
				fputc('\n', file);
			}
		} else if (view->base == XNetBaseUInt64) {
			uint64_t *values = (uint64_t *)view->attribute->data;
			for (CXSize idx = 0; idx < edgeCount; idx++) {
				CXIndex original = edgeOrder[idx];
				for (CXSize d = 0; d < view->dimension; d++) {
					if (d > 0) {
						fputc(' ', file);
					}
					uint64_t value = values[(size_t)original * view->attribute->dimension + d];
					fprintf(file, "%" PRIu64, value);
				}
				fputc('\n', file);
			}
		}
	}
	return CXTrue;
}

static CXBool XNetWriteGraphAttributes(FILE *file, const XNetAttributeViewList *attrs) {
	for (size_t i = 0; i < attrs->count; i++) {
		const XNetAttributeView *view = &attrs->items[i];
		char typeCode[16];
		const char *typeStr = XNetTypeCodeForAttribute(view, typeCode);
		if (!typeStr) {
			return CXFalse;
		}
		fprintf(file, "#g \"%s\" %s\n", view->name, typeStr);
		if (view->base == XNetBaseString) {
			CXString *value = (CXString *)view->attribute->data;
			XNetWriteEscapedString(file, value && value[0] ? value[0] : "");
			fputc('\n', file);
		} else if (view->base == XNetBaseFloat) {
			float *value = (float *)view->attribute->data;
			for (CXSize d = 0; d < view->dimension; d++) {
				if (d > 0) {
					fputc(' ', file);
				}
				fprintf(file, "%.9g", value[d]);
			}
			fputc('\n', file);
		} else if (view->base == XNetBaseInt32) {
			int32_t *value = (int32_t *)view->attribute->data;
			for (CXSize d = 0; d < view->dimension; d++) {
				if (d > 0) {
					fputc(' ', file);
				}
				fprintf(file, "%" PRId32, value[d]);
			}
			fputc('\n', file);
		} else if (view->base == XNetBaseUInt32) {
			uint32_t *value = (uint32_t *)view->attribute->data;
			for (CXSize d = 0; d < view->dimension; d++) {
				if (d > 0) {
					fputc(' ', file);
				}
				fprintf(file, "%" PRIu32, value[d]);
			}
			fputc('\n', file);
		} else if (view->base == XNetBaseInt64) {
			int64_t *value = (int64_t *)view->attribute->data;
			for (CXSize d = 0; d < view->dimension; d++) {
				if (d > 0) {
					fputc(' ', file);
				}
				fprintf(file, "%" PRId64, value[d]);
			}
			fputc('\n', file);
		} else if (view->base == XNetBaseUInt64) {
			uint64_t *value = (uint64_t *)view->attribute->data;
			for (CXSize d = 0; d < view->dimension; d++) {
				if (d > 0) {
					fputc(' ', file);
				}
				fprintf(file, "%" PRIu64, value[d]);
			}
			fputc('\n', file);
		}
	}
	return CXTrue;
}

CXBool CXNetworkWriteXNet(CXNetworkRef network, const char *path) {
	if (!network || !path) {
		return CXFalse;
	}
	FILE *file = fopen(path, "wb");
	if (!file) {
		return CXFalse;
	}

	CXSize nodeCount = network->nodeCount;
	CXSize edgeCount = network->edgeCount;
	CXBool directed = network->isDirected;

	CXIndex *nodeRemap = calloc(network->nodeCapacity > 0 ? network->nodeCapacity : 1, sizeof(CXIndex));
	CXIndex *activeNodes = nodeCount > 0 ? malloc(sizeof(CXIndex) * nodeCount) : NULL;
	CXIndex *edgeOrder = edgeCount > 0 ? malloc(sizeof(CXIndex) * edgeCount) : NULL;
	CXEdge *compactEdges = edgeCount > 0 ? malloc(sizeof(CXEdge) * edgeCount) : NULL;
	char **originalIdStrings = nodeCount > 0 ? calloc(nodeCount, sizeof(char *)) : NULL;
	CXBool success = CXFalse;

	if ((nodeCount > 0 && (!nodeRemap || !activeNodes || !originalIdStrings)) ||
	    (edgeCount > 0 && (!edgeOrder || !compactEdges))) {
		goto cleanup;
	}

	for (CXSize i = 0; i < network->nodeCapacity; i++) {
		nodeRemap[i] = CXIndexMAX;
	}
	CXSize nextNode = 0;
	for (CXSize i = 0; i < network->nodeCapacity; i++) {
		if (network->nodeActive && network->nodeActive[i]) {
			nodeRemap[i] = (CXIndex)nextNode;
			if (activeNodes) {
				activeNodes[nextNode] = (CXIndex)i;
			}
			if (originalIdStrings) {
				originalIdStrings[nextNode] = CXNewStringFromFormat("%" PRIu64, (uint64_t)i);
				if (!originalIdStrings[nextNode]) {
					goto cleanup;
				}
			}
			nextNode++;
		}
	}
	if (nextNode != nodeCount) {
		goto cleanup;
	}

	CXSize nextEdge = 0;
	for (CXSize i = 0; i < network->edgeCapacity; i++) {
		if (network->edgeActive && network->edgeActive[i]) {
			CXEdge edge = network->edges[i];
			if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
				goto cleanup;
			}
			CXIndex mappedFrom = nodeRemap[edge.from];
			CXIndex mappedTo = nodeRemap[edge.to];
			if (mappedFrom == CXIndexMAX || mappedTo == CXIndexMAX) {
				goto cleanup;
			}
			if (compactEdges) {
				compactEdges[nextEdge].from = mappedFrom;
				compactEdges[nextEdge].to = mappedTo;
			}
			if (edgeOrder) {
				edgeOrder[nextEdge] = (CXIndex)i;
			}
			nextEdge++;
		}
	}
	if (nextEdge != edgeCount) {
		goto cleanup;
	}

	XNetAttributeViewList nodeAttrs;
	XNetAttributeViewList edgeAttrs;
	XNetAttributeViewList graphAttrs;
	XNetAttributeViewListInit(&nodeAttrs);
	XNetAttributeViewListInit(&edgeAttrs);
	XNetAttributeViewListInit(&graphAttrs);

	if (!XNetCollectAttributes(network->nodeAttributes, &nodeAttrs, "_original_ids_") ||
	    !XNetCollectAttributes(network->edgeAttributes, &edgeAttrs, NULL) ||
	    !XNetCollectAttributes(network->networkAttributes, &graphAttrs, NULL)) {
		XNetAttributeViewListFree(&nodeAttrs);
		XNetAttributeViewListFree(&edgeAttrs);
		XNetAttributeViewListFree(&graphAttrs);
		goto cleanup;
	}

	fprintf(file, "%s\n", XNET_HEADER_LINE);
	fprintf(file, "#vertices %zu\n", (size_t)nodeCount);
	if (!XNetWriteGraphAttributes(file, &graphAttrs)) {
		XNetAttributeViewListFree(&nodeAttrs);
		XNetAttributeViewListFree(&edgeAttrs);
		XNetAttributeViewListFree(&graphAttrs);
		goto cleanup;
	}
	fprintf(file, "#edges %s\n", directed ? "directed" : "undirected");
	for (CXSize i = 0; i < edgeCount; i++) {
		fprintf(file, "%zu %zu\n", (size_t)compactEdges[i].from, (size_t)compactEdges[i].to);
	}
	if (!XNetWriteVertexAttributes(file, &nodeAttrs, activeNodes, nodeCount)) {
		XNetAttributeViewListFree(&nodeAttrs);
		XNetAttributeViewListFree(&edgeAttrs);
		XNetAttributeViewListFree(&graphAttrs);
		goto cleanup;
	}

	// Write original IDs attribute
	if (nodeCount > 0) {
		fprintf(file, "#v \"_original_ids_\" s\n");
		for (CXSize i = 0; i < nodeCount; i++) {
			XNetWriteEscapedString(file, originalIdStrings[i] ? originalIdStrings[i] : "");
			fputc('\n', file);
		}
	}

	if (!XNetWriteEdgeAttributes(file, &edgeAttrs, edgeOrder, edgeCount)) {
		XNetAttributeViewListFree(&nodeAttrs);
		XNetAttributeViewListFree(&edgeAttrs);
		XNetAttributeViewListFree(&graphAttrs);
		goto cleanup;
	}

	XNetAttributeViewListFree(&nodeAttrs);
	XNetAttributeViewListFree(&edgeAttrs);
	XNetAttributeViewListFree(&graphAttrs);

	if (ferror(file)) {
		goto cleanup;
	}
	success = CXTrue;

cleanup:
	if (originalIdStrings) {
		for (CXSize i = 0; i < nodeCount; i++) {
			free(originalIdStrings[i]);
		}
	}
	free(originalIdStrings);
	free(nodeRemap);
	free(activeNodes);
	free(edgeOrder);
	free(compactEdges);

	fclose(file);
	return success;
}
