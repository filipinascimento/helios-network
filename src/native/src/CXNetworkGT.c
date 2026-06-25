#include "CXNetworkGT.h"
#include "CXNetwork.h"
#include "CXDictionary.h"
#include "CXZstd.h"

#include <errno.h>
#include <limits.h>

enum {
	GT_PROP_GRAPH = 0,
	GT_PROP_VERTEX = 1,
	GT_PROP_EDGE = 2
};

enum {
	GT_VALUE_BOOL = 0,
	GT_VALUE_INT16 = 1,
	GT_VALUE_INT32 = 2,
	GT_VALUE_INT64 = 3,
	GT_VALUE_DOUBLE = 4,
	GT_VALUE_LONG_DOUBLE = 5,
	GT_VALUE_STRING = 6,
	GT_VALUE_VECTOR_BOOL = 7,
	GT_VALUE_VECTOR_INT16 = 8,
	GT_VALUE_VECTOR_INT32 = 9,
	GT_VALUE_VECTOR_INT64 = 10,
	GT_VALUE_VECTOR_DOUBLE = 11,
	GT_VALUE_VECTOR_LONG_DOUBLE = 12,
	GT_VALUE_VECTOR_STRING = 13,
	GT_VALUE_PYTHON_OBJECT = 14
};

static const uint8_t GT_MAGIC[] = { 0xe2, 0x9b, 0xbe, 0x20, 0x67, 0x74 };

typedef struct {
	CXZstdInputStream *stream;
	CXBool bigEndian;
} GTReader;

typedef struct {
	CXEdge *items;
	CXSize count;
	CXSize capacity;
} GTEdgeArray;

static CXBool GTEdgeArrayAppend(GTEdgeArray *array, CXEdge edge) {
	if (!array) {
		return CXFalse;
	}
	if (array->count >= array->capacity) {
		CXSize next = array->capacity > 0 ? array->capacity * 2 : 256;
		if (next <= array->count) {
			next = array->count + 1;
		}
		CXEdge *items = realloc(array->items, sizeof(CXEdge) * next);
		if (!items) {
			return CXFalse;
		}
		array->items = items;
		array->capacity = next;
	}
	array->items[array->count++] = edge;
	return CXTrue;
}

static CXBool GTReadRaw(GTReader *reader, void *dst, size_t size) {
	return reader && reader->stream && CXZstdInputStreamRead(reader->stream, dst, size);
}

static CXBool GTWriteRaw(FILE *file, const void *src, size_t size) {
	return file && fwrite(src, 1, size, file) == size;
}

static uint16_t GTSwap16(uint16_t value) {
	return (uint16_t)((value >> 8) | (value << 8));
}

static uint32_t GTSwap32(uint32_t value) {
	return ((value & 0x000000ffu) << 24) |
		((value & 0x0000ff00u) << 8) |
		((value & 0x00ff0000u) >> 8) |
		((value & 0xff000000u) >> 24);
}

static uint64_t GTSwap64(uint64_t value) {
	return ((value & UINT64_C(0x00000000000000ff)) << 56) |
		((value & UINT64_C(0x000000000000ff00)) << 40) |
		((value & UINT64_C(0x0000000000ff0000)) << 24) |
		((value & UINT64_C(0x00000000ff000000)) << 8) |
		((value & UINT64_C(0x000000ff00000000)) >> 8) |
		((value & UINT64_C(0x0000ff0000000000)) >> 24) |
		((value & UINT64_C(0x00ff000000000000)) >> 40) |
		((value & UINT64_C(0xff00000000000000)) >> 56);
}

static CXBool GTNativeIsBigEndian(void) {
	const uint16_t marker = 0x0102;
	return *((const uint8_t *)&marker) == 0x01 ? CXTrue : CXFalse;
}

static CXBool GTNeedsSwap(GTReader *reader) {
	return reader && reader->bigEndian != GTNativeIsBigEndian();
}

static CXBool GTReadU8(GTReader *reader, uint8_t *out) {
	return GTReadRaw(reader, out, sizeof(uint8_t));
}

static CXBool GTReadU16(GTReader *reader, uint16_t *out) {
	uint16_t value = 0;
	if (!GTReadRaw(reader, &value, sizeof(value))) {
		return CXFalse;
	}
	if (GTNeedsSwap(reader)) {
		value = GTSwap16(value);
	}
	*out = value;
	return CXTrue;
}

static CXBool GTReadU32(GTReader *reader, uint32_t *out) {
	uint32_t value = 0;
	if (!GTReadRaw(reader, &value, sizeof(value))) {
		return CXFalse;
	}
	if (GTNeedsSwap(reader)) {
		value = GTSwap32(value);
	}
	*out = value;
	return CXTrue;
}

static CXBool GTReadU64(GTReader *reader, uint64_t *out) {
	uint64_t value = 0;
	if (!GTReadRaw(reader, &value, sizeof(value))) {
		return CXFalse;
	}
	if (GTNeedsSwap(reader)) {
		value = GTSwap64(value);
	}
	*out = value;
	return CXTrue;
}

static CXBool GTReadI16(GTReader *reader, int16_t *out) {
	uint16_t value = 0;
	if (!GTReadU16(reader, &value)) {
		return CXFalse;
	}
	*out = (int16_t)value;
	return CXTrue;
}

static CXBool GTReadI32(GTReader *reader, int32_t *out) {
	uint32_t value = 0;
	if (!GTReadU32(reader, &value)) {
		return CXFalse;
	}
	*out = (int32_t)value;
	return CXTrue;
}

static CXBool GTReadI64(GTReader *reader, int64_t *out) {
	uint64_t value = 0;
	if (!GTReadU64(reader, &value)) {
		return CXFalse;
	}
	*out = (int64_t)value;
	return CXTrue;
}

static CXBool GTReadDouble(GTReader *reader, double *out) {
	uint64_t bits = 0;
	if (!GTReadU64(reader, &bits)) {
		return CXFalse;
	}
	memcpy(out, &bits, sizeof(double));
	return CXTrue;
}

static CXBool GTSkip(GTReader *reader, uint64_t size) {
	return reader && reader->stream && CXZstdInputStreamSkip(reader->stream, size);
}

static char* GTReadString(GTReader *reader) {
	uint64_t length = 0;
	if (!GTReadU64(reader, &length)) {
		return NULL;
	}
	if (length > SIZE_MAX - 1) {
		return NULL;
	}
	char *value = calloc((size_t)length + 1, sizeof(char));
	if (!value) {
		return NULL;
	}
	if (length > 0 && !GTReadRaw(reader, value, (size_t)length)) {
		free(value);
		return NULL;
	}
	value[length] = '\0';
	return value;
}

static CXBool GTSkipString(GTReader *reader) {
	uint64_t length = 0;
	return GTReadU64(reader, &length) && GTSkip(reader, length);
}

static CXBool GTWriteU8(FILE *file, uint8_t value) {
	return GTWriteRaw(file, &value, sizeof(value));
}

static CXBool GTWriteU16(FILE *file, uint16_t value) {
	return GTWriteRaw(file, &value, sizeof(value));
}

static CXBool GTWriteU32(FILE *file, uint32_t value) {
	return GTWriteRaw(file, &value, sizeof(value));
}

static CXBool GTWriteU64(FILE *file, uint64_t value) {
	return GTWriteRaw(file, &value, sizeof(value));
}

static CXBool GTWriteI32(FILE *file, int32_t value) {
	return GTWriteU32(file, (uint32_t)value);
}

static CXBool GTWriteI64(FILE *file, int64_t value) {
	return GTWriteU64(file, (uint64_t)value);
}

static CXBool GTWriteDouble(FILE *file, double value) {
	uint64_t bits = 0;
	memcpy(&bits, &value, sizeof(double));
	return GTWriteU64(file, bits);
}

static CXBool GTWriteString(FILE *file, const char *value) {
	if (!value) {
		value = "";
	}
	size_t length = strlen(value);
	return GTWriteU64(file, (uint64_t)length) && GTWriteRaw(file, value, length);
}

static CXBool GTReadNodeIndex(GTReader *reader, uint8_t width, uint64_t *out) {
	if (width == 1) {
		uint8_t value = 0;
		if (!GTReadU8(reader, &value)) return CXFalse;
		*out = value;
		return CXTrue;
	}
	if (width == 2) {
		uint16_t value = 0;
		if (!GTReadU16(reader, &value)) return CXFalse;
		*out = value;
		return CXTrue;
	}
	if (width == 4) {
		uint32_t value = 0;
		if (!GTReadU32(reader, &value)) return CXFalse;
		*out = value;
		return CXTrue;
	}
	if (width == 8) {
		return GTReadU64(reader, out);
	}
	return CXFalse;
}

static CXBool GTWriteNodeIndex(FILE *file, uint8_t width, uint64_t value) {
	if (width == 1) return GTWriteU8(file, (uint8_t)value);
	if (width == 2) return GTWriteU16(file, (uint16_t)value);
	if (width == 4) return GTWriteU32(file, (uint32_t)value);
	if (width == 8) return GTWriteU64(file, value);
	return CXFalse;
}

static uint8_t GTNodeIndexWidth(uint64_t nodeCount) {
	if (nodeCount <= UINT8_MAX) return 1;
	if (nodeCount <= UINT16_MAX) return 2;
	if (nodeCount <= UINT32_MAX) return 4;
	return 8;
}

static CXBool GTDefineAttribute(CXNetworkRef network, uint8_t propType, const char *name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	CXString mutableName = (CXString)name;
	if (propType == GT_PROP_VERTEX) return CXNetworkDefineNodeAttribute(network, mutableName, type, dimension);
	if (propType == GT_PROP_EDGE) return CXNetworkDefineEdgeAttribute(network, mutableName, type, dimension);
	if (propType == GT_PROP_GRAPH) return CXNetworkDefineNetworkAttribute(network, mutableName, type, dimension);
	return CXFalse;
}

static CXAttributeRef GTGetAttribute(CXNetworkRef network, uint8_t propType, const char *name) {
	CXString mutableName = (CXString)name;
	if (propType == GT_PROP_VERTEX) return CXNetworkGetNodeAttribute(network, mutableName);
	if (propType == GT_PROP_EDGE) return CXNetworkGetEdgeAttribute(network, mutableName);
	if (propType == GT_PROP_GRAPH) return CXNetworkGetNetworkAttribute(network, mutableName);
	return NULL;
}

static const char* GTPropTypeLabel(uint8_t propType) {
	if (propType == GT_PROP_VERTEX) return "vertex";
	if (propType == GT_PROP_EDGE) return "edge";
	if (propType == GT_PROP_GRAPH) return "graph";
	return "unknown";
}

static CXBool GTReadScalarProperty(GTReader *reader, CXNetworkRef network, uint8_t propType, const char *name, uint8_t valueType, CXSize count) {
	if (valueType == GT_VALUE_BOOL) {
		if (!GTDefineAttribute(network, propType, name, CXBooleanAttributeType, 1)) return CXFalse;
		CXAttributeRef attr = GTGetAttribute(network, propType, name);
		uint8_t *dst = attr ? (uint8_t *)attr->data : NULL;
		if (!dst) return CXFalse;
		for (CXSize i = 0; i < count; i++) {
			uint8_t value = 0;
			if (!GTReadU8(reader, &value)) return CXFalse;
			dst[i] = value != 0 ? 1 : 0;
		}
		return CXTrue;
	}
	if (valueType == GT_VALUE_INT16 || valueType == GT_VALUE_INT32) {
		if (!GTDefineAttribute(network, propType, name, CXIntegerAttributeType, 1)) return CXFalse;
		CXAttributeRef attr = GTGetAttribute(network, propType, name);
		int32_t *dst = attr ? (int32_t *)attr->data : NULL;
		if (!dst) return CXFalse;
		for (CXSize i = 0; i < count; i++) {
			if (valueType == GT_VALUE_INT16) {
				int16_t value = 0;
				if (!GTReadI16(reader, &value)) return CXFalse;
				dst[i] = value;
			} else {
				if (!GTReadI32(reader, &dst[i])) return CXFalse;
			}
		}
		return CXTrue;
	}
	if (valueType == GT_VALUE_INT64) {
		int64_t *values = count > 0 ? calloc(count, sizeof(int64_t)) : NULL;
		if (count > 0 && !values) return CXFalse;
		CXBool fitsInt32 = CXTrue;
		for (CXSize i = 0; i < count; i++) {
			if (!GTReadI64(reader, &values[i])) {
				free(values);
				return CXFalse;
			}
			if (values[i] < INT32_MIN || values[i] > INT32_MAX) {
				fitsInt32 = CXFalse;
			}
		}
		CXAttributeType type = fitsInt32 ? CXIntegerAttributeType : CXBigIntegerAttributeType;
		if (!GTDefineAttribute(network, propType, name, type, 1)) {
			free(values);
			return CXFalse;
		}
		CXAttributeRef attr = GTGetAttribute(network, propType, name);
		if (!attr || !attr->data) {
			free(values);
			return CXFalse;
		}
		if (fitsInt32) {
			int32_t *dst = (int32_t *)attr->data;
			for (CXSize i = 0; i < count; i++) dst[i] = (int32_t)values[i];
		} else {
			memcpy(attr->data, values, sizeof(int64_t) * count);
		}
		free(values);
		return CXTrue;
	}
	if (valueType == GT_VALUE_DOUBLE) {
		if (!GTDefineAttribute(network, propType, name, CXDoubleAttributeType, 1)) return CXFalse;
		CXAttributeRef attr = GTGetAttribute(network, propType, name);
		double *dst = attr ? (double *)attr->data : NULL;
		if (!dst) return CXFalse;
		for (CXSize i = 0; i < count; i++) {
			if (!GTReadDouble(reader, &dst[i])) return CXFalse;
		}
		return CXTrue;
	}
	if (valueType == GT_VALUE_STRING) {
		if (!GTDefineAttribute(network, propType, name, CXStringAttributeType, 1)) return CXFalse;
		CXAttributeRef attr = GTGetAttribute(network, propType, name);
		char **dst = attr ? (char **)attr->data : NULL;
		if (!dst) return CXFalse;
		for (CXSize i = 0; i < count; i++) {
			dst[i] = GTReadString(reader);
			if (!dst[i]) return CXFalse;
		}
		return CXTrue;
	}
	return CXFalse;
}

static CXBool GTSkipPropertyValues(GTReader *reader, uint8_t valueType, CXSize count) {
	for (CXSize i = 0; i < count; i++) {
		uint64_t length = 0;
		switch (valueType) {
			case GT_VALUE_BOOL: if (!GTSkip(reader, sizeof(uint8_t))) return CXFalse; break;
			case GT_VALUE_INT16: if (!GTSkip(reader, sizeof(int16_t))) return CXFalse; break;
			case GT_VALUE_INT32: if (!GTSkip(reader, sizeof(int32_t))) return CXFalse; break;
			case GT_VALUE_INT64: if (!GTSkip(reader, sizeof(int64_t))) return CXFalse; break;
			case GT_VALUE_DOUBLE: if (!GTSkip(reader, sizeof(double))) return CXFalse; break;
			case GT_VALUE_LONG_DOUBLE: if (!GTSkip(reader, sizeof(long double))) return CXFalse; break;
			case GT_VALUE_STRING:
			case GT_VALUE_PYTHON_OBJECT:
				if (!GTSkipString(reader)) return CXFalse;
				break;
			case GT_VALUE_VECTOR_BOOL:
				if (!GTReadU64(reader, &length) || !GTSkip(reader, length * sizeof(uint8_t))) return CXFalse;
				break;
			case GT_VALUE_VECTOR_INT16:
				if (!GTReadU64(reader, &length) || !GTSkip(reader, length * sizeof(int16_t))) return CXFalse;
				break;
			case GT_VALUE_VECTOR_INT32:
				if (!GTReadU64(reader, &length) || !GTSkip(reader, length * sizeof(int32_t))) return CXFalse;
				break;
			case GT_VALUE_VECTOR_INT64:
				if (!GTReadU64(reader, &length) || !GTSkip(reader, length * sizeof(int64_t))) return CXFalse;
				break;
			case GT_VALUE_VECTOR_DOUBLE:
				if (!GTReadU64(reader, &length) || !GTSkip(reader, length * sizeof(double))) return CXFalse;
				break;
			case GT_VALUE_VECTOR_LONG_DOUBLE:
				if (!GTReadU64(reader, &length) || !GTSkip(reader, length * sizeof(long double))) return CXFalse;
				break;
			case GT_VALUE_VECTOR_STRING:
				if (!GTReadU64(reader, &length)) return CXFalse;
				for (uint64_t j = 0; j < length; j++) {
					if (!GTSkipString(reader)) return CXFalse;
				}
				break;
			default:
				return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool GTReadNumericVectorProperty(GTReader *reader, CXNetworkRef network, uint8_t propType, const char *name, uint8_t valueType, CXSize count) {
	uint64_t dimension = UINT64_MAX;
	CXBool consistent = CXTrue;
	CXBool fitsInt32 = CXTrue;
	uint8_t *boolValues = NULL;
	int32_t *int32Values = NULL;
	int64_t *int64Values = NULL;
	double *doubleValues = NULL;
	for (CXSize i = 0; i < count; i++) {
		uint64_t length = 0;
		if (!GTReadU64(reader, &length)) goto fail;
		if (dimension == UINT64_MAX) {
			dimension = length;
			if (dimension > SIZE_MAX || count > 0) {
				size_t total = (size_t)dimension * (size_t)count;
				if (dimension != 0 && total / (size_t)dimension != (size_t)count) goto fail;
				if (valueType == GT_VALUE_VECTOR_BOOL) boolValues = total > 0 ? calloc(total, sizeof(uint8_t)) : NULL;
				else if (valueType == GT_VALUE_VECTOR_INT16 || valueType == GT_VALUE_VECTOR_INT32) int32Values = total > 0 ? calloc(total, sizeof(int32_t)) : NULL;
				else if (valueType == GT_VALUE_VECTOR_INT64) int64Values = total > 0 ? calloc(total, sizeof(int64_t)) : NULL;
				else if (valueType == GT_VALUE_VECTOR_DOUBLE) doubleValues = total > 0 ? calloc(total, sizeof(double)) : NULL;
				if (total > 0 && !boolValues && !int32Values && !int64Values && !doubleValues) goto fail;
			}
		} else if (length != dimension) {
			consistent = CXFalse;
		}
		for (uint64_t d = 0; d < length; d++) {
			size_t offset = (size_t)i * (size_t)(dimension == UINT64_MAX ? 0 : dimension) + (size_t)d;
			if (valueType == GT_VALUE_VECTOR_BOOL) {
				uint8_t value = 0;
				if (!GTReadU8(reader, &value)) goto fail;
				if (consistent && d < dimension && boolValues) boolValues[offset] = value != 0 ? 1 : 0;
			} else if (valueType == GT_VALUE_VECTOR_INT16) {
				int16_t value = 0;
				if (!GTReadI16(reader, &value)) goto fail;
				if (consistent && d < dimension && int32Values) int32Values[offset] = value;
			} else if (valueType == GT_VALUE_VECTOR_INT32) {
				int32_t value = 0;
				if (!GTReadI32(reader, &value)) goto fail;
				if (consistent && d < dimension && int32Values) int32Values[offset] = value;
			} else if (valueType == GT_VALUE_VECTOR_INT64) {
				int64_t value = 0;
				if (!GTReadI64(reader, &value)) goto fail;
				if (value < INT32_MIN || value > INT32_MAX) fitsInt32 = CXFalse;
				if (consistent && d < dimension && int64Values) int64Values[offset] = value;
			} else if (valueType == GT_VALUE_VECTOR_DOUBLE) {
				double value = 0;
				if (!GTReadDouble(reader, &value)) goto fail;
				if (consistent && d < dimension && doubleValues) doubleValues[offset] = value;
			}
		}
	}
	if (!consistent) {
		CXNetworkSerializationWarningAppend("GT skipped %s property \"%s\" with inconsistent vector lengths", GTPropTypeLabel(propType), name);
		free(boolValues); free(int32Values); free(int64Values); free(doubleValues);
		return CXTrue;
	}
	if (dimension == UINT64_MAX || dimension == 0) {
		CXNetworkSerializationWarningAppend("GT skipped %s property \"%s\" with empty vector values", GTPropTypeLabel(propType), name);
		free(boolValues); free(int32Values); free(int64Values); free(doubleValues);
		return CXTrue;
	}
	CXSize dim = (CXSize)dimension;
	CXAttributeType attrType = CXDoubleAttributeType;
	if (valueType == GT_VALUE_VECTOR_BOOL) attrType = CXBooleanAttributeType;
	else if (valueType == GT_VALUE_VECTOR_INT16 || valueType == GT_VALUE_VECTOR_INT32) attrType = CXIntegerAttributeType;
	else if (valueType == GT_VALUE_VECTOR_INT64) attrType = fitsInt32 ? CXIntegerAttributeType : CXBigIntegerAttributeType;
	if (!GTDefineAttribute(network, propType, name, attrType, dim)) goto fail;
	CXAttributeRef attr = GTGetAttribute(network, propType, name);
	if (!attr || !attr->data) goto fail;
	if (attrType == CXBooleanAttributeType) memcpy(attr->data, boolValues, sizeof(uint8_t) * count * dim);
	else if (attrType == CXIntegerAttributeType) {
		int32_t *dst = (int32_t *)attr->data;
		if (int32Values) memcpy(dst, int32Values, sizeof(int32_t) * count * dim);
		else {
			for (CXSize i = 0; i < count * dim; i++) dst[i] = (int32_t)int64Values[i];
		}
	} else if (attrType == CXBigIntegerAttributeType) memcpy(attr->data, int64Values, sizeof(int64_t) * count * dim);
	else memcpy(attr->data, doubleValues, sizeof(double) * count * dim);
	free(boolValues); free(int32Values); free(int64Values); free(doubleValues);
	return CXTrue;
fail:
	free(boolValues); free(int32Values); free(int64Values); free(doubleValues);
	return CXFalse;
}

static CXBool GTReadStringVectorProperty(GTReader *reader, CXNetworkRef network, uint8_t propType, const char *name, CXSize count) {
	uint64_t dimension = UINT64_MAX;
	CXBool consistent = CXTrue;
	char **values = NULL;
	for (CXSize i = 0; i < count; i++) {
		uint64_t length = 0;
		if (!GTReadU64(reader, &length)) goto fail;
		if (dimension == UINT64_MAX) {
			dimension = length;
			size_t total = (size_t)dimension * (size_t)count;
			if (dimension > SIZE_MAX || (dimension != 0 && total / (size_t)dimension != (size_t)count)) goto fail;
			values = total > 0 ? calloc(total, sizeof(char *)) : NULL;
			if (total > 0 && !values) goto fail;
		} else if (length != dimension) {
			consistent = CXFalse;
		}
		for (uint64_t d = 0; d < length; d++) {
			char *value = GTReadString(reader);
			if (!value) goto fail;
			if (consistent && d < dimension && values) values[(size_t)i * (size_t)dimension + (size_t)d] = value;
			else free(value);
		}
	}
	if (!consistent) {
		CXNetworkSerializationWarningAppend("GT skipped %s property \"%s\" with inconsistent vector lengths", GTPropTypeLabel(propType), name);
		goto cleanup_success;
	}
	if (dimension == UINT64_MAX || dimension == 0) {
		CXNetworkSerializationWarningAppend("GT skipped %s property \"%s\" with empty vector values", GTPropTypeLabel(propType), name);
		goto cleanup_success;
	}
	CXSize dim = (CXSize)dimension;
	if (!GTDefineAttribute(network, propType, name, CXStringAttributeType, dim)) goto fail;
	CXAttributeRef attr = GTGetAttribute(network, propType, name);
	if (!attr || !attr->data) goto fail;
	memcpy(attr->data, values, sizeof(char *) * count * dim);
	free(values);
	return CXTrue;
cleanup_success:
	if (values) {
		size_t total = dimension == UINT64_MAX ? 0 : (size_t)dimension * (size_t)count;
		for (size_t i = 0; i < total; i++) free(values[i]);
	}
	free(values);
	return CXTrue;
fail:
	if (values) {
		size_t total = dimension == UINT64_MAX ? 0 : (size_t)dimension * (size_t)count;
		for (size_t i = 0; i < total; i++) free(values[i]);
	}
	free(values);
	return CXFalse;
}

static CXBool GTReadProperty(GTReader *reader, CXNetworkRef network, uint8_t propType, CXSize count) {
	char *name = GTReadString(reader);
	if (!name) {
		return CXFalse;
	}
	uint8_t valueType = 0;
	if (!GTReadU8(reader, &valueType)) {
		free(name);
		return CXFalse;
	}
	CXBool ok = CXFalse;
	switch (valueType) {
		case GT_VALUE_BOOL:
		case GT_VALUE_INT16:
		case GT_VALUE_INT32:
		case GT_VALUE_INT64:
		case GT_VALUE_DOUBLE:
		case GT_VALUE_STRING:
			ok = GTReadScalarProperty(reader, network, propType, name, valueType, count);
			break;
		case GT_VALUE_VECTOR_BOOL:
		case GT_VALUE_VECTOR_INT16:
		case GT_VALUE_VECTOR_INT32:
		case GT_VALUE_VECTOR_INT64:
		case GT_VALUE_VECTOR_DOUBLE:
			ok = GTReadNumericVectorProperty(reader, network, propType, name, valueType, count);
			break;
		case GT_VALUE_VECTOR_STRING:
			ok = GTReadStringVectorProperty(reader, network, propType, name, count);
			break;
		case GT_VALUE_LONG_DOUBLE:
		case GT_VALUE_VECTOR_LONG_DOUBLE:
		case GT_VALUE_PYTHON_OBJECT:
			CXNetworkSerializationWarningAppend("GT skipped unsupported %s property \"%s\"", GTPropTypeLabel(propType), name);
			ok = GTSkipPropertyValues(reader, valueType, count);
			break;
		default:
			ok = CXFalse;
			break;
	}
	free(name);
	return ok;
}

struct CXNetwork* CXNetworkReadGT(const char *path) {
	CXNetworkSerializationWarningClear();
	if (!path) {
		return NULL;
	}
	CXZstdInputStream *stream = CXZstdInputStreamOpen(path);
	if (!stream) {
		return NULL;
	}
	GTReader reader = { stream, CXFalse };
	uint8_t magic[sizeof(GT_MAGIC)] = { 0 };
	uint8_t version = 0;
	uint8_t bigEndian = 0;
	CXNetworkRef network = NULL;
	GTEdgeArray edges = { 0 };
	char *comment = NULL;
	if (!GTReadRaw(&reader, magic, sizeof(GT_MAGIC)) || memcmp(magic, GT_MAGIC, sizeof(GT_MAGIC)) != 0) goto fail;
	if (!GTReadU8(&reader, &version) || version != 1) goto fail;
	if (!GTReadU8(&reader, &bigEndian)) goto fail;
	reader.bigEndian = bigEndian ? CXTrue : CXFalse;
	comment = GTReadString(&reader);
	if (!comment) goto fail;
	free(comment);
	comment = NULL;
	uint8_t directed = 0;
	uint64_t nodeCount64 = 0;
	if (!GTReadU8(&reader, &directed) || !GTReadU64(&reader, &nodeCount64)) goto fail;
	if (nodeCount64 > (uint64_t)SIZE_MAX) goto fail;
	CXSize nodeCount = (CXSize)nodeCount64;
	network = CXNewNetworkWithCapacity(directed ? CXTrue : CXFalse, nodeCount, 256);
	if (!network) goto fail;
	if (nodeCount > 0 && !CXNetworkAddNodes(network, nodeCount, NULL)) goto fail;
	uint8_t width = GTNodeIndexWidth(nodeCount64);
	for (CXSize v = 0; v < nodeCount; v++) {
		uint64_t degree = 0;
		if (!GTReadU64(&reader, &degree)) goto fail;
		for (uint64_t i = 0; i < degree; i++) {
			uint64_t target = 0;
			if (!GTReadNodeIndex(&reader, width, &target) || target >= nodeCount64) goto fail;
			CXEdge edge = { (CXUInteger)v, (CXUInteger)target };
			if (!GTEdgeArrayAppend(&edges, edge)) goto fail;
		}
	}
	if (edges.count > 0 && !CXNetworkAddEdges(network, edges.items, edges.count, NULL)) goto fail;
	uint64_t propCount = 0;
	if (!GTReadU64(&reader, &propCount)) goto fail;
	for (uint64_t i = 0; i < propCount; i++) {
		uint8_t propType = 0;
		if (!GTReadU8(&reader, &propType)) goto fail;
		CXSize valueCount = 0;
		if (propType == GT_PROP_GRAPH) valueCount = 1;
		else if (propType == GT_PROP_VERTEX) valueCount = network->nodeCount;
		else if (propType == GT_PROP_EDGE) valueCount = network->edgeCount;
		else goto fail;
		if (!GTReadProperty(&reader, network, propType, valueCount)) goto fail;
	}
	free(edges.items);
	CXZstdInputStreamClose(stream);
	return network;
fail:
	free(comment);
	free(edges.items);
	if (network) {
		CXFreeNetwork(network);
	}
	CXZstdInputStreamClose(stream);
	return NULL;
}

static const char* GTCategoryLabelForId(CXAttributeRef attr, int32_t id) {
	if (!attr || !attr->categoricalDictionary) {
		return NULL;
	}
	CXStringDictionaryFOR(entry, attr->categoricalDictionary) {
		uintptr_t raw = (uintptr_t)entry->data;
		int32_t entryId = 0;
		if (raw == 1u) entryId = -1;
		else if (raw > 1u) entryId = (int32_t)(uint32_t)(raw - 2u);
		else continue;
		if (entryId == id) {
			return entry->key;
		}
	}
	return NULL;
}

static CXBool GTAttributeCanWrite(CXAttributeRef attr) {
	if (!attr) return CXFalse;
	switch (attr->type) {
		case CXBooleanAttributeType:
		case CXFloatAttributeType:
		case CXIntegerAttributeType:
		case CXUnsignedIntegerAttributeType:
		case CXDoubleAttributeType:
		case CXBigIntegerAttributeType:
		case CXStringAttributeType:
		case CXDataAttributeCategoryType:
			return CXTrue;
		case CXDataAttributeMultiCategoryType:
			return attr->multiCategory && !attr->multiCategory->hasWeights;
		default:
			return CXFalse;
	}
}

static CXBool GTAttributeNameIsHeliosPrivate(const char *name) {
	return name && strncmp(name, "_helios_", 8) == 0;
}

static uint8_t GTValueTypeForAttribute(CXAttributeRef attr) {
	if (attr->type == CXBooleanAttributeType) return attr->dimension == 1 ? GT_VALUE_BOOL : GT_VALUE_VECTOR_BOOL;
	if (attr->type == CXIntegerAttributeType) return attr->dimension == 1 ? GT_VALUE_INT32 : GT_VALUE_VECTOR_INT32;
	if (attr->type == CXUnsignedIntegerAttributeType || attr->type == CXBigIntegerAttributeType) return attr->dimension == 1 ? GT_VALUE_INT64 : GT_VALUE_VECTOR_INT64;
	if (attr->type == CXFloatAttributeType || attr->type == CXDoubleAttributeType) return attr->dimension == 1 ? GT_VALUE_DOUBLE : GT_VALUE_VECTOR_DOUBLE;
	if (attr->type == CXStringAttributeType || attr->type == CXDataAttributeCategoryType) return attr->dimension == 1 ? GT_VALUE_STRING : GT_VALUE_VECTOR_STRING;
	if (attr->type == CXDataAttributeMultiCategoryType) return GT_VALUE_VECTOR_STRING;
	return GT_VALUE_PYTHON_OBJECT;
}

static CXSize GTWritableAttributeCount(CXStringDictionaryRef dict, const char *scopeLabel) {
	CXSize count = 0;
	CXStringDictionaryFOR(entry, dict) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		if (GTAttributeNameIsHeliosPrivate(entry->key)) {
			CXNetworkSerializationWarningAppend("GT skipped Helios-private %s attribute \"%s\"", scopeLabel, entry->key);
			continue;
		}
		if (GTAttributeCanWrite(attr)) {
			count++;
		} else {
			CXNetworkSerializationWarningAppend("GT skipped unsupported %s attribute \"%s\"", scopeLabel, entry->key);
		}
	}
	return count;
}

static CXBool GTWriteAttributeValue(FILE *file, CXAttributeRef attr, CXIndex index) {
	if (attr->type == CXBooleanAttributeType) {
		uint8_t *values = (uint8_t *)attr->data;
		if (attr->dimension == 1) return GTWriteU8(file, values[index] != 0 ? 1 : 0);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteU8(file, values[(size_t)index * attr->dimension + d] != 0 ? 1 : 0)) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXIntegerAttributeType) {
		int32_t *values = (int32_t *)attr->data;
		if (attr->dimension == 1) return GTWriteI32(file, values[index]);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteI32(file, values[(size_t)index * attr->dimension + d])) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXUnsignedIntegerAttributeType) {
		uint32_t *values = (uint32_t *)attr->data;
		if (attr->dimension == 1) return GTWriteI64(file, (int64_t)values[index]);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteI64(file, (int64_t)values[(size_t)index * attr->dimension + d])) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXBigIntegerAttributeType) {
		int64_t *values = (int64_t *)attr->data;
		if (attr->dimension == 1) return GTWriteI64(file, values[index]);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteI64(file, values[(size_t)index * attr->dimension + d])) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXFloatAttributeType) {
		float *values = (float *)attr->data;
		if (attr->dimension == 1) return GTWriteDouble(file, (double)values[index]);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteDouble(file, (double)values[(size_t)index * attr->dimension + d])) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXDoubleAttributeType) {
		double *values = (double *)attr->data;
		if (attr->dimension == 1) return GTWriteDouble(file, values[index]);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteDouble(file, values[(size_t)index * attr->dimension + d])) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXStringAttributeType) {
		char **values = (char **)attr->data;
		if (attr->dimension == 1) return GTWriteString(file, values[index]);
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			if (!GTWriteString(file, values[(size_t)index * attr->dimension + d])) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXDataAttributeCategoryType) {
		int32_t *values = (int32_t *)attr->data;
		if (attr->dimension == 1) {
			const char *label = GTCategoryLabelForId(attr, values[index]);
			return GTWriteString(file, label ? label : "");
		}
		if (!GTWriteU64(file, attr->dimension)) return CXFalse;
		for (CXSize d = 0; d < attr->dimension; d++) {
			const char *label = GTCategoryLabelForId(attr, values[(size_t)index * attr->dimension + d]);
			if (!GTWriteString(file, label ? label : "")) return CXFalse;
		}
		return CXTrue;
	}
	if (attr->type == CXDataAttributeMultiCategoryType) {
		if (!attr->multiCategory || attr->multiCategory->hasWeights) return CXFalse;
		uint32_t start = attr->multiCategory->offsets[index];
		uint32_t end = attr->multiCategory->offsets[index + 1];
		if (!GTWriteU64(file, (uint64_t)(end - start))) return CXFalse;
		for (uint32_t i = start; i < end; i++) {
			const char *label = GTCategoryLabelForId(attr, (int32_t)attr->multiCategory->ids[i]);
			if (!GTWriteString(file, label ? label : "")) return CXFalse;
		}
		return CXTrue;
	}
	return CXFalse;
}

static CXBool GTWriteProperty(FILE *file, uint8_t propType, const char *name, CXAttributeRef attr, const CXIndex *order, CXSize count) {
	if (!GTWriteU8(file, propType) || !GTWriteString(file, name) || !GTWriteU8(file, GTValueTypeForAttribute(attr))) {
		return CXFalse;
	}
	if (propType == GT_PROP_GRAPH) {
		return GTWriteAttributeValue(file, attr, 0);
	}
	for (CXSize i = 0; i < count; i++) {
		if (!GTWriteAttributeValue(file, attr, order[i])) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool GTBuildWriteOrder(CXNetworkRef network, CXIndex **outNodes, CXSize *outNodeCount, CXIndex **outEdges, CXSize *outEdgeCount, CXIndex **outMap) {
	CXIndex *nodes = network->nodeCount > 0 ? calloc(network->nodeCount, sizeof(CXIndex)) : NULL;
	CXIndex *map = network->nodeCapacity > 0 ? malloc(sizeof(CXIndex) * network->nodeCapacity) : NULL;
	if ((network->nodeCount > 0 && !nodes) || (network->nodeCapacity > 0 && !map)) goto fail;
	for (CXSize i = 0; i < network->nodeCapacity; i++) map[i] = (CXIndex)CXIndexMAX;
	CXSize nodeCount = 0;
	for (CXSize i = 0; i < network->nodeCapacity; i++) {
		if (network->nodeActive && network->nodeActive[i]) {
			nodes[nodeCount] = (CXIndex)i;
			map[i] = nodeCount;
			nodeCount++;
		}
	}
	CXSize *degrees = nodeCount > 0 ? calloc(nodeCount, sizeof(CXSize)) : NULL;
	if (nodeCount > 0 && !degrees) goto fail;
	CXSize edgeCount = 0;
	for (CXSize i = 0; i < network->edgeCapacity; i++) {
		if (!network->edgeActive || !network->edgeActive[i]) continue;
		CXEdge edge = network->edges[i];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) continue;
		if (map[edge.from] == (CXIndex)CXIndexMAX || map[edge.to] == (CXIndex)CXIndexMAX) continue;
		degrees[map[edge.from]]++;
		edgeCount++;
	}
	CXIndex *edges = edgeCount > 0 ? calloc(edgeCount, sizeof(CXIndex)) : NULL;
	CXSize *offsets = nodeCount + 1 > 0 ? calloc(nodeCount + 1, sizeof(CXSize)) : NULL;
	CXSize *cursor = nodeCount > 0 ? calloc(nodeCount, sizeof(CXSize)) : NULL;
	if ((edgeCount > 0 && !edges) || !offsets || (nodeCount > 0 && !cursor)) goto fail_order;
	for (CXSize i = 0; i < nodeCount; i++) offsets[i + 1] = offsets[i] + degrees[i];
	memcpy(cursor, offsets, sizeof(CXSize) * nodeCount);
	for (CXSize i = 0; i < network->edgeCapacity; i++) {
		if (!network->edgeActive || !network->edgeActive[i]) continue;
		CXEdge edge = network->edges[i];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) continue;
		if (map[edge.from] == (CXIndex)CXIndexMAX || map[edge.to] == (CXIndex)CXIndexMAX) continue;
		CXIndex from = map[edge.from];
		edges[cursor[from]++] = (CXIndex)i;
	}
	free(degrees);
	free(offsets);
	free(cursor);
	*outNodes = nodes;
	*outNodeCount = nodeCount;
	*outEdges = edges;
	*outEdgeCount = edgeCount;
	*outMap = map;
	return CXTrue;
fail_order:
	free(degrees);
	free(edges);
	free(offsets);
	free(cursor);
fail:
	free(nodes);
	free(map);
	return CXFalse;
}

CXBool CXNetworkWriteGT(CXNetworkRef network, const char *path) {
	CXNetworkSerializationWarningClear();
	if (!network || !path) {
		return CXFalse;
	}
	FILE *file = fopen(path, "wb");
	if (!file) {
		return CXFalse;
	}
	CXIndex *nodeOrder = NULL;
	CXIndex *edgeOrder = NULL;
	CXIndex *nodeMap = NULL;
	CXSize nodeCount = 0;
	CXSize edgeCount = 0;
	CXBool ok = CXFalse;
	if (!GTBuildWriteOrder(network, &nodeOrder, &nodeCount, &edgeOrder, &edgeCount, &nodeMap)) goto cleanup;
	uint64_t propCount = 0;
	propCount += GTWritableAttributeCount(network->networkAttributes, "graph");
	propCount += GTWritableAttributeCount(network->nodeAttributes, "vertex");
	propCount += GTWritableAttributeCount(network->edgeAttributes, "edge");
	if (!GTWriteRaw(file, GT_MAGIC, sizeof(GT_MAGIC))) goto cleanup;
	if (!GTWriteU8(file, 1) || !GTWriteU8(file, 0)) goto cleanup;
	char *comment = CXNewStringFromFormat(
		"Helios graph-tool binary file stats: %" PRIu64 " vertices, %" PRIu64 " edges, %s, %" PRIu64 " props",
		(uint64_t)nodeCount,
		(uint64_t)edgeCount,
		network->isDirected ? "directed" : "undirected",
		propCount
	);
	if (!comment) goto cleanup;
	CXBool wroteComment = GTWriteString(file, comment);
	free(comment);
	if (!wroteComment) goto cleanup;
	if (!GTWriteU8(file, network->isDirected ? 1 : 0) || !GTWriteU64(file, (uint64_t)nodeCount)) goto cleanup;
	uint8_t width = GTNodeIndexWidth((uint64_t)nodeCount);
	CXSize edgeCursor = 0;
	for (CXSize n = 0; n < nodeCount; n++) {
		CXIndex oldNode = nodeOrder[n];
		CXSize degree = 0;
		for (CXSize i = edgeCursor; i < edgeCount; i++) {
			CXEdge edge = network->edges[edgeOrder[i]];
			if (edge.from != oldNode) break;
			degree++;
		}
		if (!GTWriteU64(file, (uint64_t)degree)) goto cleanup;
		for (CXSize i = 0; i < degree; i++) {
			CXEdge edge = network->edges[edgeOrder[edgeCursor++]];
			if (!GTWriteNodeIndex(file, width, nodeMap[edge.to])) goto cleanup;
		}
	}
	if (!GTWriteU64(file, propCount)) goto cleanup;
	CXStringDictionaryFOR(gEntry, network->networkAttributes) {
		CXAttributeRef attr = (CXAttributeRef)gEntry->data;
		if (!GTAttributeNameIsHeliosPrivate(gEntry->key) && GTAttributeCanWrite(attr) && !GTWriteProperty(file, GT_PROP_GRAPH, gEntry->key, attr, NULL, 1)) goto cleanup;
	}
	CXStringDictionaryFOR(vEntry, network->nodeAttributes) {
		CXAttributeRef attr = (CXAttributeRef)vEntry->data;
		if (!GTAttributeNameIsHeliosPrivate(vEntry->key) && GTAttributeCanWrite(attr) && !GTWriteProperty(file, GT_PROP_VERTEX, vEntry->key, attr, nodeOrder, nodeCount)) goto cleanup;
	}
	CXStringDictionaryFOR(eEntry, network->edgeAttributes) {
		CXAttributeRef attr = (CXAttributeRef)eEntry->data;
		if (!GTAttributeNameIsHeliosPrivate(eEntry->key) && GTAttributeCanWrite(attr) && !GTWriteProperty(file, GT_PROP_EDGE, eEntry->key, attr, edgeOrder, edgeCount)) goto cleanup;
	}
	ok = CXTrue;
cleanup:
	free(nodeOrder);
	free(edgeOrder);
	free(nodeMap);
	if (fclose(file) != 0) {
		ok = CXFalse;
	}
	return ok;
}
