#include "CXNetwork.h"

// Internal helpers -----------------------------------------------------------

static const CXIndex CXInvalidIndexValue = CXIndexMAX;
static const char kCXNetworkVersionString[] = CXNETWORK_VERSION_STRING;

const char* CXNetworkVersionString(void) {
	return kCXNetworkVersionString;
}

static CXAttributeRef CXAttributeCreate(CXAttributeType type, CXSize dimension, CXSize capacity);
static void CXAttributeDestroy(CXAttributeRef attribute);
static CXBool CXAttributeEnsureCapacity(CXAttributeRef attribute, CXSize requiredCapacity);
static void CXAttributeClearSlot(CXAttributeRef attribute, CXIndex index);

static void CXDestroyAttributeDictionary(CXStringDictionaryRef dictionary);

static CXBool CXNetworkEnsureNodeCapacity(CXNetworkRef network, CXSize required);
static CXBool CXNetworkEnsureEdgeCapacity(CXNetworkRef network, CXSize required);
static void CXNetworkResetNodeRecord(CXNetworkRef network, CXIndex node);
static void CXNetworkResetEdgeRecord(CXNetworkRef network, CXIndex edge);
static void CXNetworkDetachEdge(CXNetworkRef network, CXIndex edge, CXBool recycleIndex);

static void CXNetworkMarkNodesDirty(CXNetworkRef network);
static void CXNetworkMarkEdgesDirty(CXNetworkRef network);

// Dense buffer helpers
static CXDenseAttributeBuffer* CXNetworkFindDenseBuffer(CXDenseAttributeBuffer *buffers, CXSize count, const CXString name);
static CXBool CXNetworkEnsureDenseListCapacity(CXDenseAttributeBuffer **buffers, CXSize *capacity, CXSize required);
static CXBool CXNetworkAddDenseBuffer(CXDenseAttributeBuffer **buffers, CXSize *count, CXSize *capacity, const CXString name, CXSize initialCapacity, CXBool isIndex);
static void CXNetworkFreeDenseBuffer(CXDenseAttributeBuffer *buffer);
static void CXNetworkFreeDenseLists(CXNetworkRef network);
static CXBool CXNetworkMarkDenseBuffersDirty(CXDenseAttributeBuffer *buffers, CXSize count);
static CXBool CXNetworkPackDenseAttribute(CXNetworkRef network, CXDenseAttributeBuffer *buffer, CXAttributeRef attribute, const CXIndex *order, CXSize orderCount, const CXBool *activity, CXSize capacityLimit);
static CXBool CXNetworkPackDenseIndex(CXDenseAttributeBuffer *buffer, const CXIndex *order, CXSize orderCount, const CXBool *activity, CXSize capacityLimit);
static CXBool CXNetworkSetDenseOrder(CXIndex **target, CXSize *count, CXSize *capacity, const CXIndex *order, CXSize orderCount);
static CXBool CXNetworkRecomputeValidRange(const CXBool *activity, CXSize capacity, CXSize *start, CXSize *end);
static CXBool CXNetworkRecomputeValidRange(const CXBool *activity, CXSize capacity, CXSize *start, CXSize *end);
static CXBool CXNetworkRemoveAttributeInternal(CXStringDictionaryRef dict, const CXString name);
static CXDenseColorEncodedAttribute* CXNetworkFindDenseColorAttribute(CXDenseColorEncodedAttribute *attributes, CXSize count, const CXString encodedName);
static CXBool CXNetworkEnsureDenseColorListCapacity(CXDenseColorEncodedAttribute **attributes, CXSize *capacity, CXSize required);
static CXBool CXNetworkAddDenseColorAttribute(CXDenseColorEncodedAttribute **attributes, CXSize *count, CXSize *capacity, const CXString sourceName, const CXString encodedName, CXDenseColorFormat format, CXBool useIndexSource);
static void CXNetworkFreeDenseColorAttribute(CXDenseColorEncodedAttribute *attribute);
static void CXNetworkFreeDenseColorLists(CXNetworkRef network);
static void CXNetworkMarkDenseColorBuffersDirty(CXDenseColorEncodedAttribute *attributes, CXSize count);
static void CXNetworkMarkDenseColorBuffersDirtyForSource(CXDenseColorEncodedAttribute *attributes, CXSize count, const CXString sourceName);
static uint32_t CXNetworkEncodeColorValueFromAttribute(CXAttributeRef attribute, CXIndex index);
static CXBool CXNetworkPackDenseColorEncoded(CXDenseColorEncodedAttribute *attribute, const CXIndex *order, CXSize orderCount, const CXBool *activity, CXSize capacityLimit, CXAttributeRef sourceAttribute);

static CXSelector* CXSelectorCreateInternal(CXSize initialCapacity);
static void CXSelectorDestroyInternal(CXSelector *selector);
static CXBool CXSelectorEnsureCapacity(CXSelector *selector, CXSize capacity);

// -----------------------------------------------------------------------------
// Attribute helpers
// -----------------------------------------------------------------------------

static CXBool CXAttributeComputeLayout(
	CXAttributeType type,
	CXSize dimension,
	CXSize *elementSize,
	CXSize *stride,
	CXBool *usesJavascriptShadow
) {
	if (dimension == 0) {
		dimension = 1;
	}
	CXSize baseSize = 0;
	CXBool requiresShadow = CXFalse;
	switch (type) {
		case CXStringAttributeType:
			baseSize = sizeof(CXString);
			break;
		case CXBooleanAttributeType:
			baseSize = sizeof(uint8_t);
			break;
		case CXFloatAttributeType:
			baseSize = sizeof(float);
			break;
		case CXIntegerAttributeType:
			baseSize = sizeof(int64_t);
			break;
		case CXUnsignedIntegerAttributeType:
			baseSize = sizeof(uint64_t);
			break;
		case CXDoubleAttributeType:
			baseSize = sizeof(double);
			break;
		case CXDataAttributeCategoryType:
			baseSize = sizeof(uint32_t);
			break;
		case CXDataAttributeType:
			baseSize = sizeof(uintptr_t);
			break;
		case CXJavascriptAttributeType:
			baseSize = sizeof(uint32_t);
			requiresShadow = CXTrue;
			break;
		default:
			return CXFalse;
	}
	if (elementSize) {
		*elementSize = baseSize;
	}
	if (stride) {
		*stride = baseSize * dimension;
	}
	if (usesJavascriptShadow) {
		*usesJavascriptShadow = requiresShadow;
	}
	return CXTrue;
}

static CXAttributeRef CXAttributeCreate(CXAttributeType type, CXSize dimension, CXSize capacity) {
	CXSize elementSize = 0;
	CXSize stride = 0;
	CXBool usesJavascriptShadow = CXFalse;
	if (!CXAttributeComputeLayout(type, dimension, &elementSize, &stride, &usesJavascriptShadow)) {
		return NULL;
	}

	CXAttributeRef attribute = calloc(1, sizeof(CXAttribute));
	if (!attribute) {
		return NULL;
	}
	attribute->type = type;
	attribute->dimension = dimension == 0 ? 1 : dimension;
	attribute->elementSize = elementSize;
	attribute->stride = stride;
	attribute->usesJavascriptShadow = usesJavascriptShadow;

	if (type == CXDataAttributeCategoryType) {
		attribute->categoricalDictionary = CXNewStringDictionary();
	}

	if (capacity > 0) {
		attribute->data = calloc(capacity, stride);
		if (!attribute->data) {
			CXAttributeDestroy(attribute);
			return NULL;
		}
		attribute->capacity = capacity;
	}
	return attribute;
}

static void CXAttributeDestroy(CXAttributeRef attribute) {
	if (!attribute) {
		return;
	}
	if (attribute->data) {
		free(attribute->data);
		attribute->data = NULL;
	}
	if (attribute->categoricalDictionary) {
		CXStringDictionaryDestroy(attribute->categoricalDictionary);
	}
	free(attribute);
}

/** Ensures an attribute buffer has room for the requested number of entries. */
static CXBool CXAttributeEnsureCapacity(CXAttributeRef attribute, CXSize requiredCapacity) {
	if (!attribute) {
		return CXFalse;
	}
	if (requiredCapacity <= attribute->capacity) {
		return CXTrue;
	}
	CXSize newCapacity = attribute->capacity > 0 ? attribute->capacity : 4;
	while (newCapacity < requiredCapacity) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < requiredCapacity) {
			newCapacity = requiredCapacity;
			break;
		}
	}
	uint8_t *newData = realloc(attribute->data, newCapacity * attribute->stride);
	if (!newData) {
		return CXFalse;
	}
	size_t oldBytes = (size_t)attribute->capacity * attribute->stride;
	size_t newBytes = (size_t)newCapacity * attribute->stride;
	if (newBytes > oldBytes) {
		memset(newData + oldBytes, 0, newBytes - oldBytes);
	}
	attribute->data = newData;
	attribute->capacity = newCapacity;
	return CXTrue;
}

/** Zeroes the attribute payload for a single logical index. */
static void CXAttributeClearSlot(CXAttributeRef attribute, CXIndex index) {
	if (!attribute || !attribute->data || index >= attribute->capacity) {
		return;
	}
	memset(attribute->data + ((size_t)index * attribute->stride), 0, attribute->stride);
}

// -----------------------------------------------------------------------------
// Dictionary helper
// -----------------------------------------------------------------------------

/** Helper that destroys every attribute stored in the provided dictionary. */
static void CXDestroyAttributeDictionary(CXStringDictionaryRef dictionary) {
	if (!dictionary) {
		return;
	}
	CXStringDictionaryEntry *entry, *tmp;
	HASH_ITER(hh, (*dictionary), entry, tmp) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		CXAttributeDestroy(attribute);
	}
	CXStringDictionaryClear(dictionary);
	CXStringDictionaryDestroy(dictionary);
}

// -----------------------------------------------------------------------------
// Activity markers
// -----------------------------------------------------------------------------

/** Marks node-derived caches dirty. */
static void CXNetworkMarkNodesDirty(CXNetworkRef network) {
	if (!network) {
		return;
	}
	CXNetworkMarkDenseColorBuffersDirty(network->nodeColorAttributes, network->nodeColorAttributeCount);
}

/** Marks edge-derived caches dirty. */
static void CXNetworkMarkEdgesDirty(CXNetworkRef network) {
	if (!network) {
		return;
	}
	CXNetworkMarkDenseColorBuffersDirty(network->edgeColorAttributes, network->edgeColorAttributeCount);
}

// -----------------------------------------------------------------------------
// Dense buffers
// -----------------------------------------------------------------------------

static CXDenseAttributeBuffer* CXNetworkFindDenseBuffer(CXDenseAttributeBuffer *buffers, CXSize count, const CXString name) {
	if (!buffers || !name) {
		return NULL;
	}
	for (CXSize i = 0; i < count; i++) {
		if (buffers[i].name && strcmp(buffers[i].name, name) == 0) {
			return &buffers[i];
		}
	}
	return NULL;
}

static CXBool CXNetworkEnsureDenseListCapacity(CXDenseAttributeBuffer **buffers, CXSize *capacity, CXSize required) {
	if (!buffers || !capacity) {
		return CXFalse;
	}
	if (required <= *capacity) {
		return CXTrue;
	}
	CXSize newCapacity = *capacity > 0 ? *capacity : 2;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}
	CXDenseAttributeBuffer *newList = realloc(*buffers, sizeof(CXDenseAttributeBuffer) * newCapacity);
	if (!newList) {
		return CXFalse;
	}
	for (CXSize i = *capacity; i < newCapacity; i++) {
		memset(&newList[i], 0, sizeof(CXDenseAttributeBuffer));
	}
	*buffers = newList;
	*capacity = newCapacity;
	return CXTrue;
}

static CXBool CXNetworkAddDenseBuffer(CXDenseAttributeBuffer **buffers, CXSize *count, CXSize *capacity, const CXString name, CXSize initialCapacity, CXBool isIndex) {
	if (!buffers || !count || !capacity || !name) {
		return CXFalse;
	}
	if (!CXNetworkEnsureDenseListCapacity(buffers, capacity, *count + 1)) {
		return CXFalse;
	}
	CXDenseAttributeBuffer *buf = &(*buffers)[*count];
	memset(buf, 0, sizeof(CXDenseAttributeBuffer));
	buf->name = strdup(name);
	if (!buf->name) {
		return CXFalse;
	}
	buf->stride = 0;
	buf->capacity = initialCapacity;
	if (initialCapacity > 0) {
		buf->data = calloc(initialCapacity, 1);
		if (!buf->data) {
			free(buf->name);
			buf->name = NULL;
			return CXFalse;
		}
	}
	buf->dirty = CXTrue;
	buf->isIndexBuffer = isIndex;
	(*count)++;
	return CXTrue;
}

static void CXNetworkFreeDenseBuffer(CXDenseAttributeBuffer *buffer) {
	if (!buffer) {
		return;
	}
	if (buffer->name) {
		free(buffer->name);
		buffer->name = NULL;
	}
	if (buffer->data) {
		free(buffer->data);
		buffer->data = NULL;
	}
	buffer->count = 0;
	buffer->capacity = 0;
	buffer->stride = 0;
	buffer->validStart = 0;
	buffer->validEnd = 0;
	buffer->dirty = CXFalse;
	buffer->isIndexBuffer = CXFalse;
}

static void CXNetworkFreeDenseLists(CXNetworkRef network) {
	if (!network) {
		return;
	}
	for (CXSize i = 0; i < network->nodeDenseBufferCount; i++) {
		CXNetworkFreeDenseBuffer(&network->nodeDenseBuffers[i]);
	}
	free(network->nodeDenseBuffers);
	network->nodeDenseBuffers = NULL;
	network->nodeDenseBufferCount = 0;
	network->nodeDenseBufferCapacity = 0;
	for (CXSize i = 0; i < network->edgeDenseBufferCount; i++) {
		CXNetworkFreeDenseBuffer(&network->edgeDenseBuffers[i]);
	}
	free(network->edgeDenseBuffers);
	network->edgeDenseBuffers = NULL;
	network->edgeDenseBufferCount = 0;
	network->edgeDenseBufferCapacity = 0;
	CXNetworkFreeDenseBuffer(&network->nodeIndexDense);
	CXNetworkFreeDenseBuffer(&network->edgeIndexDense);
	if (network->nodeDenseOrder) {
		free(network->nodeDenseOrder);
		network->nodeDenseOrder = NULL;
		network->nodeDenseOrderCount = 0;
		network->nodeDenseOrderCapacity = 0;
	}
	if (network->edgeDenseOrder) {
		free(network->edgeDenseOrder);
		network->edgeDenseOrder = NULL;
		network->edgeDenseOrderCount = 0;
		network->edgeDenseOrderCapacity = 0;
	}
	CXNetworkFreeDenseColorLists(network);
}

static CXBool CXNetworkMarkDenseBuffersDirty(CXDenseAttributeBuffer *buffers, CXSize count) {
	for (CXSize i = 0; i < count; i++) {
		buffers[i].dirty = CXTrue;
	}
	return CXTrue;
}

static CXDenseColorEncodedAttribute* CXNetworkFindDenseColorAttribute(CXDenseColorEncodedAttribute *attributes, CXSize count, const CXString encodedName) {
	if (!attributes || !encodedName) {
		return NULL;
	}
	for (CXSize i = 0; i < count; i++) {
		if (attributes[i].encodedName && strcmp(attributes[i].encodedName, encodedName) == 0) {
			return &attributes[i];
		}
	}
	return NULL;
}

static CXBool CXNetworkEnsureDenseColorListCapacity(CXDenseColorEncodedAttribute **attributes, CXSize *capacity, CXSize required) {
	if (!attributes || !capacity) {
		return CXFalse;
	}
	if (required <= *capacity) {
		return CXTrue;
	}
	CXSize newCapacity = *capacity > 0 ? *capacity : 2;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}
	CXDenseColorEncodedAttribute *newList = realloc(*attributes, sizeof(CXDenseColorEncodedAttribute) * newCapacity);
	if (!newList) {
		return CXFalse;
	}
	for (CXSize i = *capacity; i < newCapacity; i++) {
		memset(&newList[i], 0, sizeof(CXDenseColorEncodedAttribute));
	}
	*attributes = newList;
	*capacity = newCapacity;
	return CXTrue;
}

static CXBool CXNetworkAddDenseColorAttribute(CXDenseColorEncodedAttribute **attributes, CXSize *count, CXSize *capacity, const CXString sourceName, const CXString encodedName, CXDenseColorFormat format, CXBool useIndexSource) {
	if (!attributes || !count || !capacity || !encodedName) {
		return CXFalse;
	}
	if (!CXNetworkEnsureDenseColorListCapacity(attributes, capacity, *count + 1)) {
		return CXFalse;
	}
	CXDenseColorEncodedAttribute *entry = &(*attributes)[*count];
	memset(entry, 0, sizeof(CXDenseColorEncodedAttribute));
	entry->encodedName = strdup(encodedName);
	entry->sourceName = sourceName ? strdup(sourceName) : NULL;
	if (!entry->encodedName || (!useIndexSource && !entry->sourceName)) {
		CXNetworkFreeDenseColorAttribute(entry);
		return CXFalse;
	}
	entry->format = format;
	entry->useIndexSource = useIndexSource;
	entry->buffer.dirty = CXTrue;
	entry->buffer.isIndexBuffer = CXFalse;
	(*count)++;
	return CXTrue;
}

static void CXNetworkFreeDenseColorAttribute(CXDenseColorEncodedAttribute *attribute) {
	if (!attribute) {
		return;
	}
	if (attribute->encodedName) {
		free(attribute->encodedName);
		attribute->encodedName = NULL;
	}
	if (attribute->sourceName) {
		free(attribute->sourceName);
		attribute->sourceName = NULL;
	}
	attribute->format = CXDenseColorFormatU8x4;
	attribute->useIndexSource = CXFalse;
	CXNetworkFreeDenseBuffer(&attribute->buffer);
}

static void CXNetworkFreeDenseColorLists(CXNetworkRef network) {
	if (!network) {
		return;
	}
	for (CXSize i = 0; i < network->nodeColorAttributeCount; i++) {
		CXNetworkFreeDenseColorAttribute(&network->nodeColorAttributes[i]);
	}
	free(network->nodeColorAttributes);
	network->nodeColorAttributes = NULL;
	network->nodeColorAttributeCount = 0;
	network->nodeColorAttributeCapacity = 0;

	for (CXSize i = 0; i < network->edgeColorAttributeCount; i++) {
		CXNetworkFreeDenseColorAttribute(&network->edgeColorAttributes[i]);
	}
	free(network->edgeColorAttributes);
	network->edgeColorAttributes = NULL;
	network->edgeColorAttributeCount = 0;
	network->edgeColorAttributeCapacity = 0;
}

static void CXNetworkMarkDenseColorBuffersDirty(CXDenseColorEncodedAttribute *attributes, CXSize count) {
	if (!attributes) {
		return;
	}
	for (CXSize i = 0; i < count; i++) {
		attributes[i].buffer.dirty = CXTrue;
	}
}

static void CXNetworkMarkDenseColorBuffersDirtyForSource(CXDenseColorEncodedAttribute *attributes, CXSize count, const CXString sourceName) {
	if (!attributes || !sourceName) {
		return;
	}
	for (CXSize i = 0; i < count; i++) {
		if (attributes[i].sourceName && strcmp(attributes[i].sourceName, sourceName) == 0) {
			attributes[i].buffer.dirty = CXTrue;
		}
	}
}

static uint32_t CXNetworkEncodeColorValueFromAttribute(CXAttributeRef attribute, CXIndex index) {
	if (!attribute || !attribute->data) {
		return 0;
	}
	const uint8_t *base = attribute->data + ((size_t)index * attribute->stride);
	if (attribute->type == CXIntegerAttributeType) {
		const int64_t *valuePtr = (const int64_t *)base;
		int64_t raw = valuePtr ? valuePtr[0] : 0;
		int64_t encoded = raw >= INT64_MAX - 1 ? INT64_MAX : raw + 1;
		if (encoded < 0) {
			return 0;
		}
		if ((uint64_t)encoded > UINT32_MAX) {
			return UINT32_MAX;
		}
		return (uint32_t)encoded;
	}
	if (attribute->type == CXUnsignedIntegerAttributeType) {
		const uint64_t *valuePtr = (const uint64_t *)base;
		uint64_t raw = valuePtr ? valuePtr[0] : 0;
		uint64_t encoded = raw + 1;
		if (encoded < raw) {
			encoded = UINT64_MAX;
		}
		if (encoded > UINT32_MAX) {
			return UINT32_MAX;
		}
		return (uint32_t)encoded;
	}
	return 0;
}

static CXBool CXNetworkPackDenseColorEncoded(
	CXDenseColorEncodedAttribute *attribute,
	const CXIndex *order,
	CXSize orderCount,
	const CXBool *activity,
	CXSize capacityLimit,
	CXAttributeRef sourceAttribute
) {
	if (!attribute) {
		return CXFalse;
	}
	CXDenseAttributeBuffer *buffer = &attribute->buffer;
	const CXBool useOrder = order && orderCount > 0;
	CXSize count = 0;
	if (useOrder) {
		for (CXSize i = 0; i < orderCount; i++) {
			CXIndex idx = order[i];
			if (idx < capacityLimit && (!activity || activity[idx])) {
				count++;
			}
		}
	} else {
		for (CXSize i = 0; i < capacityLimit; i++) {
			if (!activity || activity[i]) {
				count++;
			}
		}
	}
	const CXSize stride = attribute->format == CXDenseColorFormatU32x4 ? sizeof(uint32_t) * 4 : sizeof(uint8_t) * 4;
	if (count == 0) {
		buffer->count = 0;
		buffer->validStart = 0;
		buffer->validEnd = 0;
		buffer->stride = stride;
		buffer->dirty = CXFalse;
		return CXTrue;
	}
	buffer->stride = stride;
	const CXSize requiredBytes = count * buffer->stride;
	if (buffer->capacity < requiredBytes) {
		CXSize newCapacity = buffer->capacity > 0 ? buffer->capacity : requiredBytes;
		while (newCapacity < requiredBytes) {
			newCapacity = CXCapacityGrow(newCapacity);
			if (newCapacity < requiredBytes) {
				newCapacity = requiredBytes;
				break;
			}
		}
		uint8_t *newData = realloc(buffer->data, newCapacity);
		if (!newData) {
			return CXFalse;
		}
		buffer->data = newData;
		buffer->capacity = newCapacity;
	}

	CXSize written = 0;
	CXSize minIdx = CXIndexMAX;
	CXSize maxIdx = 0;
	const CXBool emitU32 = attribute->format == CXDenseColorFormatU32x4;
	if (useOrder) {
		for (CXSize i = 0; i < orderCount; i++) {
			CXIndex idx = order[i];
			if (idx >= capacityLimit || (activity && !activity[idx])) {
				continue;
			}
			uint32_t encoded = attribute->useIndexSource ? (uint32_t)((uint64_t)idx + 1) : CXNetworkEncodeColorValueFromAttribute(sourceAttribute, idx);
			uint8_t *dst = buffer->data + ((size_t)written * buffer->stride);
			if (emitU32) {
				uint32_t *dst32 = (uint32_t *)dst;
				dst32[0] = encoded & 0xFFu;
				dst32[1] = (encoded >> 8) & 0xFFu;
				dst32[2] = (encoded >> 16) & 0xFFu;
				dst32[3] = (encoded >> 24) & 0xFFu;
			} else {
				dst[0] = (uint8_t)(encoded & 0xFFu);
				dst[1] = (uint8_t)((encoded >> 8) & 0xFFu);
				dst[2] = (uint8_t)((encoded >> 16) & 0xFFu);
				dst[3] = (uint8_t)((encoded >> 24) & 0xFFu);
			}
			if (idx < minIdx) {
				minIdx = idx;
			}
			if (idx > maxIdx) {
				maxIdx = idx;
			}
			written++;
		}
	} else {
		for (CXSize idx = 0; idx < capacityLimit; idx++) {
			if (!activity || activity[idx]) {
				uint32_t encoded = attribute->useIndexSource ? (uint32_t)((uint64_t)idx + 1) : CXNetworkEncodeColorValueFromAttribute(sourceAttribute, idx);
				uint8_t *dst = buffer->data + ((size_t)written * buffer->stride);
				if (emitU32) {
					uint32_t *dst32 = (uint32_t *)dst;
					dst32[0] = encoded & 0xFFu;
					dst32[1] = (encoded >> 8) & 0xFFu;
					dst32[2] = (encoded >> 16) & 0xFFu;
					dst32[3] = (encoded >> 24) & 0xFFu;
				} else {
					dst[0] = (uint8_t)(encoded & 0xFFu);
					dst[1] = (uint8_t)((encoded >> 8) & 0xFFu);
					dst[2] = (uint8_t)((encoded >> 16) & 0xFFu);
					dst[3] = (uint8_t)((encoded >> 24) & 0xFFu);
				}
				if (idx < minIdx) {
					minIdx = idx;
				}
				if (idx > maxIdx) {
					maxIdx = idx;
				}
				written++;
			}
		}
	}
	buffer->count = written;
	if (written == 0) {
		buffer->validStart = 0;
		buffer->validEnd = 0;
	} else {
		buffer->validStart = minIdx;
		buffer->validEnd = maxIdx + 1;
	}
	buffer->dirty = CXFalse;
	return CXTrue;
}

static CXBool CXNetworkPackDenseAttribute(
	CXNetworkRef network,
	CXDenseAttributeBuffer *buffer,
	CXAttributeRef attribute,
	const CXIndex *order,
	CXSize orderCount,
	const CXBool *activity,
	CXSize capacityLimit
) {
	if (!network || !buffer || !attribute) {
		return CXFalse;
	}
	CXSize count = 0;
	if (order) {
		count = orderCount;
	} else {
		for (CXSize i = 0; i < capacityLimit; i++) {
			if (!activity || activity[i]) {
				count++;
			}
		}
	}
	if (count == 0) {
		buffer->count = 0;
		buffer->validStart = 0;
		buffer->validEnd = 0;
		buffer->stride = attribute->stride;
		buffer->dirty = CXFalse;
		return CXTrue;
	}
	buffer->stride = attribute->stride;
	if (buffer->capacity < count * buffer->stride) {
		CXSize newCapacity = buffer->capacity > 0 ? buffer->capacity : buffer->stride * count;
		while (newCapacity < count * buffer->stride) {
			newCapacity = CXCapacityGrow(newCapacity);
			if (newCapacity < count * buffer->stride) {
				newCapacity = count * buffer->stride;
				break;
			}
		}
		uint8_t *newData = realloc(buffer->data, newCapacity);
		if (!newData) {
			return CXFalse;
		}
		buffer->data = newData;
		buffer->capacity = newCapacity;
	}

	CXSize written = 0;
	CXSize minIdx = CXIndexMAX;
	CXSize maxIdx = 0;
	if (order) {
		for (CXSize i = 0; i < orderCount; i++) {
			CXIndex idx = order[i];
			if (idx >= capacityLimit || (activity && !activity[idx])) {
				continue;
			}
			memcpy(buffer->data + written * buffer->stride, attribute->data + (size_t)idx * attribute->stride, attribute->stride);
			if (idx < minIdx) {
				minIdx = idx;
			}
			if (idx > maxIdx) {
				maxIdx = idx;
			}
			written++;
		}
	} else {
		for (CXSize idx = 0; idx < capacityLimit; idx++) {
			if (!activity || activity[idx]) {
				memcpy(buffer->data + written * buffer->stride, attribute->data + (size_t)idx * attribute->stride, attribute->stride);
				if (idx < minIdx) {
					minIdx = idx;
				}
				if (idx > maxIdx) {
					maxIdx = idx;
				}
				written++;
			}
		}
	}
	buffer->count = written;
	if (written == 0) {
		buffer->validStart = 0;
		buffer->validEnd = 0;
	} else {
		buffer->validStart = minIdx;
		buffer->validEnd = maxIdx + 1;
	}
	buffer->dirty = CXFalse;
	return CXTrue;
}

static CXBool CXNetworkPackDenseIndex(
	CXDenseAttributeBuffer *buffer,
	const CXIndex *order,
	CXSize orderCount,
	const CXBool *activity,
	CXSize capacityLimit
) {
	if (!buffer) {
		return CXFalse;
	}
	CXSize count = 0;
	if (order) {
		count = orderCount;
	} else {
		for (CXSize i = 0; i < capacityLimit; i++) {
			if (!activity || activity[i]) {
				count++;
			}
		}
	}
	if (count == 0) {
		buffer->count = 0;
		buffer->validStart = 0;
		buffer->validEnd = 0;
		buffer->stride = sizeof(CXIndex);
		buffer->dirty = CXFalse;
		return CXTrue;
	}
	buffer->stride = sizeof(CXIndex);
	if (buffer->capacity < count * buffer->stride) {
		CXSize newCapacity = buffer->capacity > 0 ? buffer->capacity : buffer->stride * count;
		while (newCapacity < count * buffer->stride) {
			newCapacity = CXCapacityGrow(newCapacity);
			if (newCapacity < count * buffer->stride) {
				newCapacity = count * buffer->stride;
				break;
			}
		}
		uint8_t *newData = realloc(buffer->data, newCapacity);
		if (!newData) {
			return CXFalse;
		}
		buffer->data = newData;
		buffer->capacity = newCapacity;
	}
	CXIndex *out = (CXIndex *)buffer->data;
	CXSize written = 0;
	CXSize minIdx = CXIndexMAX;
	CXSize maxIdx = 0;
	if (order) {
		for (CXSize i = 0; i < orderCount; i++) {
			CXIndex idx = order[i];
			if (idx >= capacityLimit || (activity && !activity[idx])) {
				continue;
			}
			out[written++] = idx;
			if (idx < minIdx) {
				minIdx = idx;
			}
			if (idx > maxIdx) {
				maxIdx = idx;
			}
		}
	} else {
		for (CXSize idx = 0; idx < capacityLimit; idx++) {
			if (!activity || activity[idx]) {
				out[written++] = idx;
				if (idx < minIdx) {
					minIdx = idx;
				}
				if (idx > maxIdx) {
					maxIdx = idx;
				}
			}
		}
	}
	buffer->count = written;
	if (written == 0) {
		buffer->validStart = 0;
		buffer->validEnd = 0;
	} else {
		buffer->validStart = minIdx;
		buffer->validEnd = maxIdx + 1;
	}
	buffer->dirty = CXFalse;
	return CXTrue;
}

static CXBool CXNetworkSetDenseOrder(CXIndex **target, CXSize *count, CXSize *capacity, const CXIndex *order, CXSize orderCount) {
	if (!target || !count || !capacity) {
		return CXFalse;
	}
	if (!order || orderCount == 0) {
		free(*target);
		*target = NULL;
		*count = 0;
		*capacity = 0;
		return CXTrue;
	}
	if (*capacity < orderCount) {
		CXSize newCap = *capacity > 0 ? *capacity : orderCount;
		while (newCap < orderCount) {
			newCap = CXCapacityGrow(newCap);
			if (newCap < orderCount) {
				newCap = orderCount;
				break;
			}
		}
		CXIndex *newData = realloc(*target, sizeof(CXIndex) * newCap);
		if (!newData) {
			return CXFalse;
		}
		*target = newData;
		*capacity = newCap;
	}
	memcpy(*target, order, sizeof(CXIndex) * orderCount);
	*count = orderCount;
	return CXTrue;
}

static CXBool CXNetworkRecomputeValidRange(const CXBool *activity, CXSize capacity, CXSize *start, CXSize *end) {
	if (!start || !end) {
		return CXFalse;
	}
	if (!activity || capacity == 0) {
		*start = 0;
		*end = 0;
		return CXTrue;
	}
	CXBool found = CXFalse;
	CXSize minIdx = 0;
	CXSize maxIdx = 0;
	for (CXSize i = 0; i < capacity; i++) {
		if (activity[i]) {
			if (!found) {
				minIdx = i;
				maxIdx = i;
				found = CXTrue;
			} else {
				maxIdx = i;
			}
		}
	}
	if (!found) {
		*start = 0;
		*end = 0;
	} else {
		*start = minIdx;
		*end = maxIdx + 1;
	}
	return CXTrue;
}

static CXBool CXNetworkRemoveAttributeInternal(CXStringDictionaryRef dict, const CXString name) {
	if (!dict || !name) {
		return CXFalse;
	}
	CXAttributeRef attribute = (CXAttributeRef)CXStringDictionaryDeleteEntry(dict, name);
	if (!attribute) {
		return CXFalse;
	}
	CXAttributeDestroy(attribute);
	return CXTrue;
}

// -----------------------------------------------------------------------------
// Network allocation and capacity
// -----------------------------------------------------------------------------

/** Initializes a node record with empty neighbour containers. */
static void CXNodeRecordInit(CXNodeRecord *record) {
	if (!record) {
		return;
	}
	memset(record, 0, sizeof(CXNodeRecord));
	CXNeighborContainerInit(&record->outNeighbors, CXDefaultNeighborStorage, 0);
	CXNeighborContainerInit(&record->inNeighbors, CXDefaultNeighborStorage, 0);
}

/** Grows node-centric buffers until the requested capacity is satisfied. */
static CXBool CXNetworkEnsureNodeCapacity(CXNetworkRef network, CXSize required) {
	if (!network) {
		return CXFalse;
	}
	if (required <= network->nodeCapacity) {
		return CXTrue;
	}

	CXSize newCapacity = network->nodeCapacity > 0 ? network->nodeCapacity : CXNetwork_INITIAL_NODE_CAPACITY;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}

	CXNodeRecord *newNodes = calloc(newCapacity, sizeof(CXNodeRecord));
	if (!newNodes) {
		return CXFalse;
	}
	if (network->nodes) {
		memcpy(newNodes, network->nodes, sizeof(CXNodeRecord) * network->nodeCapacity);
	}
	for (CXSize idx = network->nodeCapacity; idx < newCapacity; idx++) {
		CXNodeRecordInit(&newNodes[idx]);
	}
	CXBool *newActive = calloc(newCapacity, sizeof(CXBool));
	if (!newActive) {
		free(newNodes);
		return CXFalse;
	}
	if (network->nodeActive) {
		memcpy(newActive, network->nodeActive, sizeof(CXBool) * network->nodeCapacity);
	}

	if (network->nodeIndexManager) {
		if (!CXResizeIndexManager(network->nodeIndexManager, newCapacity)) {
			free(newNodes);
			free(newActive);
			return CXFalse;
		}
	}

	CXStringDictionaryFOR(entry, network->nodeAttributes) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		if (!CXAttributeEnsureCapacity(attribute, newCapacity)) {
			free(newNodes);
			free(newActive);
			return CXFalse;
		}
	}

	free(network->nodes);
	free(network->nodeActive);
	network->nodes = newNodes;
	network->nodeActive = newActive;
	network->nodeCapacity = newCapacity;
	return CXTrue;
}

/** Grows edge-centric buffers until the requested capacity is satisfied. */
static CXBool CXNetworkEnsureEdgeCapacity(CXNetworkRef network, CXSize required) {
	if (!network) {
		return CXFalse;
	}
	if (required <= network->edgeCapacity) {
		return CXTrue;
	}

	CXSize newCapacity = network->edgeCapacity > 0 ? network->edgeCapacity : CXNetwork_INITIAL_EDGE_CAPACITY;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}

	CXEdge *newEdges = calloc(newCapacity, sizeof(CXEdge));
	if (!newEdges) {
		return CXFalse;
	}
	if (network->edges) {
		memcpy(newEdges, network->edges, sizeof(CXEdge) * network->edgeCapacity);
	}
	CXBool *newActive = calloc(newCapacity, sizeof(CXBool));
	if (!newActive) {
		free(newEdges);
		return CXFalse;
	}
	if (network->edgeActive) {
		memcpy(newActive, network->edgeActive, sizeof(CXBool) * network->edgeCapacity);
	}

	if (network->edgeIndexManager) {
		if (!CXResizeIndexManager(network->edgeIndexManager, newCapacity)) {
			free(newEdges);
			free(newActive);
			return CXFalse;
		}
	}

	CXStringDictionaryFOR(entry, network->edgeAttributes) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		if (!CXAttributeEnsureCapacity(attribute, newCapacity)) {
			free(newEdges);
			free(newActive);
			return CXFalse;
		}
	}

	free(network->edges);
	free(network->edgeActive);
	network->edges = newEdges;
	network->edgeActive = newActive;
	network->edgeCapacity = newCapacity;
	return CXTrue;
}

// -----------------------------------------------------------------------------
// Network lifecycle
// -----------------------------------------------------------------------------

/** Allocates and initializes a network with explicit node/edge capacities. */
CXNetworkRef CXNewNetworkWithCapacity(CXBool isDirected, CXSize initialNodeCapacity, CXSize initialEdgeCapacity) {
	CXNetworkRef network = calloc(1, sizeof(CXNetwork));
	if (!network) {
		return NULL;
	}
	network->isDirected = isDirected;
	network->nodeAttributes = CXNewStringDictionary();
	network->edgeAttributes = CXNewStringDictionary();
	network->networkAttributes = CXNewStringDictionary();
	network->nodeIndexManager = CXNewIndexManager(initialNodeCapacity, initialNodeCapacity);
	network->edgeIndexManager = CXNewIndexManager(initialEdgeCapacity, initialEdgeCapacity);

	if (!CXNetworkEnsureNodeCapacity(network, initialNodeCapacity)) {
		CXFreeNetwork(network);
		return NULL;
	}
	if (!CXNetworkEnsureEdgeCapacity(network, initialEdgeCapacity)) {
		CXFreeNetwork(network);
		return NULL;
	}
	network->nodeValidRangeDirty = CXTrue;
	network->edgeValidRangeDirty = CXTrue;
	return network;
}

/** Convenience wrapper that builds a network using default capacities. */
CXNetworkRef CXNewNetwork(CXBool isDirected) {
	return CXNewNetworkWithCapacity(isDirected, CXNetwork_INITIAL_NODE_CAPACITY, CXNetwork_INITIAL_EDGE_CAPACITY);
}

/** Releases all resources owned by the network instance. */
void CXFreeNetwork(CXNetworkRef network) {
	if (!network) {
		return;
	}

	if (network->nodes) {
		for (CXSize i = 0; i < network->nodeCapacity; i++) {
			CXNeighborContainerFree(&network->nodes[i].outNeighbors);
			CXNeighborContainerFree(&network->nodes[i].inNeighbors);
		}
		free(network->nodes);
		network->nodes = NULL;
	}

	if (network->nodeActive) {
		free(network->nodeActive);
		network->nodeActive = NULL;
	}

	if (network->edges) {
		free(network->edges);
		network->edges = NULL;
	}

	if (network->edgeActive) {
		free(network->edgeActive);
		network->edgeActive = NULL;
	}

	if (network->nodeIndexManager) {
		CXFreeIndexManager(network->nodeIndexManager);
		free(network->nodeIndexManager);
		network->nodeIndexManager = NULL;
	}
	if (network->edgeIndexManager) {
		CXFreeIndexManager(network->edgeIndexManager);
		free(network->edgeIndexManager);
		network->edgeIndexManager = NULL;
	}

	CXNetworkFreeDenseLists(network);

	CXDestroyAttributeDictionary(network->nodeAttributes);
	CXDestroyAttributeDictionary(network->edgeAttributes);
	CXDestroyAttributeDictionary(network->networkAttributes);

	free(network);
}

// -----------------------------------------------------------------------------
// Capacity queries
// -----------------------------------------------------------------------------

/** Returns the number of active nodes in the network. */
CXSize CXNetworkNodeCount(CXNetworkRef network) {
	return network ? network->nodeCount : 0;
}

/** Returns the number of active edges in the network. */
CXSize CXNetworkEdgeCount(CXNetworkRef network) {
	return network ? network->edgeCount : 0;
}

/** Returns the allocated node capacity. */
CXSize CXNetworkNodeCapacity(CXNetworkRef network) {
	return network ? network->nodeCapacity : 0;
}

/** Returns the allocated edge capacity. */
CXSize CXNetworkEdgeCapacity(CXNetworkRef network) {
	return network ? network->edgeCapacity : 0;
}

CXBool CXNetworkIsDirected(CXNetworkRef network) {
	return network ? network->isDirected : CXFalse;
}

/** Writes active node indices into caller-provided storage. */
CXSize CXNetworkWriteActiveNodes(CXNetworkRef network, CXIndex *dst, CXSize capacity) {
	if (!network || network->nodeCapacity == 0 || !network->nodeActive) {
		return 0;
	}
	CXSize required = 0;
	for (CXSize idx = 0; idx < network->nodeCapacity; idx++) {
		if (network->nodeActive[idx]) {
			required++;
		}
	}
	if (!dst || capacity < required) {
		return required;
	}
	CXSize written = 0;
	for (CXSize idx = 0; idx < network->nodeCapacity; idx++) {
		if (network->nodeActive[idx]) {
			dst[written++] = idx;
		}
	}
	return written;
}

/** Writes active edge indices into caller-provided storage. */
CXSize CXNetworkWriteActiveEdges(CXNetworkRef network, CXIndex *dst, CXSize capacity) {
	if (!network || network->edgeCapacity == 0 || !network->edgeActive) {
		return 0;
	}
	CXSize required = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (network->edgeActive[idx]) {
			required++;
		}
	}
	if (!dst || capacity < required) {
		return required;
	}
	CXSize written = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (network->edgeActive[idx]) {
			dst[written++] = idx;
		}
	}
	return written;
}

/** Writes two position vectors per active edge into a caller-provided buffer. */
CXSize CXNetworkWriteActiveEdgeSegments(
	CXNetworkRef network,
	const float *positions,
	CXSize componentsPerNode,
	float *dstSegments,
	CXSize dstCapacityEdges
) {
	if (!network || !positions || componentsPerNode == 0 || !network->edgeActive) {
		return 0;
	}
	CXSize required = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (network->edgeActive[idx]) {
			required++;
		}
	}
	if (!dstSegments || dstCapacityEdges < required) {
		return required;
	}
	CXSize written = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (!network->edgeActive[idx]) {
			continue;
		}
		CXEdge edge = network->edges[idx];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
			continue;
		}
		float *out = dstSegments + ((size_t)written * componentsPerNode * 2);
		memcpy(out, positions + ((size_t)edge.from * componentsPerNode), sizeof(float) * componentsPerNode);
		memcpy(out + componentsPerNode, positions + ((size_t)edge.to * componentsPerNode), sizeof(float) * componentsPerNode);
		written++;
	}
	return written;
}

/** Writes paired node attribute spans for each active edge into caller storage. */
CXSize CXNetworkWriteActiveEdgeNodeAttributes(
	CXNetworkRef network,
	const uint8_t *nodeAttributes,
	CXSize componentsPerNode,
	CXSize componentSizeBytes,
	uint8_t *dst,
	CXSize dstCapacityEdges
) {
	if (!network || !nodeAttributes || !network->edgeActive || componentsPerNode == 0 || componentSizeBytes == 0) {
		return 0;
	}
	CXSize required = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (network->edgeActive[idx]) {
			required++;
		}
	}
	if (!dst || dstCapacityEdges < required) {
		return required;
	}
	const size_t spanBytes = (size_t)componentsPerNode * componentSizeBytes;
	CXSize written = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (!network->edgeActive[idx]) {
			continue;
		}
		const CXEdge edge = network->edges[idx];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
			continue;
		}
		uint8_t *out = dst + ((size_t)written * spanBytes * 2);
		memcpy(out, nodeAttributes + ((size_t)edge.from * spanBytes), spanBytes);
		memcpy(out + spanBytes, nodeAttributes + ((size_t)edge.to * spanBytes), spanBytes);
		written++;
	}
	return written;
}

/** Writes node attribute spans for each edge in the stored dense edge order (or active order if none is set). */
CXSize CXNetworkWriteEdgeNodeAttributesInOrder(
	CXNetworkRef network,
	const uint8_t *nodeAttributes,
	CXSize componentsPerNode,
	CXSize componentSizeBytes,
	uint8_t *dst,
	CXSize dstCapacityEdges
) {
	if (!network || !nodeAttributes || !network->edgeActive || componentsPerNode == 0 || componentSizeBytes == 0) {
		return 0;
	}
	const size_t spanBytes = (size_t)componentsPerNode * componentSizeBytes;
	CXSize required = 0;
	const CXBool hasOrder = network->edgeDenseOrder && network->edgeDenseOrderCount > 0;
	if (hasOrder) {
		for (CXSize i = 0; i < network->edgeDenseOrderCount; i++) {
			CXIndex edgeIdx = network->edgeDenseOrder[i];
			if (edgeIdx < network->edgeCapacity && network->edgeActive[edgeIdx]) {
				required++;
			}
		}
	} else {
		for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
			if (network->edgeActive[idx]) {
				required++;
			}
		}
	}
	if (!dst || dstCapacityEdges < required) {
		return required;
	}
	CXSize written = 0;
	if (hasOrder) {
		for (CXSize i = 0; i < network->edgeDenseOrderCount; i++) {
			CXIndex edgeIdx = network->edgeDenseOrder[i];
			if (edgeIdx >= network->edgeCapacity || !network->edgeActive[edgeIdx]) {
				continue;
			}
			const CXEdge edge = network->edges[edgeIdx];
			if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
				continue;
			}
			uint8_t *out = dst + ((size_t)written * spanBytes * 2);
			memcpy(out, nodeAttributes + ((size_t)edge.from * spanBytes), spanBytes);
			memcpy(out + spanBytes, nodeAttributes + ((size_t)edge.to * spanBytes), spanBytes);
			written++;
		}
		return written;
	}
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (!network->edgeActive[idx]) {
			continue;
		}
		const CXEdge edge = network->edges[idx];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
			continue;
		}
		uint8_t *out = dst + ((size_t)written * spanBytes * 2);
		memcpy(out, nodeAttributes + ((size_t)edge.from * spanBytes), spanBytes);
		memcpy(out + spanBytes, nodeAttributes + ((size_t)edge.to * spanBytes), spanBytes);
		written++;
	}
	return written;
}

/** Copies node attribute spans into the edge attribute buffer honoring endpoint selection. */
CXSize CXNetworkCopyNodeAttributesToEdgeAttributes(
	CXNetworkRef network,
	const uint8_t *nodeAttributes,
	CXSize nodeStrideBytes,
	uint8_t *edgeAttributes,
	CXSize edgeStrideBytes,
	int endpointMode,
	CXBool duplicateSingleEndpoint
) {
	if (!network || !nodeAttributes || !edgeAttributes || !network->edgeActive || nodeStrideBytes == 0 || edgeStrideBytes == 0) {
		return 0;
	}
	const CXBool copyFrom = endpointMode != 1;
	const CXBool copyTo = endpointMode != 0;
	const CXBool duplicateSingle = duplicateSingleEndpoint && (endpointMode == 0 || endpointMode == 1);
	CXSize written = 0;
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (!network->edgeActive[idx]) {
			continue;
		}
		const CXEdge edge = network->edges[idx];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
			continue;
		}
		uint8_t *out = edgeAttributes + ((size_t)idx * edgeStrideBytes);
		if (endpointMode == -1) {
			memcpy(out, nodeAttributes + ((size_t)edge.from * nodeStrideBytes), nodeStrideBytes);
			memcpy(out + nodeStrideBytes, nodeAttributes + ((size_t)edge.to * nodeStrideBytes), nodeStrideBytes);
		} else if (endpointMode == 0) {
			memcpy(out, nodeAttributes + ((size_t)edge.from * nodeStrideBytes), nodeStrideBytes);
			if (duplicateSingle) {
				memcpy(out + nodeStrideBytes, nodeAttributes + ((size_t)edge.from * nodeStrideBytes), nodeStrideBytes);
			}
		} else { /* endpointMode == 1 */
			memcpy(out, nodeAttributes + ((size_t)edge.to * nodeStrideBytes), nodeStrideBytes);
			if (duplicateSingle) {
				memcpy(out + nodeStrideBytes, nodeAttributes + ((size_t)edge.to * nodeStrideBytes), nodeStrideBytes);
			}
		}
		written++;
	}
	return written;
}

// -----------------------------------------------------------------------------
// Node management
// -----------------------------------------------------------------------------

/** Clears neighbour state for the given node so it can be reused. */
static void CXNetworkResetNodeRecord(CXNetworkRef network, CXIndex node) {
	CXNodeRecord *record = &network->nodes[node];
	CXNeighborContainerFree(&record->outNeighbors);
	CXNeighborContainerFree(&record->inNeighbors);
	CXNeighborContainerInit(&record->outNeighbors, CXDefaultNeighborStorage, 0);
	CXNeighborContainerInit(&record->inNeighbors, CXDefaultNeighborStorage, 0);
}

/** Adds `count` nodes to the network, optionally returning their indices. */
CXBool CXNetworkAddNodes(CXNetworkRef network, CXSize count, CXIndex *outIndices) {
	if (!network || count == 0) {
		return CXFalse;
	}
	if (!CXNetworkEnsureNodeCapacity(network, network->nodeCount + count)) {
		return CXFalse;
	}

	for (CXSize i = 0; i < count; i++) {
		CXIndex index = CXIndexManagerGetIndex(network->nodeIndexManager);
		if (index == CXInvalidIndexValue) {
			if (!CXNetworkEnsureNodeCapacity(network, network->nodeCapacity + 1)) {
				return CXFalse;
			}
			index = CXIndexManagerGetIndex(network->nodeIndexManager);
			if (index == CXInvalidIndexValue) {
				return CXFalse;
			}
		}
		if (outIndices) {
			outIndices[i] = index;
		}
		network->nodeActive[index] = CXTrue;
		CXNetworkResetNodeRecord(network, index);

		CXStringDictionaryFOR(entry, network->nodeAttributes) {
			CXAttributeRef attribute = (CXAttributeRef)entry->data;
			CXAttributeEnsureCapacity(attribute, network->nodeCapacity);
			CXAttributeClearSlot(attribute, index);
		}

		network->nodeCount++;
	}
	CXNetworkMarkDenseBuffersDirty(network->nodeDenseBuffers, network->nodeDenseBufferCount);
	network->nodeIndexDense.dirty = CXTrue;
	network->nodeValidRangeDirty = CXTrue;
	CXNetworkMarkNodesDirty(network);
	return CXTrue;
}

/** Collects edges from a neighbour container into a heap-allocated buffer. */
static void CXCollectEdgesFromContainer(CXNeighborContainer *container, CXIndex **buffer, CXSize *count) {
	CXSize edgeCount = CXNeighborContainerCount(container);
	if (edgeCount == 0) {
		*buffer = NULL;
		*count = 0;
		return;
	}
	CXIndex *edges = malloc(sizeof(CXIndex) * edgeCount);
	if (!edges) {
		*buffer = NULL;
		*count = 0;
		return;
	}
	CXNeighborContainerGetEdges(container, edges, edgeCount);
	*buffer = edges;
	*count = edgeCount;
}

/** Removes the referenced nodes alongside their incident edges. */
CXBool CXNetworkRemoveNodes(CXNetworkRef network, const CXIndex *indices, CXSize count) {
	if (!network || !indices || count == 0) {
		return CXFalse;
	}

	for (CXSize i = 0; i < count; i++) {
		CXIndex node = indices[i];
		if (node >= network->nodeCapacity || !network->nodeActive[node]) {
			continue;
		}

		// Remove outgoing edges
		CXIndex *edgesBuffer = NULL;
		CXSize edgesCount = 0;
		CXCollectEdgesFromContainer(&network->nodes[node].outNeighbors, &edgesBuffer, &edgesCount);
		for (CXSize e = 0; e < edgesCount; e++) {
			CXNetworkDetachEdge(network, edgesBuffer[e], CXTrue);
		}
		free(edgesBuffer);

		// Remove incoming edges
		edgesBuffer = NULL;
		edgesCount = 0;
		CXCollectEdgesFromContainer(&network->nodes[node].inNeighbors, &edgesBuffer, &edgesCount);
		for (CXSize e = 0; e < edgesCount; e++) {
			CXNetworkDetachEdge(network, edgesBuffer[e], CXTrue);
		}
		free(edgesBuffer);

		CXNetworkResetNodeRecord(network, node);
		network->nodeActive[node] = CXFalse;
		CXIndexManagerAddIndex(network->nodeIndexManager, node);
		CXStringDictionaryFOR(entry, network->nodeAttributes) {
			CXAttributeClearSlot((CXAttributeRef)entry->data, node);
		}
		if (network->nodeCount > 0) {
			network->nodeCount--;
		}
	}
	CXNetworkMarkDenseBuffersDirty(network->nodeDenseBuffers, network->nodeDenseBufferCount);
	network->nodeIndexDense.dirty = CXTrue;
	CXNetworkMarkDenseBuffersDirty(network->edgeDenseBuffers, network->edgeDenseBufferCount);
	network->edgeIndexDense.dirty = CXTrue;
	network->nodeValidRangeDirty = CXTrue;
	network->edgeValidRangeDirty = CXTrue;
	CXNetworkMarkNodesDirty(network);
	CXNetworkMarkEdgesDirty(network);
	return CXTrue;
}

/** Returns whether the provided node index currently maps to an active node. */
CXBool CXNetworkIsNodeActive(CXNetworkRef network, CXIndex node) {
	if (!network || node >= network->nodeCapacity) {
		return CXFalse;
	}
	return network->nodeActive[node];
}

/** Exposes the raw node-activity bitmap for zero-copy access. */
const CXBool* CXNetworkNodeActivityBuffer(CXNetworkRef network) {
	return network ? network->nodeActive : NULL;
}

// -----------------------------------------------------------------------------
// Edge management
// -----------------------------------------------------------------------------

/** Clears edge endpoints so the slot can be reused. */
static void CXNetworkResetEdgeRecord(CXNetworkRef network, CXIndex edge) {
	if (!network || edge >= network->edgeCapacity) {
		return;
	}
	memset(&network->edges[edge], 0, sizeof(CXEdge));
}

/** Removes the specific edge from the provided neighbour container. */
static void CXNeighborContainerRemoveSingleEdge(CXNeighborContainer *container, CXIndex edge) {
	if (!container) {
		return;
	}
	if (container->storageType == CXNeighborListType) {
		CXNeighborListRemoveEdgesFromArray(&container->storage.list, &edge, 1);
	} else {
		CXNeighborMapRemoveEdgesFromArray(&container->storage.map, &edge, 1);
	}
}

/** Disconnects an edge from its endpoints and optionally recycles its index. */
static void CXNetworkDetachEdge(CXNetworkRef network, CXIndex edge, CXBool recycleIndex) {
	if (!network || edge >= network->edgeCapacity || !network->edgeActive[edge]) {
		return;
	}
	CXNetworkMarkEdgesDirty(network);
	CXNetworkMarkDenseBuffersDirty(network->edgeDenseBuffers, network->edgeDenseBufferCount);
	network->edgeIndexDense.dirty = CXTrue;
	network->edgeValidRangeDirty = CXTrue;
	CXEdge edgeData = network->edges[edge];
	CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.from].outNeighbors, edge);
	CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.to].inNeighbors, edge);
	if (!network->isDirected) {
		CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.from].inNeighbors, edge);
		CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.to].outNeighbors, edge);
	}
	network->edgeActive[edge] = CXFalse;
	CXNetworkResetEdgeRecord(network, edge);
	CXStringDictionaryFOR(entry, network->edgeAttributes) {
		CXAttributeClearSlot((CXAttributeRef)entry->data, edge);
	}
	if (network->edgeCount > 0) {
		network->edgeCount--;
	}
	if (recycleIndex) {
		CXIndexManagerAddIndex(network->edgeIndexManager, edge);
	}
}

/** Adds new edges to the network, validating endpoints and returning indices. */
CXBool CXNetworkAddEdges(CXNetworkRef network, const CXEdge *edges, CXSize count, CXIndex *outIndices) {
	if (!network || !edges || count == 0) {
		return CXFalse;
	}
	if (!CXNetworkEnsureEdgeCapacity(network, network->edgeCount + count)) {
		return CXFalse;
	}

	for (CXSize i = 0; i < count; i++) {
		CXEdge edge = edges[i];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
			return CXFalse;
		}
		if (!network->nodeActive[edge.from] || !network->nodeActive[edge.to]) {
			return CXFalse;
		}

		CXIndex edgeIndex = CXIndexManagerGetIndex(network->edgeIndexManager);
		if (edgeIndex == CXInvalidIndexValue) {
			if (!CXNetworkEnsureEdgeCapacity(network, network->edgeCapacity + 1)) {
				return CXFalse;
			}
			edgeIndex = CXIndexManagerGetIndex(network->edgeIndexManager);
			if (edgeIndex == CXInvalidIndexValue) {
				return CXFalse;
			}
		}
		if (outIndices) {
			outIndices[i] = edgeIndex;
		}
		network->edges[edgeIndex] = edge;
		network->edgeActive[edgeIndex] = CXTrue;

		CXNeighborContainerAdd(&network->nodes[edge.from].outNeighbors, edge.to, edgeIndex);
		CXNeighborContainerAdd(&network->nodes[edge.to].inNeighbors, edge.from, edgeIndex);
		if (!network->isDirected) {
			CXNeighborContainerAdd(&network->nodes[edge.from].inNeighbors, edge.to, edgeIndex);
			CXNeighborContainerAdd(&network->nodes[edge.to].outNeighbors, edge.from, edgeIndex);
		}

		CXStringDictionaryFOR(entry, network->edgeAttributes) {
			CXAttributeEnsureCapacity((CXAttributeRef)entry->data, network->edgeCapacity);
			CXAttributeClearSlot((CXAttributeRef)entry->data, edgeIndex);
		}

		network->edgeCount++;
	}
	CXNetworkMarkDenseBuffersDirty(network->edgeDenseBuffers, network->edgeDenseBufferCount);
	network->edgeIndexDense.dirty = CXTrue;
	network->edgeValidRangeDirty = CXTrue;
	CXNetworkMarkEdgesDirty(network);
	return CXTrue;
}

/** Removes the referenced edges from the network. */
CXBool CXNetworkRemoveEdges(CXNetworkRef network, const CXIndex *indices, CXSize count) {
	if (!network || !indices || count == 0) {
		return CXFalse;
	}
	for (CXSize i = 0; i < count; i++) {
		CXNetworkDetachEdge(network, indices[i], CXTrue);
	}
	return CXTrue;
}

/** Returns whether the supplied edge index corresponds to an active edge. */
CXBool CXNetworkIsEdgeActive(CXNetworkRef network, CXIndex edge) {
	if (!network || edge >= network->edgeCapacity) {
		return CXFalse;
	}
	return network->edgeActive[edge];
}

/** Exposes the raw edge-activity bitmap for zero-copy access. */
const CXBool* CXNetworkEdgeActivityBuffer(CXNetworkRef network) {
	return network ? network->edgeActive : NULL;
}

/** Returns a pointer to the contiguous edge buffer `[from,to,...]`. */
CXEdge* CXNetworkEdgesBuffer(CXNetworkRef network) {
	return network ? network->edges : NULL;
}

// -----------------------------------------------------------------------------
// Adjacency access
// -----------------------------------------------------------------------------

/** Returns the outbound neighbour container for the given node. */
CXNeighborContainer* CXNetworkOutNeighbors(CXNetworkRef network, CXIndex node) {
	if (!network || node >= network->nodeCapacity) {
		return NULL;
	}
	return &network->nodes[node].outNeighbors;
}

/** Returns the inbound neighbour container for the given node. */
CXNeighborContainer* CXNetworkInNeighbors(CXNetworkRef network, CXIndex node) {
	if (!network || node >= network->nodeCapacity) {
		return NULL;
	}
	return &network->nodes[node].inNeighbors;
}

// -----------------------------------------------------------------------------
// Attribute API
// -----------------------------------------------------------------------------

/** Looks up an attribute within the provided dictionary helper. */
static CXAttributeRef CXNetworkGetAttribute(CXStringDictionaryRef dictionary, const CXString name) {
	if (!dictionary || !name) {
		return NULL;
	}
	return (CXAttributeRef)CXStringDictionaryEntryForKey(dictionary, name);
}

/** Registers a node attribute with the provided configuration. */
CXBool CXNetworkDefineNodeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkGetAttribute(network->nodeAttributes, name)) {
		return CXFalse;
	}
	CXAttributeRef attribute = CXAttributeCreate(type, dimension, network->nodeCapacity);
	if (!attribute) {
		return CXFalse;
	}
	CXStringDictionarySetEntry(network->nodeAttributes, name, attribute);
	return CXTrue;
}

/** Registers an edge attribute with the provided configuration. */
CXBool CXNetworkDefineEdgeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkGetAttribute(network->edgeAttributes, name)) {
		return CXFalse;
	}
	CXAttributeRef attribute = CXAttributeCreate(type, dimension, network->edgeCapacity);
	if (!attribute) {
		return CXFalse;
	}
	CXStringDictionarySetEntry(network->edgeAttributes, name, attribute);
	return CXTrue;
}

/** Registers a network-level attribute with the provided configuration. */
CXBool CXNetworkDefineNetworkAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkGetAttribute(network->networkAttributes, name)) {
		return CXFalse;
	}
	CXAttributeRef attribute = CXAttributeCreate(type, dimension, 1);
	if (!attribute) {
		return CXFalse;
	}
	CXStringDictionarySetEntry(network->networkAttributes, name, attribute);
	return CXTrue;
}

CXBool CXNetworkRemoveNodeAttribute(CXNetworkRef network, const CXString name) {
	if (!network || !name) {
		return CXFalse;
	}
	CXNetworkRemoveDenseNodeAttribute(network, name);
	return CXNetworkRemoveAttributeInternal(network->nodeAttributes, name);
}

CXBool CXNetworkRemoveEdgeAttribute(CXNetworkRef network, const CXString name) {
	if (!network || !name) {
		return CXFalse;
	}
	CXNetworkRemoveDenseEdgeAttribute(network, name);
	return CXNetworkRemoveAttributeInternal(network->edgeAttributes, name);
}

CXBool CXNetworkRemoveNetworkAttribute(CXNetworkRef network, const CXString name) {
	if (!network || !name) {
		return CXFalse;
	}
	return CXNetworkRemoveAttributeInternal(network->networkAttributes, name);
}

/** Retrieves the node attribute descriptor for the supplied name. */
CXAttributeRef CXNetworkGetNodeAttribute(CXNetworkRef network, const CXString name) {
	return network ? CXNetworkGetAttribute(network->nodeAttributes, name) : NULL;
}

/** Retrieves the edge attribute descriptor for the supplied name. */
CXAttributeRef CXNetworkGetEdgeAttribute(CXNetworkRef network, const CXString name) {
	return network ? CXNetworkGetAttribute(network->edgeAttributes, name) : NULL;
}

/** Retrieves the network-level attribute descriptor for the supplied name. */
CXAttributeRef CXNetworkGetNetworkAttribute(CXNetworkRef network, const CXString name) {
	return network ? CXNetworkGetAttribute(network->networkAttributes, name) : NULL;
}

/** Returns a pointer to the raw node attribute buffer, or NULL when missing. */
void* CXNetworkGetNodeAttributeBuffer(CXNetworkRef network, const CXString name) {
	CXAttributeRef attr = CXNetworkGetNodeAttribute(network, name);
	return attr ? attr->data : NULL;
}

/** Returns a pointer to the raw edge attribute buffer, or NULL when missing. */
void* CXNetworkGetEdgeAttributeBuffer(CXNetworkRef network, const CXString name) {
	CXAttributeRef attr = CXNetworkGetEdgeAttribute(network, name);
	return attr ? attr->data : NULL;
}

/** Returns a pointer to the raw network attribute buffer, or NULL when missing. */
void* CXNetworkGetNetworkAttributeBuffer(CXNetworkRef network, const CXString name) {
	CXAttributeRef attr = CXNetworkGetNetworkAttribute(network, name);
	return attr ? attr->data : NULL;
}

/** Returns the byte stride for entries in the attribute buffer. */
CXSize CXAttributeStride(CXAttributeRef attribute) {
	return attribute ? attribute->stride : 0;
}

// -----------------------------------------------------------------------------
// Compaction
// -----------------------------------------------------------------------------

static CXAttributeRef CXEnsureMappingAttribute(CXNetworkRef network, CXStringDictionaryRef dictionary, const CXString name, CXSize capacity) {
	if (!name) {
		return NULL;
	}
	CXAttributeRef attr = (CXAttributeRef)CXStringDictionaryEntryForKey(dictionary, name);
	if (attr) {
		if (attr->type != CXUnsignedIntegerAttributeType || attr->dimension != 1) {
			return NULL;
		}
		if (!CXAttributeEnsureCapacity(attr, capacity)) {
			return NULL;
		}
		return attr;
	}
	if (dictionary == network->nodeAttributes) {
		if (!CXNetworkDefineNodeAttribute(network, name, CXUnsignedIntegerAttributeType, 1)) {
			return NULL;
		}
		return CXNetworkGetNodeAttribute(network, name);
	}
	if (dictionary == network->edgeAttributes) {
		if (!CXNetworkDefineEdgeAttribute(network, name, CXUnsignedIntegerAttributeType, 1)) {
			return NULL;
		}
		return CXNetworkGetEdgeAttribute(network, name);
	}
	return NULL;
}

CXBool CXNetworkCompact(CXNetworkRef network, const CXString nodeOriginalIndexAttr, const CXString edgeOriginalIndexAttr) {
	if (!network) {
		return CXFalse;
	}

	CXSize nodeCount = network->nodeCount;
	CXSize edgeCount = network->edgeCount;

	CXNetworkRef compact = CXNewNetworkWithCapacity(
		network->isDirected,
		nodeCount > 0 ? nodeCount : 1,
		edgeCount > 0 ? edgeCount : 1
	);
	if (!compact) {
		return CXFalse;
	}

	CXIndex *nodeRemap = NULL;
	CXIndex *edgeRemap = NULL;
	CXEdge *edgeBuffer = NULL;
	CXIndex *edgeOrder = NULL;
	CXIndex *newEdgeIds = NULL;

	// Clone attribute declarations and transfer categorical dictionaries.
	CXStringDictionaryFOR(nodeEntry, network->nodeAttributes) {
		CXAttributeRef attr = (CXAttributeRef)nodeEntry->data;
		if (!CXNetworkDefineNodeAttribute(compact, nodeEntry->key, attr->type, attr->dimension)) {
			goto fail;
		}
		CXAttributeRef newAttr = CXNetworkGetNodeAttribute(compact, nodeEntry->key);
		if (!CXAttributeEnsureCapacity(newAttr, nodeCount)) {
			goto fail;
		}
		newAttr->usesJavascriptShadow = attr->usesJavascriptShadow;
		newAttr->categoricalDictionary = attr->categoricalDictionary;
		attr->categoricalDictionary = NULL;
	}

	CXStringDictionaryFOR(edgeEntry, network->edgeAttributes) {
		CXAttributeRef attr = (CXAttributeRef)edgeEntry->data;
		if (!CXNetworkDefineEdgeAttribute(compact, edgeEntry->key, attr->type, attr->dimension)) {
			goto fail;
		}
		CXAttributeRef newAttr = CXNetworkGetEdgeAttribute(compact, edgeEntry->key);
		if (!CXAttributeEnsureCapacity(newAttr, edgeCount)) {
			goto fail;
		}
		newAttr->usesJavascriptShadow = attr->usesJavascriptShadow;
		newAttr->categoricalDictionary = attr->categoricalDictionary;
		attr->categoricalDictionary = NULL;
	}

	CXStringDictionaryFOR(netEntry, network->networkAttributes) {
		CXAttributeRef attr = (CXAttributeRef)netEntry->data;
		if (!CXNetworkDefineNetworkAttribute(compact, netEntry->key, attr->type, attr->dimension)) {
			goto fail;
		}
		CXAttributeRef newAttr = CXNetworkGetNetworkAttribute(compact, netEntry->key);
		if (!CXAttributeEnsureCapacity(newAttr, 1)) {
			goto fail;
		}
		newAttr->usesJavascriptShadow = attr->usesJavascriptShadow;
		newAttr->categoricalDictionary = attr->categoricalDictionary;
		attr->categoricalDictionary = NULL;
	}

	if (nodeCount > 0) {
		if (!CXNetworkAddNodes(compact, nodeCount, NULL)) {
			goto fail;
		}
	}

	nodeRemap = calloc(network->nodeCapacity > 0 ? network->nodeCapacity : 1, sizeof(CXIndex));
	if (!nodeRemap) {
		goto fail;
	}
	for (CXSize i = 0; i < network->nodeCapacity; i++) {
		nodeRemap[i] = CXIndexMAX;
	}
	CXSize nextNode = 0;
	for (CXSize i = 0; i < network->nodeCapacity; i++) {
		if (network->nodeActive && network->nodeActive[i]) {
			nodeRemap[i] = (CXIndex)nextNode++;
		}
	}

	edgeRemap = calloc(network->edgeCapacity > 0 ? network->edgeCapacity : 1, sizeof(CXIndex));
	if (!edgeRemap) {
		goto fail;
	}
	for (CXSize i = 0; i < network->edgeCapacity; i++) {
		edgeRemap[i] = CXIndexMAX;
	}

	if (edgeCount > 0) {
		edgeBuffer = malloc(sizeof(CXEdge) * edgeCount);
		edgeOrder = malloc(sizeof(CXIndex) * edgeCount);
		newEdgeIds = malloc(sizeof(CXIndex) * edgeCount);
		if (!edgeBuffer || !edgeOrder || !newEdgeIds) {
			goto fail;
		}
		CXSize writeEdge = 0;
		for (CXSize i = 0; i < network->edgeCapacity; i++) {
			if (network->edgeActive && network->edgeActive[i]) {
				CXEdge edge = network->edges[i];
				CXIndex from = nodeRemap[edge.from];
				CXIndex to = nodeRemap[edge.to];
				if (from == CXIndexMAX || to == CXIndexMAX) {
					goto fail;
				}
				edgeBuffer[writeEdge].from = from;
				edgeBuffer[writeEdge].to = to;
				edgeOrder[writeEdge] = (CXIndex)i;
				writeEdge++;
			}
		}
		if (writeEdge != edgeCount) {
			goto fail;
		}
		if (!CXNetworkAddEdges(compact, edgeBuffer, edgeCount, newEdgeIds)) {
			goto fail;
		}
		for (CXSize i = 0; i < edgeCount; i++) {
			edgeRemap[edgeOrder[i]] = newEdgeIds[i];
		}
	}

	// Copy node attribute payloads.
	CXStringDictionaryFOR(nodeEntry2, network->nodeAttributes) {
		CXAttributeRef oldAttr = (CXAttributeRef)nodeEntry2->data;
		CXAttributeRef newAttr = CXNetworkGetNodeAttribute(compact, nodeEntry2->key);
		if (!newAttr) {
			goto fail;
		}
		uint8_t *dstData = newAttr->data ? (uint8_t *)newAttr->data : NULL;
		uint8_t *srcData = oldAttr->data ? (uint8_t *)oldAttr->data : NULL;
		if (!dstData || !srcData) {
			continue;
		}
		for (CXSize i = 0; i < network->nodeCapacity; i++) {
			CXIndex mapped = nodeRemap[i];
			if (mapped == CXIndexMAX) {
				continue;
			}
			memcpy(dstData + (size_t)mapped * newAttr->stride, srcData + (size_t)i * oldAttr->stride, oldAttr->stride);
		}
	}

	// Copy edge attribute payloads.
	CXStringDictionaryFOR(edgeEntry2, network->edgeAttributes) {
		CXAttributeRef oldAttr = (CXAttributeRef)edgeEntry2->data;
		CXAttributeRef newAttr = CXNetworkGetEdgeAttribute(compact, edgeEntry2->key);
		if (!newAttr) {
			goto fail;
		}
		uint8_t *dstData = newAttr->data ? (uint8_t *)newAttr->data : NULL;
		uint8_t *srcData = oldAttr->data ? (uint8_t *)oldAttr->data : NULL;
		if (!dstData || !srcData) {
			continue;
		}
		for (CXSize i = 0; i < network->edgeCapacity; i++) {
			CXIndex mapped = edgeRemap[i];
			if (mapped == CXIndexMAX) {
				continue;
			}
			memcpy(dstData + (size_t)mapped * newAttr->stride, srcData + (size_t)i * oldAttr->stride, oldAttr->stride);
		}
	}

	// Copy network-level attributes.
	CXStringDictionaryFOR(netEntry2, network->networkAttributes) {
		CXAttributeRef oldAttr = (CXAttributeRef)netEntry2->data;
		CXAttributeRef newAttr = CXNetworkGetNetworkAttribute(compact, netEntry2->key);
		if (!newAttr || !oldAttr->data || !newAttr->data) {
			continue;
		}
		memcpy(newAttr->data, oldAttr->data, oldAttr->stride);
	}

	// Optional original index attributes.
	CXAttributeRef nodeOriginAttr = CXEnsureMappingAttribute(compact, compact->nodeAttributes, nodeOriginalIndexAttr, nodeCount > 0 ? nodeCount : 1);
	if (nodeOriginalIndexAttr && !nodeOriginAttr) {
		goto fail;
	}
	CXAttributeRef edgeOriginAttr = CXEnsureMappingAttribute(compact, compact->edgeAttributes, edgeOriginalIndexAttr, edgeCount > 0 ? edgeCount : 1);
	if (edgeOriginalIndexAttr && !edgeOriginAttr) {
		goto fail;
	}

	if (nodeOriginAttr && nodeOriginAttr->data) {
		uint64_t *origin = (uint64_t *)nodeOriginAttr->data;
		for (CXSize i = 0; i < network->nodeCapacity; i++) {
			CXIndex mapped = nodeRemap[i];
			if (mapped == CXIndexMAX) {
				continue;
			}
			origin[mapped] = (uint64_t)i;
		}
	}
	if (edgeOriginAttr && edgeOriginAttr->data) {
		uint64_t *origin = (uint64_t *)edgeOriginAttr->data;
		for (CXSize i = 0; i < network->edgeCapacity; i++) {
			CXIndex mapped = edgeRemap[i];
			if (mapped == CXIndexMAX) {
				continue;
			}
			origin[mapped] = (uint64_t)i;
		}
	}

	// Shrink backing arrays to match the exact number of active elements.
	if (nodeCount > 0 && compact->nodeCapacity > nodeCount) {
		CXNodeRecord *newNodes = calloc(nodeCount, sizeof(CXNodeRecord));
		CXBool *newActive = calloc(nodeCount, sizeof(CXBool));
		if (!newNodes || !newActive) {
			free(newNodes);
			free(newActive);
			goto fail;
		}
		memcpy(newNodes, compact->nodes, sizeof(CXNodeRecord) * nodeCount);
		memcpy(newActive, compact->nodeActive, sizeof(CXBool) * nodeCount);
		free(compact->nodes);
		free(compact->nodeActive);
		compact->nodes = newNodes;
		compact->nodeActive = newActive;
		compact->nodeCapacity = nodeCount;
	}
	if (edgeCount > 0 && compact->edgeCapacity > edgeCount) {
		CXEdge *newEdges = calloc(edgeCount, sizeof(CXEdge));
		CXBool *newEdgeActive = calloc(edgeCount, sizeof(CXBool));
		if (!newEdges || !newEdgeActive) {
			free(newEdges);
			free(newEdgeActive);
			goto fail;
		}
		memcpy(newEdges, compact->edges, sizeof(CXEdge) * edgeCount);
		memcpy(newEdgeActive, compact->edgeActive, sizeof(CXBool) * edgeCount);
		free(compact->edges);
		free(compact->edgeActive);
		compact->edges = newEdges;
		compact->edgeActive = newEdgeActive;
		compact->edgeCapacity = edgeCount;
	}

	// Adjust index managers to the new capacities.
	CXResizeIndexManager(compact->nodeIndexManager, nodeCount);
	CXResizeIndexManager(compact->edgeIndexManager, edgeCount);
	compact->nodeCount = nodeCount;
	compact->edgeCount = edgeCount;
	if (nodeCount == 0) {
		if (compact->nodes) {
			for (CXSize i = 0; i < compact->nodeCapacity; i++) {
				CXNeighborContainerFree(&compact->nodes[i].outNeighbors);
				CXNeighborContainerFree(&compact->nodes[i].inNeighbors);
			}
			free(compact->nodes);
			compact->nodes = NULL;
		}
		free(compact->nodeActive);
		compact->nodeActive = NULL;
		compact->nodeCapacity = 0;
	}
	if (edgeCount == 0) {
		free(compact->edges);
		compact->edges = NULL;
		free(compact->edgeActive);
		compact->edgeActive = NULL;
		compact->edgeCapacity = 0;
	}

	CXNetworkFreeDenseLists(network);
	CXNetworkFreeDenseLists(compact);

	CXNetwork temp = *network;
	*network = *compact;
	compact->nodes = temp.nodes;
	compact->nodeActive = temp.nodeActive;
	compact->edges = temp.edges;
	compact->edgeActive = temp.edgeActive;
	compact->nodeAttributes = temp.nodeAttributes;
	compact->edgeAttributes = temp.edgeAttributes;
	compact->networkAttributes = temp.networkAttributes;
	compact->nodeIndexManager = temp.nodeIndexManager;
	compact->edgeIndexManager = temp.edgeIndexManager;
	CXFreeNetwork(compact);

	free(nodeRemap);
	free(edgeRemap);
	free(edgeBuffer);
	free(edgeOrder);
	free(newEdgeIds);
	return CXTrue;

fail:
	if (nodeRemap) {
		free(nodeRemap);
	}
	if (edgeRemap) {
		free(edgeRemap);
	}
	free(edgeBuffer);
	free(edgeOrder);
	free(newEdgeIds);
	CXFreeNetwork(compact);
	return CXFalse;
}

// -----------------------------------------------------------------------------
// Dense attribute buffers
// -----------------------------------------------------------------------------

CXBool CXNetworkAddDenseNodeAttribute(CXNetworkRef network, const CXString name, CXSize initialCapacity) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkFindDenseBuffer(network->nodeDenseBuffers, network->nodeDenseBufferCount, name)) {
		return CXTrue;
	}
	if (!CXNetworkGetNodeAttribute(network, name)) {
		return CXFalse;
	}
	return CXNetworkAddDenseBuffer(&network->nodeDenseBuffers, &network->nodeDenseBufferCount, &network->nodeDenseBufferCapacity, name, initialCapacity, CXFalse);
}

CXBool CXNetworkAddDenseEdgeAttribute(CXNetworkRef network, const CXString name, CXSize initialCapacity) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkFindDenseBuffer(network->edgeDenseBuffers, network->edgeDenseBufferCount, name)) {
		return CXTrue;
	}
	if (!CXNetworkGetEdgeAttribute(network, name)) {
		return CXFalse;
	}
	return CXNetworkAddDenseBuffer(&network->edgeDenseBuffers, &network->edgeDenseBufferCount, &network->edgeDenseBufferCapacity, name, initialCapacity, CXFalse);
}

CXBool CXNetworkRemoveDenseNodeAttribute(CXNetworkRef network, const CXString name) {
	if (!network || !name) {
		return CXFalse;
	}
	for (CXSize i = 0; i < network->nodeDenseBufferCount; i++) {
		if (network->nodeDenseBuffers[i].name && strcmp(network->nodeDenseBuffers[i].name, name) == 0) {
			CXNetworkFreeDenseBuffer(&network->nodeDenseBuffers[i]);
			if (i + 1 < network->nodeDenseBufferCount) {
				memmove(&network->nodeDenseBuffers[i], &network->nodeDenseBuffers[i + 1], sizeof(CXDenseAttributeBuffer) * (network->nodeDenseBufferCount - i - 1));
			}
			network->nodeDenseBufferCount--;
			return CXTrue;
		}
	}
	return CXFalse;
}

CXBool CXNetworkRemoveDenseEdgeAttribute(CXNetworkRef network, const CXString name) {
	if (!network || !name) {
		return CXFalse;
	}
	for (CXSize i = 0; i < network->edgeDenseBufferCount; i++) {
		if (network->edgeDenseBuffers[i].name && strcmp(network->edgeDenseBuffers[i].name, name) == 0) {
			CXNetworkFreeDenseBuffer(&network->edgeDenseBuffers[i]);
			if (i + 1 < network->edgeDenseBufferCount) {
				memmove(&network->edgeDenseBuffers[i], &network->edgeDenseBuffers[i + 1], sizeof(CXDenseAttributeBuffer) * (network->edgeDenseBufferCount - i - 1));
			}
			network->edgeDenseBufferCount--;
			return CXTrue;
		}
	}
	return CXFalse;
}

CXBool CXNetworkMarkDenseNodeAttributeDirty(CXNetworkRef network, const CXString name) {
	CXDenseAttributeBuffer *buf = network ? CXNetworkFindDenseBuffer(network->nodeDenseBuffers, network->nodeDenseBufferCount, name) : NULL;
	if (!buf) {
		return CXFalse;
	}
	buf->dirty = CXTrue;
	CXNetworkMarkDenseColorBuffersDirtyForSource(network->nodeColorAttributes, network->nodeColorAttributeCount, name);
	return CXTrue;
}

CXBool CXNetworkMarkDenseEdgeAttributeDirty(CXNetworkRef network, const CXString name) {
	CXDenseAttributeBuffer *buf = network ? CXNetworkFindDenseBuffer(network->edgeDenseBuffers, network->edgeDenseBufferCount, name) : NULL;
	if (!buf) {
		return CXFalse;
	}
	buf->dirty = CXTrue;
	CXNetworkMarkDenseColorBuffersDirtyForSource(network->edgeColorAttributes, network->edgeColorAttributeCount, name);
	return CXTrue;
}

static CXBool CXNetworkValidateColorSourceAttribute(CXAttributeRef attribute) {
	if (!attribute) {
		return CXFalse;
	}
	if (attribute->dimension != 1) {
		return CXFalse;
	}
	return attribute->type == CXIntegerAttributeType || attribute->type == CXUnsignedIntegerAttributeType;
}

static CXDenseColorFormat CXNetworkNormalizeColorFormat(CXDenseColorFormat format) {
	return format == CXDenseColorFormatU32x4 ? CXDenseColorFormatU32x4 : CXDenseColorFormatU8x4;
}

CXBool CXNetworkDefineDenseColorEncodedNodeAttribute(CXNetworkRef network, const CXString sourceName, const CXString encodedName, CXDenseColorEncodingOptions options) {
	if (!network || !sourceName || !encodedName) {
		return CXFalse;
	}
	const CXBool useIndex = strcmp(sourceName, "index") == 0;
	const CXDenseColorFormat format = CXNetworkNormalizeColorFormat(options.format);
	CXDenseColorEncodedAttribute *existing = CXNetworkFindDenseColorAttribute(network->nodeColorAttributes, network->nodeColorAttributeCount, encodedName);
	if (existing) {
		const CXBool sameSource = existing->useIndexSource == useIndex && (useIndex || (existing->sourceName && strcmp(existing->sourceName, sourceName) == 0));
		if (!sameSource || existing->format != format) {
			return CXFalse;
		}
		return CXTrue;
	}
	if (!useIndex) {
		CXAttributeRef sourceAttr = CXNetworkGetNodeAttribute(network, sourceName);
		if (!CXNetworkValidateColorSourceAttribute(sourceAttr)) {
			return CXFalse;
		}
	}
	return CXNetworkAddDenseColorAttribute(&network->nodeColorAttributes, &network->nodeColorAttributeCount, &network->nodeColorAttributeCapacity, sourceName, encodedName, format, useIndex);
}

CXBool CXNetworkDefineDenseColorEncodedEdgeAttribute(CXNetworkRef network, const CXString sourceName, const CXString encodedName, CXDenseColorEncodingOptions options) {
	if (!network || !sourceName || !encodedName) {
		return CXFalse;
	}
	const CXBool useIndex = strcmp(sourceName, "index") == 0;
	const CXDenseColorFormat format = CXNetworkNormalizeColorFormat(options.format);
	CXDenseColorEncodedAttribute *existing = CXNetworkFindDenseColorAttribute(network->edgeColorAttributes, network->edgeColorAttributeCount, encodedName);
	if (existing) {
		const CXBool sameSource = existing->useIndexSource == useIndex && (useIndex || (existing->sourceName && strcmp(existing->sourceName, sourceName) == 0));
		if (!sameSource || existing->format != format) {
			return CXFalse;
		}
		return CXTrue;
	}
	if (!useIndex) {
		CXAttributeRef sourceAttr = CXNetworkGetEdgeAttribute(network, sourceName);
		if (!CXNetworkValidateColorSourceAttribute(sourceAttr)) {
			return CXFalse;
		}
	}
	return CXNetworkAddDenseColorAttribute(&network->edgeColorAttributes, &network->edgeColorAttributeCount, &network->edgeColorAttributeCapacity, sourceName, encodedName, format, useIndex);
}

CXBool CXNetworkRemoveDenseColorEncodedNodeAttribute(CXNetworkRef network, const CXString encodedName) {
	if (!network || !encodedName) {
		return CXFalse;
	}
	for (CXSize i = 0; i < network->nodeColorAttributeCount; i++) {
		if (network->nodeColorAttributes[i].encodedName && strcmp(network->nodeColorAttributes[i].encodedName, encodedName) == 0) {
			CXNetworkFreeDenseColorAttribute(&network->nodeColorAttributes[i]);
			if (i + 1 < network->nodeColorAttributeCount) {
				memmove(&network->nodeColorAttributes[i], &network->nodeColorAttributes[i + 1], sizeof(CXDenseColorEncodedAttribute) * (network->nodeColorAttributeCount - i - 1));
			}
			network->nodeColorAttributeCount--;
			return CXTrue;
		}
	}
	return CXFalse;
}

CXBool CXNetworkRemoveDenseColorEncodedEdgeAttribute(CXNetworkRef network, const CXString encodedName) {
	if (!network || !encodedName) {
		return CXFalse;
	}
	for (CXSize i = 0; i < network->edgeColorAttributeCount; i++) {
		if (network->edgeColorAttributes[i].encodedName && strcmp(network->edgeColorAttributes[i].encodedName, encodedName) == 0) {
			CXNetworkFreeDenseColorAttribute(&network->edgeColorAttributes[i]);
			if (i + 1 < network->edgeColorAttributeCount) {
				memmove(&network->edgeColorAttributes[i], &network->edgeColorAttributes[i + 1], sizeof(CXDenseColorEncodedAttribute) * (network->edgeColorAttributeCount - i - 1));
			}
			network->edgeColorAttributeCount--;
			return CXTrue;
		}
	}
	return CXFalse;
}

CXBool CXNetworkMarkDenseColorEncodedNodeAttributeDirty(CXNetworkRef network, const CXString encodedName) {
	CXDenseColorEncodedAttribute *attr = network ? CXNetworkFindDenseColorAttribute(network->nodeColorAttributes, network->nodeColorAttributeCount, encodedName) : NULL;
	if (!attr) {
		return CXFalse;
	}
	attr->buffer.dirty = CXTrue;
	return CXTrue;
}

CXBool CXNetworkMarkDenseColorEncodedEdgeAttributeDirty(CXNetworkRef network, const CXString encodedName) {
	CXDenseColorEncodedAttribute *attr = network ? CXNetworkFindDenseColorAttribute(network->edgeColorAttributes, network->edgeColorAttributeCount, encodedName) : NULL;
	if (!attr) {
		return CXFalse;
	}
	attr->buffer.dirty = CXTrue;
	return CXTrue;
}

static const CXDenseAttributeBuffer* CXNetworkUpdateDenseAttributeInternal(
	CXNetworkRef network,
	const CXString name,
	CXBool isNode,
	const CXIndex *order,
	CXSize orderCount
) {
	if (!network || !name) {
		return NULL;
	}
	CXDenseAttributeBuffer *buf = isNode
		? CXNetworkFindDenseBuffer(network->nodeDenseBuffers, network->nodeDenseBufferCount, name)
		: CXNetworkFindDenseBuffer(network->edgeDenseBuffers, network->edgeDenseBufferCount, name);
	if (!buf) {
		return NULL;
	}
	CXAttributeRef attr = isNode ? CXNetworkGetNodeAttribute(network, name) : CXNetworkGetEdgeAttribute(network, name);
	if (!attr || !attr->data) {
		return NULL;
	}
	const CXBool *activity = isNode ? network->nodeActive : network->edgeActive;
	CXSize cap = isNode ? network->nodeCapacity : network->edgeCapacity;
	if (buf->dirty || !buf->data) {
		if (!CXNetworkPackDenseAttribute(network, buf, attr, order, orderCount, activity, cap)) {
			return NULL;
		}
	}
	return buf;
}

const CXDenseAttributeBuffer* CXNetworkUpdateDenseNodeAttribute(CXNetworkRef network, const CXString name) {
	const CXIndex *useOrder = NULL;
	CXSize useCount = 0;
	if (network && network->nodeDenseOrder && network->nodeDenseOrderCount > 0) {
		useOrder = network->nodeDenseOrder;
		useCount = network->nodeDenseOrderCount;
	}
	return CXNetworkUpdateDenseAttributeInternal(network, name, CXTrue, useOrder, useCount);
}

const CXDenseAttributeBuffer* CXNetworkUpdateDenseEdgeAttribute(CXNetworkRef network, const CXString name) {
	const CXIndex *useOrder = NULL;
	CXSize useCount = 0;
	if (network && network->edgeDenseOrder && network->edgeDenseOrderCount > 0) {
		useOrder = network->edgeDenseOrder;
		useCount = network->edgeDenseOrderCount;
	}
	return CXNetworkUpdateDenseAttributeInternal(network, name, CXFalse, useOrder, useCount);
}

const CXDenseAttributeBuffer* CXNetworkUpdateDenseNodeIndexBuffer(CXNetworkRef network) {
	if (!network) {
		return NULL;
	}
	const CXIndex *useOrder = NULL;
	CXSize useCount = 0;
	if (network->nodeDenseOrder && network->nodeDenseOrderCount > 0) {
		useOrder = network->nodeDenseOrder;
		useCount = network->nodeDenseOrderCount;
	}
	if (!network->nodeIndexDense.data && network->nodeIndexDense.capacity == 0) {
		memset(&network->nodeIndexDense, 0, sizeof(CXDenseAttributeBuffer));
		network->nodeIndexDense.isIndexBuffer = CXTrue;
		network->nodeIndexDense.dirty = CXTrue;
	}
	if (network->nodeIndexDense.dirty || !network->nodeIndexDense.data) {
		if (!CXNetworkPackDenseIndex(&network->nodeIndexDense, useOrder, useCount, network->nodeActive, network->nodeCapacity)) {
			return NULL;
		}
	}
	return &network->nodeIndexDense;
}

const CXDenseAttributeBuffer* CXNetworkUpdateDenseEdgeIndexBuffer(CXNetworkRef network) {
	if (!network) {
		return NULL;
	}
	const CXIndex *useOrder = NULL;
	CXSize useCount = 0;
	if (network->edgeDenseOrder && network->edgeDenseOrderCount > 0) {
		useOrder = network->edgeDenseOrder;
		useCount = network->edgeDenseOrderCount;
	}
	if (!network->edgeIndexDense.data && network->edgeIndexDense.capacity == 0) {
		memset(&network->edgeIndexDense, 0, sizeof(CXDenseAttributeBuffer));
		network->edgeIndexDense.isIndexBuffer = CXTrue;
		network->edgeIndexDense.dirty = CXTrue;
	}
	if (network->edgeIndexDense.dirty || !network->edgeIndexDense.data) {
		if (!CXNetworkPackDenseIndex(&network->edgeIndexDense, useOrder, useCount, network->edgeActive, network->edgeCapacity)) {
			return NULL;
		}
	}
	return &network->edgeIndexDense;
}

const CXDenseAttributeBuffer* CXNetworkUpdateDenseColorEncodedNodeAttribute(CXNetworkRef network, const CXString encodedName) {
	if (!network || !encodedName) {
		return NULL;
	}
	CXDenseColorEncodedAttribute *attr = CXNetworkFindDenseColorAttribute(network->nodeColorAttributes, network->nodeColorAttributeCount, encodedName);
	if (!attr) {
		return NULL;
	}
	const CXIndex *order = NULL;
	CXSize orderCount = 0;
	if (network->nodeDenseOrder && network->nodeDenseOrderCount > 0) {
		order = network->nodeDenseOrder;
		orderCount = network->nodeDenseOrderCount;
	}
	CXAttributeRef sourceAttr = NULL;
	if (!attr->useIndexSource) {
		sourceAttr = CXNetworkGetNodeAttribute(network, attr->sourceName);
		if (!CXNetworkValidateColorSourceAttribute(sourceAttr) || !sourceAttr->data) {
			return NULL;
		}
	}
	if (attr->buffer.dirty || !attr->buffer.data) {
		if (!CXNetworkPackDenseColorEncoded(attr, order, orderCount, network->nodeActive, network->nodeCapacity, sourceAttr)) {
			return NULL;
		}
	}
	return &attr->buffer;
}

const CXDenseAttributeBuffer* CXNetworkUpdateDenseColorEncodedEdgeAttribute(CXNetworkRef network, const CXString encodedName) {
	if (!network || !encodedName) {
		return NULL;
	}
	CXDenseColorEncodedAttribute *attr = CXNetworkFindDenseColorAttribute(network->edgeColorAttributes, network->edgeColorAttributeCount, encodedName);
	if (!attr) {
		return NULL;
	}
	const CXIndex *order = NULL;
	CXSize orderCount = 0;
	if (network->edgeDenseOrder && network->edgeDenseOrderCount > 0) {
		order = network->edgeDenseOrder;
		orderCount = network->edgeDenseOrderCount;
	}
	CXAttributeRef sourceAttr = NULL;
	if (!attr->useIndexSource) {
		sourceAttr = CXNetworkGetEdgeAttribute(network, attr->sourceName);
		if (!CXNetworkValidateColorSourceAttribute(sourceAttr) || !sourceAttr->data) {
			return NULL;
		}
	}
	if (attr->buffer.dirty || !attr->buffer.data) {
		if (!CXNetworkPackDenseColorEncoded(attr, order, orderCount, network->edgeActive, network->edgeCapacity, sourceAttr)) {
			return NULL;
		}
	}
	return &attr->buffer;
}

CXBool CXNetworkSetDenseNodeOrder(CXNetworkRef network, const CXIndex *order, CXSize count) {
	if (!network) {
		return CXFalse;
	}
	if (!CXNetworkSetDenseOrder(&network->nodeDenseOrder, &network->nodeDenseOrderCount, &network->nodeDenseOrderCapacity, order, count)) {
		return CXFalse;
	}
	CXNetworkMarkDenseBuffersDirty(network->nodeDenseBuffers, network->nodeDenseBufferCount);
	network->nodeIndexDense.dirty = CXTrue;
	CXNetworkMarkDenseColorBuffersDirty(network->nodeColorAttributes, network->nodeColorAttributeCount);
	return CXTrue;
}

CXBool CXNetworkSetDenseEdgeOrder(CXNetworkRef network, const CXIndex *order, CXSize count) {
	if (!network) {
		return CXFalse;
	}
	if (!CXNetworkSetDenseOrder(&network->edgeDenseOrder, &network->edgeDenseOrderCount, &network->edgeDenseOrderCapacity, order, count)) {
		return CXFalse;
	}
	CXNetworkMarkDenseBuffersDirty(network->edgeDenseBuffers, network->edgeDenseBufferCount);
	network->edgeIndexDense.dirty = CXTrue;
	CXNetworkMarkDenseColorBuffersDirty(network->edgeColorAttributes, network->edgeColorAttributeCount);
	return CXTrue;
}

CXBool CXNetworkGetNodeValidRange(CXNetworkRef network, CXSize *start, CXSize *end) {
	if (!network) {
		return CXFalse;
	}
	if (network->nodeValidRangeDirty) {
		if (!CXNetworkRecomputeValidRange(network->nodeActive, network->nodeCapacity, &network->nodeValidStart, &network->nodeValidEnd)) {
			return CXFalse;
		}
		network->nodeValidRangeDirty = CXFalse;
	}
	if (start) {
		*start = network->nodeValidStart;
	}
	if (end) {
		*end = network->nodeValidEnd;
	}
	return CXTrue;
}

CXBool CXNetworkGetEdgeValidRange(CXNetworkRef network, CXSize *start, CXSize *end) {
	if (!network) {
		return CXFalse;
	}
	if (network->edgeValidRangeDirty) {
		if (!CXNetworkRecomputeValidRange(network->edgeActive, network->edgeCapacity, &network->edgeValidStart, &network->edgeValidEnd)) {
			return CXFalse;
		}
		network->edgeValidRangeDirty = CXFalse;
	}
	if (start) {
		*start = network->edgeValidStart;
	}
	if (end) {
		*end = network->edgeValidEnd;
	}
	return CXTrue;
}

// -----------------------------------------------------------------------------
// Selector utilities
// -----------------------------------------------------------------------------

/** Allocates a selector and optionally reserves storage for indices. */
static CXSelector* CXSelectorCreateInternal(CXSize initialCapacity) {
	CXSelector *selector = calloc(1, sizeof(CXSelector));
	if (!selector) {
		return NULL;
	}
	if (initialCapacity > 0) {
		selector->indices = malloc(sizeof(CXIndex) * initialCapacity);
		if (!selector->indices) {
			free(selector);
			return NULL;
		}
		selector->capacity = initialCapacity;
	}
	return selector;
}

/** Releases the selector and its backing storage. */
static void CXSelectorDestroyInternal(CXSelector *selector) {
	if (!selector) {
		return;
	}
	if (selector->indices) {
		free(selector->indices);
	}
	free(selector);
}

/** Ensures the selector can store at least `capacity` indices. */
static CXBool CXSelectorEnsureCapacity(CXSelector *selector, CXSize capacity) {
	if (!selector) {
		return CXFalse;
	}
	if (capacity <= selector->capacity) {
		return CXTrue;
	}
	CXSize newCapacity = selector->capacity > 0 ? selector->capacity : 4;
	while (newCapacity < capacity) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < capacity) {
			newCapacity = capacity;
			break;
		}
	}
	CXIndex *newIndices = realloc(selector->indices, sizeof(CXIndex) * newCapacity);
	if (!newIndices) {
		return CXFalse;
	}
	selector->indices = newIndices;
	selector->capacity = newCapacity;
	return CXTrue;
}

/** Populates the selector with every active index reported by `activity`. */
static CXBool CXSelectorFillAll(CXSelector *selector, const CXBool *activity, CXSize capacity) {
	if (!selector || !activity) {
		return CXFalse;
	}
	CXSize required = 0;
	for (CXSize idx = 0; idx < capacity; idx++) {
		if (activity[idx]) {
			required++;
		}
	}
	if (!CXSelectorEnsureCapacity(selector, required)) {
		return CXFalse;
	}
	selector->count = 0;
	for (CXSize idx = 0; idx < capacity; idx++) {
		if (activity[idx]) {
			selector->indices[selector->count++] = idx;
		}
	}
	return CXTrue;
}

/** Copies the provided indices into the selector, resizing as needed. */
static CXBool CXSelectorFillFromArrayInternal(CXSelector *selector, const CXIndex *indices, CXSize count) {
	if (!selector || !indices) {
		return CXFalse;
	}
	if (!CXSelectorEnsureCapacity(selector, count)) {
		return CXFalse;
	}
	memcpy(selector->indices, indices, sizeof(CXIndex) * count);
	selector->count = count;
	return CXTrue;
}

/** Allocates a node selector with optional preallocated capacity. */
CXNodeSelectorRef CXNodeSelectorCreate(CXSize initialCapacity) {
	return CXSelectorCreateInternal(initialCapacity);
}

/** Releases the selector and its backing storage. */
void CXNodeSelectorDestroy(CXNodeSelectorRef selector) {
	CXSelectorDestroyInternal(selector);
}

/** Populates the selector with all active node indices. */
CXBool CXNodeSelectorFillAll(CXNodeSelectorRef selector, CXNetworkRef network) {
	if (!network || !selector) {
		return CXFalse;
	}
	return CXSelectorFillAll(selector, network->nodeActive, network->nodeCapacity);
}

/** Copies the provided list of indices into the selector. */
CXBool CXNodeSelectorFillFromArray(CXNodeSelectorRef selector, const CXIndex *indices, CXSize count) {
	return CXSelectorFillFromArrayInternal(selector, indices, count);
}

/** Returns a pointer to the contiguous array of node indices. */
CXIndex* CXNodeSelectorData(CXNodeSelectorRef selector) {
	return selector ? selector->indices : NULL;
}

/** Returns how many entries are currently stored in the selector. */
CXSize CXNodeSelectorCount(CXNodeSelectorRef selector) {
	return selector ? selector->count : 0;
}

/** Allocates an edge selector with optional preallocated capacity. */
CXEdgeSelectorRef CXEdgeSelectorCreate(CXSize initialCapacity) {
	return CXSelectorCreateInternal(initialCapacity);
}

/** Releases the selector and its backing storage. */
void CXEdgeSelectorDestroy(CXEdgeSelectorRef selector) {
	CXSelectorDestroyInternal(selector);
}

/** Populates the selector with all active edge indices. */
CXBool CXEdgeSelectorFillAll(CXEdgeSelectorRef selector, CXNetworkRef network) {
	if (!network || !selector) {
		return CXFalse;
	}
	return CXSelectorFillAll(selector, network->edgeActive, network->edgeCapacity);
}

/** Copies the provided list of edge indices into the selector. */
CXBool CXEdgeSelectorFillFromArray(CXEdgeSelectorRef selector, const CXIndex *indices, CXSize count) {
	return CXSelectorFillFromArrayInternal(selector, indices, count);
}

/** Returns a pointer to the contiguous array of edge indices. */
CXIndex* CXEdgeSelectorData(CXEdgeSelectorRef selector) {
	return selector ? selector->indices : NULL;
}

/** Returns how many entries are currently stored in the selector. */
CXSize CXEdgeSelectorCount(CXEdgeSelectorRef selector) {
	return selector ? selector->count : 0;
}
