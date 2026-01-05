#include "CXNetworkBXNet.h"

#include "CXNetwork.h"
#include "CXNeighborStorage.h"
#include "CXDictionary.h"

#include "htslib/bgzf.h"


#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define CX_NETWORK_MAGIC_BYTES "ZXNETFMT"
#define CX_NETWORK_FOOTER_MAGIC_BYTES "ZXFOOTER"

enum {
	CX_NETWORK_SERIAL_VERSION_MAJOR = 1,
	CX_NETWORK_SERIAL_VERSION_MINOR = 0,
	CX_NETWORK_SERIAL_VERSION_PATCH = 0
};

enum {
	CX_ATTR_FLAG_HAS_DICTIONARY = 1u << 0,
	CX_ATTR_FLAG_HAS_JAVASCRIPT_SHADOW = 1u << 1,
	CX_ATTR_FLAG_POINTER_PAYLOAD = 1u << 2
};

typedef struct {
	void *context;
	CXBool (*write)(void *ctx, const void *data, size_t length);
	int64_t (*tell)(void *ctx);
	CXBool (*flush)(void *ctx);
	uint32_t *crc;
} CXOutputStream;

typedef struct {
	const char *name;
	CXAttributeRef attribute;
	uint32_t storageWidth;
	uint16_t flags;
} CXAttributeEntry;

typedef struct {
	CXAttributeEntry *items;
	size_t count;
	size_t capacity;
} CXAttributeList;

typedef struct {
	uint32_t chunkId;
	uint32_t flags;
	uint64_t offset;
	uint64_t length;
} CXWrittenChunk;

typedef struct {
	CXWrittenChunk *items;
	size_t count;
	size_t capacity;
} CXWrittenChunkList;

typedef struct {
	CXOutputStream *stream;
	uint64_t expectedBytes;
	uint64_t writtenBytes;
} CXSizedWriterContext;

static uint64_t CXSizedBlockLength(uint64_t payload) {
	return sizeof(uint64_t) + payload;
}

static uint16_t cx_read_u16le(const uint8_t *src) {
	return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static uint32_t cx_read_u32le(const uint8_t *src) {
	return ((uint32_t)src[0]) |
		((uint32_t)src[1] << 8) |
		((uint32_t)src[2] << 16) |
		((uint32_t)src[3] << 24);
}

static uint64_t cx_read_u64le(const uint8_t *src) {
	return ((uint64_t)src[0]) |
		((uint64_t)src[1] << 8) |
		((uint64_t)src[2] << 16) |
		((uint64_t)src[3] << 24) |
		((uint64_t)src[4] << 32) |
		((uint64_t)src[5] << 40) |
		((uint64_t)src[6] << 48) |
		((uint64_t)src[7] << 56);
}

static void cx_write_u16le(uint16_t value, uint8_t *dst) {
	dst[0] = (uint8_t)(value & 0xFF);
	dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void cx_write_u32le(uint32_t value, uint8_t *dst) {
	dst[0] = (uint8_t)(value & 0xFF);
	dst[1] = (uint8_t)((value >> 8) & 0xFF);
	dst[2] = (uint8_t)((value >> 16) & 0xFF);
	dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

static void cx_write_u64le(uint64_t value, uint8_t *dst) {
	for (int i = 0; i < 8; i++) {
		dst[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
	}
}

typedef struct {
	void *context;
	ssize_t (*read)(void *ctx, void *buffer, size_t length);
	int64_t (*tell)(void *ctx);
	CXBool (*seek)(void *ctx, int64_t offset, int whence);
	uint32_t *crc;
} CXInputStream;

typedef struct {
	char *name;
	CXAttributeRef attribute;
	CXAttributeType type;
	uint32_t storageWidth;
	uint32_t dimension;
	uint64_t capacity;
} CXAttributeLoadEntry;

typedef struct {
	CXAttributeLoadEntry *items;
	size_t count;
	size_t capacity;
} CXAttributeLoadList;

typedef enum {
	CXAttributeScopeNode,
	CXAttributeScopeEdge,
	CXAttributeScopeNetwork
} CXAttributeScope;

typedef struct {
	CXBool isDirected;
	uint64_t nodeCount;
	uint64_t edgeCount;
	uint64_t nodeCapacity;
	uint64_t edgeCapacity;
	uint64_t nodeAttributeCount;
	uint64_t edgeAttributeCount;
	uint64_t networkAttributeCount;
} CXMetaChunkPayload;

typedef struct {
	uint16_t versionMajor;
	uint16_t versionMinor;
	uint32_t versionPatch;
	uint32_t codec;
	uint32_t flags;
	uint64_t nodeCount;
	uint64_t edgeCount;
	uint64_t nodeCapacity;
	uint64_t edgeCapacity;
} CXParsedHeader;

static CXBool CXFileWrite(void *ctx, const void *data, size_t length) {
	FILE *fp = (FILE *)ctx;
	return fp && fwrite(data, 1, length, fp) == length;
}

static int64_t CXFileTell(void *ctx) {
	FILE *fp = (FILE *)ctx;
	if (!fp) {
		return -1;
	}
#if defined(_WIN32)
	return _ftelli64(fp);
#else
	long long position = ftello(fp);
	return position >= 0 ? position : -1;
#endif
}

static CXBool CXFileFlush(void *ctx) {
	FILE *fp = (FILE *)ctx;
	return fp && fflush(fp) == 0;
}

static CXBool CXBGZFWrite(void *ctx, const void *data, size_t length) {
	BGZF *bgzf = (BGZF *)ctx;
	if (!bgzf) {
		return CXFalse;
	}
	return bgzf_write(bgzf, data, length) == (ssize_t)length ? CXTrue : CXFalse;
}

static int64_t CXBGZFTell(void *ctx) {
	BGZF *bgzf = (BGZF *)ctx;
	if (!bgzf) {
		return -1;
	}
	int64_t position = bgzf_tell(bgzf);
	return position >= 0 ? position : -1;
}

static CXBool CXBGZFFlush(void *ctx) {
	BGZF *bgzf = (BGZF *)ctx;
	return bgzf && bgzf_flush(bgzf) == 0;
}

static CXBool CXOutputStreamWrite(CXOutputStream *stream, const void *data, size_t length) {
	if (!stream || !stream->write) {
		return CXFalse;
	}
	if (!stream->write(stream->context, data, length)) {
		return CXFalse;
	}
	if (stream->crc) {
		*stream->crc = (uint32_t)crc32(*stream->crc, (const unsigned char *)data, (uInt)length);
	}
	return CXTrue;
}

static CXBool CXAttributeListReserve(CXAttributeList *list, size_t desired) {
	if (list->capacity >= desired) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 4;
	while (newCapacity < desired) {
		newCapacity *= 2;
	}
	CXAttributeEntry *newItems = realloc(list->items, newCapacity * sizeof(CXAttributeEntry));
	if (!newItems) {
		return CXFalse;
	}
	list->items = newItems;
	list->capacity = newCapacity;
	return CXTrue;
}

static int CXAttributeEntryCompare(const void *lhs, const void *rhs) {
	const CXAttributeEntry *a = (const CXAttributeEntry *)lhs;
	const CXAttributeEntry *b = (const CXAttributeEntry *)rhs;
	return strcmp(a->name, b->name);
}

static CXBool CXAttributeStorageInfo(CXAttributeRef attribute, uint32_t *storageWidth, uint16_t *flags) {
	if (!attribute || !storageWidth || !flags) {
		return CXFalse;
	}

	uint32_t width = attribute->elementSize;
	uint16_t flagBits = 0;

	switch (attribute->type) {
		case CXStringAttributeType:
			width = 0;
			break;
		case CXDataAttributeType:
		case CXJavascriptAttributeType:
			return CXFalse;
		case CXBooleanAttributeType:
		case CXFloatAttributeType:
		case CXDoubleAttributeType:
		case CXIntegerAttributeType:
		case CXUnsignedIntegerAttributeType:
		case CXBigIntegerAttributeType:
		case CXUnsignedBigIntegerAttributeType:
		case CXDataAttributeCategoryType:
			break;
		default:
			return CXFalse;
	}

	if (attribute->usesJavascriptShadow) {
		flagBits |= CX_ATTR_FLAG_HAS_JAVASCRIPT_SHADOW;
	}

	if (attribute->categoricalDictionary) {
		// Only flag as having dictionary if entries exist.
		if (CXStringDictionaryCount(attribute->categoricalDictionary) > 0) {
			flagBits |= CX_ATTR_FLAG_HAS_DICTIONARY;
		}
	}

	*storageWidth = width;
	*flags = flagBits;
	return CXTrue;
}

static CXBool CXExpectedStorageWidthForType(CXAttributeType type, uint32_t *outWidth) {
	if (!outWidth) {
		return CXFalse;
	}
	switch (type) {
		case CXStringAttributeType:
			*outWidth = 0;
			return CXTrue;
		case CXBooleanAttributeType:
			*outWidth = 1;
			return CXTrue;
		case CXFloatAttributeType:
			*outWidth = sizeof(float);
			return CXTrue;
		case CXDoubleAttributeType:
			*outWidth = sizeof(double);
			return CXTrue;
		case CXIntegerAttributeType:
			*outWidth = sizeof(int32_t);
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
			*outWidth = sizeof(uint32_t);
			return CXTrue;
		case CXBigIntegerAttributeType:
			*outWidth = sizeof(int64_t);
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			*outWidth = sizeof(uint64_t);
			return CXTrue;
		case CXDataAttributeCategoryType:
			*outWidth = sizeof(uint32_t);
			return CXTrue;
		default:
			errno = ENOTSUP;
			return CXFalse;
	}
}

static CXBool CXAttributeListCollect(CXStringDictionaryRef dictionary, CXAttributeList *outList) {
	if (!outList) {
		return CXFalse;
	}
	memset(outList, 0, sizeof(*outList));
	if (!dictionary) {
		return CXTrue;
	}

	size_t anticipated = (size_t)CXStringDictionaryCount(dictionary);
	if (anticipated > 0 && !CXAttributeListReserve(outList, anticipated)) {
		return CXFalse;
	}

	CXStringDictionaryFOR(entry, dictionary) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		uint32_t storageWidth = 0;
		uint16_t flags = 0;
		if (!CXAttributeStorageInfo(attribute, &storageWidth, &flags)) {
			return CXFalse;
		}
		if (!CXAttributeListReserve(outList, outList->count + 1)) {
			return CXFalse;
		}
		outList->items[outList->count].name = entry->key;
		outList->items[outList->count].attribute = attribute;
		outList->items[outList->count].storageWidth = storageWidth;
		outList->items[outList->count].flags = flags;
		outList->count += 1;
	}

	if (outList->count > 1) {
		qsort(outList->items, outList->count, sizeof(CXAttributeEntry), CXAttributeEntryCompare);
	}

	return CXTrue;
}

static void CXAttributeListDestroy(CXAttributeList *list) {
	if (!list) {
		return;
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static CXBool CXAttributeLoadListReserve(CXAttributeLoadList *list, size_t desired) {
	if (list->capacity >= desired) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 4;
	while (newCapacity < desired) {
		newCapacity *= 2;
	}
	CXAttributeLoadEntry *newItems = realloc(list->items, newCapacity * sizeof(CXAttributeLoadEntry));
	if (!newItems) {
		return CXFalse;
	}
	list->items = newItems;
	list->capacity = newCapacity;
	return CXTrue;
}

static CXAttributeLoadEntry* CXAttributeLoadListFind(CXAttributeLoadList *list, const char *name) {
	if (!list || !name) {
		return NULL;
	}
	for (size_t idx = 0; idx < list->count; idx++) {
		if (strcmp(list->items[idx].name, name) == 0) {
			return &list->items[idx];
		}
	}
	return NULL;
}

static void CXAttributeLoadListDestroy(CXAttributeLoadList *list) {
	if (!list) {
		return;
	}
	for (size_t idx = 0; idx < list->count; idx++) {
		free(list->items[idx].name);
		list->items[idx].name = NULL;
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static ssize_t CXFileRead(void *ctx, void *buffer, size_t length) {
	FILE *fp = (FILE *)ctx;
	if (!fp) {
		return -1;
	}
	size_t readCount = fread(buffer, 1, length, fp);
	if (readCount == length) {
		return (ssize_t)readCount;
	}
	if (ferror(fp)) {
		return -1;
	}
	return (ssize_t)readCount;
}

static CXBool CXFileSeek(void *ctx, int64_t offset, int whence) {
	FILE *fp = (FILE *)ctx;
	if (!fp) {
		return CXFalse;
	}
#if defined(_WIN32)
	return _fseeki64(fp, offset, whence) == 0 ? CXTrue : CXFalse;
#else
	return fseeko(fp, offset, whence) == 0 ? CXTrue : CXFalse;
#endif
}

static ssize_t CXBGZFRead(void *ctx, void *buffer, size_t length) {
	BGZF *bgzf = (BGZF *)ctx;
	if (!bgzf) {
		return -1;
	}
	return bgzf_read(bgzf, buffer, length);
}

static CXBool CXBGZFSeek(void *ctx, int64_t offset, int whence) {
	BGZF *bgzf = (BGZF *)ctx;
	if (!bgzf) {
		return CXFalse;
	}
	return bgzf_seek(bgzf, offset, whence) >= 0 ? CXTrue : CXFalse;
}

static CXBool CXReadExact(CXInputStream *stream, void *buffer, size_t length) {
	if (!stream || !stream->read) {
		return CXFalse;
	}
	uint8_t *cursor = (uint8_t *)buffer;
	size_t remaining = length;
	while (remaining > 0) {
		ssize_t bytesRead = stream->read(stream->context, cursor, remaining);
		if (bytesRead <= 0) {
			return CXFalse;
		}
		if (stream->crc) {
			*stream->crc = (uint32_t)crc32(*stream->crc, cursor, (uInt)bytesRead);
		}
		cursor += bytesRead;
		remaining -= (size_t)bytesRead;
	}
	return CXTrue;
}

static CXBool CXSkipExact(CXInputStream *stream, uint64_t length) {
	uint8_t scratch[1024];
	while (length > 0) {
		size_t chunk = length > sizeof(scratch) ? sizeof(scratch) : (size_t)length;
		if (!CXReadExact(stream, scratch, chunk)) {
			return CXFalse;
		}
		length -= chunk;
	}
	return CXTrue;
}

static CXBool CXReadU32(CXInputStream *stream, uint32_t *outValue) {
	uint8_t buffer[4];
	if (!CXReadExact(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}
	*outValue = cx_read_u32le(buffer);
	return CXTrue;
}

static CXBool CXReadU64(CXInputStream *stream, uint64_t *outValue) {
	uint8_t buffer[8];
	if (!CXReadExact(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}
	*outValue = cx_read_u64le(buffer);
	return CXTrue;
}

static CXBool CXReadSizedBlockLength(CXInputStream *stream, uint64_t *outLength) {
	return CXReadU64(stream, outLength);
}

static CXBool CXReadChunkHeader(CXInputStream *stream, uint32_t *chunkId, uint32_t *flags, uint64_t *payloadSize) {
	uint8_t buffer[16];
	if (!CXReadExact(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}
	if (chunkId) {
		*chunkId = cx_read_u32le(buffer);
	}
	if (flags) {
		*flags = cx_read_u32le(buffer + 4);
	}
	if (payloadSize) {
		*payloadSize = cx_read_u64le(buffer + 8);
	}
	return CXTrue;
}

static CXBool CXReadHeaderBlock(CXInputStream *stream, CXParsedHeader *outHeader) {
	uint8_t buffer[CX_NETWORK_FILE_HEADER_SIZE];
	if (!CXReadExact(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}
	if (memcmp(buffer, CX_NETWORK_MAGIC_BYTES, sizeof(CX_NETWORK_MAGIC_BYTES) - 1) != 0) {
		errno = EINVAL;
		return CXFalse;
	}

	uint16_t versionMajor = cx_read_u16le(buffer + 8);
	uint16_t versionMinor = cx_read_u16le(buffer + 10);
	uint32_t versionPatch = cx_read_u32le(buffer + 12);
	if (versionMajor != CX_NETWORK_SERIAL_VERSION_MAJOR || versionMinor != CX_NETWORK_SERIAL_VERSION_MINOR || versionPatch != CX_NETWORK_SERIAL_VERSION_PATCH) {
		errno = ENOTSUP;
		return CXFalse;
	}

	if (outHeader) {
		outHeader->versionMajor = versionMajor;
		outHeader->versionMinor = versionMinor;
		outHeader->versionPatch = versionPatch;
		outHeader->codec = cx_read_u32le(buffer + 16);
		outHeader->flags = cx_read_u32le(buffer + 20);
		outHeader->nodeCount = cx_read_u64le(buffer + 32);
		outHeader->edgeCount = cx_read_u64le(buffer + 40);
		outHeader->nodeCapacity = cx_read_u64le(buffer + 48);
		outHeader->edgeCapacity = cx_read_u64le(buffer + 56);
	}
	return CXTrue;
}

static CXBool CXReadMetaChunk(CXInputStream *stream, uint64_t payloadSize, CXMetaChunkPayload *outMeta) {
	if (!stream || !outMeta) {
		return CXFalse;
	}
	if (payloadSize != CXSizedBlockLength(64)) {
		errno = EINVAL;
		return CXFalse;
	}

	uint64_t blockSize = 0;
	if (!CXReadSizedBlockLength(stream, &blockSize)) {
		return CXFalse;
	}
	if (blockSize != 64) {
		errno = EINVAL;
		return CXFalse;
	}

	uint8_t buffer[64];
	if (!CXReadExact(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}

	CXMetaChunkPayload meta = {0};
	meta.isDirected = buffer[0] ? CXTrue : CXFalse;
	meta.nodeCount = cx_read_u64le(buffer + 8);
	meta.edgeCount = cx_read_u64le(buffer + 16);
	meta.nodeCapacity = cx_read_u64le(buffer + 24);
	meta.edgeCapacity = cx_read_u64le(buffer + 32);
	meta.nodeAttributeCount = cx_read_u64le(buffer + 40);
	meta.edgeAttributeCount = cx_read_u64le(buffer + 48);
	meta.networkAttributeCount = cx_read_u64le(buffer + 56);

	*outMeta = meta;
	return CXTrue;
}

static CXBool CXReadNodeChunk(CXInputStream *stream, uint64_t payloadSize, CXNetworkRef network) {
	if (!stream || !network) {
		return CXFalse;
	}
	uint64_t expectedPayload = CXSizedBlockLength((uint64_t)network->nodeCapacity);
	if (payloadSize != expectedPayload) {
		errno = EINVAL;
		return CXFalse;
	}

	uint64_t blockSize = 0;
	if (!CXReadSizedBlockLength(stream, &blockSize)) {
		return CXFalse;
	}
	if (blockSize != (uint64_t)network->nodeCapacity) {
		errno = EINVAL;
		return CXFalse;
	}

	if (!CXReadExact(stream, network->nodeActive, (size_t)blockSize)) {
		return CXFalse;
	}
	return CXTrue;
}

static CXBool CXReadEdgeChunk(CXInputStream *stream, uint64_t payloadSize, CXNetworkRef network) {
	if (!stream || !network) {
		return CXFalse;
	}
	if (network->edgeCapacity > 0 && (uint64_t)network->edgeCapacity > UINT64_MAX / 16u) {
		errno = ERANGE;
		return CXFalse;
	}
	uint64_t expectedPayload = CXSizedBlockLength((uint64_t)network->edgeCapacity) + CXSizedBlockLength((uint64_t)network->edgeCapacity * 16u);
	if (payloadSize != expectedPayload) {
		errno = EINVAL;
		return CXFalse;
	}

	uint64_t blockSize = 0;
	if (!CXReadSizedBlockLength(stream, &blockSize)) {
		return CXFalse;
	}
	if (blockSize != (uint64_t)network->edgeCapacity) {
		errno = EINVAL;
		return CXFalse;
	}
	if (!CXReadExact(stream, network->edgeActive, (size_t)blockSize)) {
		return CXFalse;
	}

	if (!CXReadSizedBlockLength(stream, &blockSize)) {
		return CXFalse;
	}
	uint64_t expectedEdgeBytes = (uint64_t)network->edgeCapacity * 16u;
	if (blockSize != expectedEdgeBytes) {
		errno = EINVAL;
		return CXFalse;
	}

	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		uint8_t buffer[16];
		if (!CXReadExact(stream, buffer, sizeof(buffer))) {
			return CXFalse;
		}
		uint64_t from = cx_read_u64le(buffer + 0);
		uint64_t to = cx_read_u64le(buffer + 8);
		if (from > (uint64_t)CXIndexMAX || to > (uint64_t)CXIndexMAX) {
			errno = ERANGE;
			return CXFalse;
		}
		network->edges[idx].from = (CXUInteger)from;
		network->edges[idx].to = (CXUInteger)to;
	}
	return CXTrue;
}

static CXAttributeRef CXDefineAttributeForScope(CXNetworkRef network, CXAttributeScope scope, char *name, CXAttributeType type, uint32_t dimension) {
	switch (scope) {
		case CXAttributeScopeNode:
			if (!CXNetworkDefineNodeAttribute(network, name, type, dimension)) {
				return NULL;
			}
			return CXNetworkGetNodeAttribute(network, name);
		case CXAttributeScopeEdge:
			if (!CXNetworkDefineEdgeAttribute(network, name, type, dimension)) {
				return NULL;
			}
			return CXNetworkGetEdgeAttribute(network, name);
		case CXAttributeScopeNetwork:
			if (!CXNetworkDefineNetworkAttribute(network, name, type, dimension)) {
				return NULL;
			}
			return CXNetworkGetNetworkAttribute(network, name);
		default:
			return NULL;
	}
}

static uint64_t CXExpectedAttributeCapacity(CXNetworkRef network, CXAttributeScope scope) {
	switch (scope) {
		case CXAttributeScopeNode:
			return (uint64_t)network->nodeCapacity;
		case CXAttributeScopeEdge:
			return (uint64_t)network->edgeCapacity;
		case CXAttributeScopeNetwork:
			return 1;
		default:
			return 0;
	}
}

static CXBool CXReadAttributeDefinitionsChunk(CXInputStream *stream, uint64_t payloadSize, CXNetworkRef network, CXAttributeScope scope, CXAttributeLoadList *outList) {
	if (!stream || !network || !outList) {
		return CXFalse;
	}
	uint64_t remaining = payloadSize;

	uint64_t blockSize = 0;
	if (!CXReadSizedBlockLength(stream, &blockSize)) {
		return CXFalse;
	}
	if (blockSize != 8) {
		errno = EINVAL;
		return CXFalse;
	}
	uint8_t countBuffer[8];
	if (!CXReadExact(stream, countBuffer, sizeof(countBuffer))) {
		return CXFalse;
	}
	uint32_t attributeCount = cx_read_u32le(countBuffer);
	remaining -= CXSizedBlockLength(8);

	if (!CXAttributeLoadListReserve(outList, outList->count + attributeCount)) {
		return CXFalse;
	}

	for (uint32_t idx = 0; idx < attributeCount; idx++) {
		uint64_t nameSize = 0;
		if (!CXReadSizedBlockLength(stream, &nameSize)) {
			return CXFalse;
		}
		if (nameSize == 0 || nameSize > SIZE_MAX - 1) {
			errno = EINVAL;
			return CXFalse;
		}
		char *name = malloc((size_t)nameSize + 1);
		if (!name) {
			return CXFalse;
		}
		if (!CXReadExact(stream, name, (size_t)nameSize)) {
			free(name);
			return CXFalse;
		}
		name[nameSize] = '\0';

		uint64_t descriptorSize = 0;
		if (!CXReadSizedBlockLength(stream, &descriptorSize)) {
			free(name);
			return CXFalse;
		}
		if (descriptorSize != 24) {
			free(name);
			errno = EINVAL;
			return CXFalse;
		}

		uint8_t descriptor[24];
		if (!CXReadExact(stream, descriptor, sizeof(descriptor))) {
			free(name);
			return CXFalse;
		}
		CXAttributeType type = (CXAttributeType)descriptor[0];
		uint16_t flags = cx_read_u16le(descriptor + 2);
		if (flags != 0) {
			free(name);
			errno = ENOTSUP;
			return CXFalse;
		}
		uint32_t dimension = cx_read_u32le(descriptor + 4);
		uint32_t storageWidth = cx_read_u32le(descriptor + 8);
		uint64_t capacity = cx_read_u64le(descriptor + 16);

		uint32_t expectedWidth = 0;
		if (!CXExpectedStorageWidthForType(type, &expectedWidth)) {
			free(name);
			return CXFalse;
		}
		if (storageWidth != expectedWidth) {
			free(name);
			errno = EINVAL;
			return CXFalse;
		}

		uint64_t expectedCapacity = CXExpectedAttributeCapacity(network, scope);
		if (capacity != expectedCapacity) {
			free(name);
			errno = EINVAL;
			return CXFalse;
		}

		uint64_t dictionarySize = 0;
		if (!CXReadSizedBlockLength(stream, &dictionarySize)) {
			free(name);
			return CXFalse;
		}
		if (dictionarySize != 0) {
			free(name);
			errno = ENOTSUP;
			return CXFalse;
		}

		CXAttributeRef attribute = CXDefineAttributeForScope(network, scope, name, type, dimension);
		if (!attribute) {
			free(name);
			return CXFalse;
		}
		if (!CXAttributeLoadListReserve(outList, outList->count + 1)) {
			free(name);
			return CXFalse;
		}
		CXAttributeLoadEntry *entry = &outList->items[outList->count++];
		entry->name = name;
		entry->attribute = attribute;
		entry->type = type;
		entry->storageWidth = storageWidth;
		entry->dimension = dimension;
		entry->capacity = capacity;

		remaining -= CXSizedBlockLength(nameSize);
		remaining -= CXSizedBlockLength(descriptorSize);
		remaining -= CXSizedBlockLength(0);
	}

	if (remaining != 0) {
		errno = EINVAL;
		return CXFalse;
	}
	return CXTrue;
}

static CXBool CXReadStringAttributeValues(CXInputStream *stream, CXAttributeLoadEntry *plan, uint64_t valueBytes) {
	if (!stream || !plan || !plan->attribute) {
		return CXFalse;
	}
	uint64_t capacity = plan->capacity;
	uint64_t dimension = plan->attribute->dimension > 0 ? (uint64_t)plan->attribute->dimension : 1;
	uint8_t *destination = (uint8_t *)plan->attribute->data;
	if (capacity > 0 && !destination) {
		errno = EINVAL;
		return CXFalse;
	}

	uint64_t consumed = 0;
	for (uint64_t idx = 0; idx < capacity; idx++) {
		CXString *entryBase = destination ? (CXString *)(destination + ((size_t)idx * plan->attribute->stride)) : NULL;
		for (uint64_t dim = 0; dim < dimension; dim++) {
			if (consumed + sizeof(uint32_t) > valueBytes) {
				errno = EINVAL;
				return CXFalse;
			}
			uint8_t lengthBytes[4];
			if (!CXReadExact(stream, lengthBytes, sizeof(lengthBytes))) {
				return CXFalse;
			}
			consumed += sizeof(uint32_t);
			uint32_t length = cx_read_u32le(lengthBytes);
			CXString *slot = entryBase ? &entryBase[dim] : NULL;
			if (slot && *slot) {
				free(*slot);
				*slot = NULL;
			}
			if (length == UINT32_MAX) {
				if (slot) {
					*slot = NULL;
				}
				continue;
			}
			if ((uint64_t)length > valueBytes - consumed) {
				errno = EINVAL;
				return CXFalse;
			}
			char *buffer = malloc((size_t)length + 1);
			if (!buffer) {
				errno = ENOMEM;
				return CXFalse;
			}
			if (length > 0 && !CXReadExact(stream, buffer, length)) {
				free(buffer);
				return CXFalse;
			}
			buffer[length] = '\0';
			if (slot) {
				*slot = buffer;
			} else {
				free(buffer);
			}
			consumed += length;
		}
	}

	if (consumed != valueBytes) {
		errno = EINVAL;
		return CXFalse;
	}
	return CXTrue;
}

static CXBool CXReadAttributeValuesIntoPlan(CXInputStream *stream, CXAttributeLoadEntry *plan, uint64_t valueBytes) {
	if (!stream || !plan || !plan->attribute) {
		return CXFalse;
	}
	if (plan->type == CXStringAttributeType) {
		return CXReadStringAttributeValues(stream, plan, valueBytes);
	}
	uint64_t capacity = plan->capacity;
	uint64_t dimension = plan->dimension;
	if (dimension != 0 && capacity > UINT64_MAX / dimension) {
		errno = ERANGE;
		return CXFalse;
	}
	uint64_t elementCount = capacity * dimension;
	if (plan->storageWidth != 0 && elementCount > UINT64_MAX / plan->storageWidth) {
		errno = ERANGE;
		return CXFalse;
	}
	uint64_t expectedBytes = elementCount * plan->storageWidth;
	if (valueBytes != expectedBytes) {
		errno = EINVAL;
		return CXFalse;
	}

	uint8_t *destination = (uint8_t *)plan->attribute->data;
	if (!destination) {
		errno = EINVAL;
		return CXFalse;
	}

	for (uint64_t idx = 0; idx < capacity; idx++) {
		uint8_t *entryBase = destination + (size_t)idx * plan->attribute->stride;
		for (uint64_t dim = 0; dim < dimension; dim++) {
			uint8_t buffer[8] = {0};
			if (!CXReadExact(stream, buffer, plan->storageWidth)) {
				return CXFalse;
			}
			uint8_t *target = entryBase + (size_t)dim * plan->attribute->elementSize;
			switch (plan->type) {
				case CXBooleanAttributeType:
					target[0] = buffer[0];
					break;
				case CXFloatAttributeType: {
					uint32_t bits = cx_read_u32le(buffer);
					float value;
					memcpy(&value, &bits, sizeof(value));
					memcpy(target, &value, sizeof(value));
					break;
				}
				case CXDoubleAttributeType: {
					uint64_t bits = cx_read_u64le(buffer);
					double value;
					memcpy(&value, &bits, sizeof(value));
					memcpy(target, &value, sizeof(value));
					break;
				}
				case CXIntegerAttributeType: {
					int32_t value = (int32_t)cx_read_u32le(buffer);
					memcpy(target, &value, sizeof(value));
					break;
				}
				case CXUnsignedIntegerAttributeType: {
					uint32_t value = cx_read_u32le(buffer);
					memcpy(target, &value, sizeof(value));
					break;
				}
				case CXBigIntegerAttributeType: {
					int64_t value = (int64_t)cx_read_u64le(buffer);
					memcpy(target, &value, sizeof(value));
					break;
				}
				case CXUnsignedBigIntegerAttributeType: {
					uint64_t value = cx_read_u64le(buffer);
					memcpy(target, &value, sizeof(value));
					break;
				}
				case CXDataAttributeCategoryType: {
					uint32_t value = cx_read_u32le(buffer);
					memcpy(target, &value, sizeof(value));
					break;
				}
				default:
					errno = ENOTSUP;
					return CXFalse;
			}
		}
	}
	return CXTrue;
}

static CXBool CXReadAttributeValuesChunk(CXInputStream *stream, uint64_t payloadSize, CXAttributeLoadList *list) {
	if (!stream || !list) {
		return CXFalse;
	}
	uint64_t remaining = payloadSize;

	uint64_t blockSize = 0;
	if (!CXReadSizedBlockLength(stream, &blockSize)) {
		return CXFalse;
	}
	if (blockSize != 8) {
		errno = EINVAL;
		return CXFalse;
	}
	uint8_t countBuffer[8];
	if (!CXReadExact(stream, countBuffer, sizeof(countBuffer))) {
		return CXFalse;
	}
	uint32_t attributeCount = cx_read_u32le(countBuffer);
	if (attributeCount != list->count) {
		errno = EINVAL;
		return CXFalse;
	}
	remaining -= CXSizedBlockLength(8);

	for (uint32_t idx = 0; idx < attributeCount; idx++) {
		uint64_t nameSize = 0;
		if (!CXReadSizedBlockLength(stream, &nameSize)) {
			return CXFalse;
		}
		if (nameSize == 0 || nameSize > SIZE_MAX - 1) {
			errno = EINVAL;
			return CXFalse;
		}
		char *name = malloc((size_t)nameSize + 1);
		if (!name) {
			return CXFalse;
		}
		if (!CXReadExact(stream, name, (size_t)nameSize)) {
			free(name);
			return CXFalse;
		}
		name[nameSize] = '\0';

		uint64_t valueSize = 0;
		if (!CXReadSizedBlockLength(stream, &valueSize)) {
			free(name);
			return CXFalse;
		}

		CXAttributeLoadEntry *entry = CXAttributeLoadListFind(list, name);
		free(name);
		if (!entry) {
			errno = EINVAL;
			return CXFalse;
		}

		if (!CXReadAttributeValuesIntoPlan(stream, entry, valueSize)) {
			return CXFalse;
		}

		remaining -= CXSizedBlockLength(nameSize);
		remaining -= CXSizedBlockLength(valueSize);
	}

	if (remaining != 0) {
		errno = EINVAL;
		return CXFalse;
	}
	return CXTrue;
}

static CXSize CXCountActive(const CXBool *bitmap, CXSize capacity) {
	CXSize count = 0;
	for (CXSize idx = 0; idx < capacity; idx++) {
		if (bitmap[idx]) {
			count++;
		}
	}
	return count;
}

static CXBool CXRebuildIndexManager(CXIndexManagerRef manager, const CXBool *active, CXSize capacity) {
	if (!manager) {
		return CXFalse;
	}
	if (!CXResizeIndexManager(manager, capacity)) {
		return CXFalse;
	}
	CXIndexManagerReset(manager);
	CXBool hasActive = CXFalse;
	CXIndex nextIndex = 0;
	for (CXSize idx = 0; idx < capacity; idx++) {
		if (active[idx]) {
			hasActive = CXTrue;
			nextIndex = (CXIndex)(idx + 1);
		} else {
			CXIndexManagerAddIndex(manager, (CXIndex)idx);
		}
	}
	if (!hasActive) {
		nextIndex = 0;
	}
	if ((CXSize)nextIndex > capacity) {
		nextIndex = (CXIndex)capacity;
	}
	manager->nextIndex = nextIndex;
	return CXTrue;
}

static CXBool CXRebuildAdjacency(CXNetworkRef network) {
	if (!network) {
		return CXFalse;
	}
	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		if (!network->edgeActive[idx]) {
			continue;
		}
		CXEdge edge = network->edges[idx];
		if ((CXSize)edge.from >= network->nodeCapacity || (CXSize)edge.to >= network->nodeCapacity) {
			errno = EINVAL;
			return CXFalse;
		}
		if (!CXNeighborContainerAdd(&network->nodes[edge.from].outNeighbors, edge.to, (CXIndex)idx)) {
			return CXFalse;
		}
		if (!CXNeighborContainerAdd(&network->nodes[edge.to].inNeighbors, edge.from, (CXIndex)idx)) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool CXReadFooter(CXInputStream *stream, const CXMetaChunkPayload *meta, uint32_t expectedChecksum) {
	if (!stream || !meta) {
		return CXFalse;
	}
	uint8_t buffer[CX_NETWORK_FILE_FOOTER_SIZE];
	if (!CXReadExact(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}
	if (memcmp(buffer, CX_NETWORK_FOOTER_MAGIC_BYTES, sizeof(CX_NETWORK_FOOTER_MAGIC_BYTES) - 1) != 0) {
		errno = EINVAL;
		return CXFalse;
	}

	uint32_t chunkCount = cx_read_u32le(buffer + 8);
	(void)chunkCount; // Currently unused but reserved for validation later.

	size_t countBase = 16 + CX_NETWORK_FOOTER_MAX_LOCATORS * 24;
	uint64_t nodeCount = cx_read_u64le(buffer + countBase + 0);
	uint64_t edgeCount = cx_read_u64le(buffer + countBase + 8);
	uint64_t nodeAttrCount = cx_read_u64le(buffer + countBase + 16);
	uint64_t edgeAttrCount = cx_read_u64le(buffer + countBase + 24);
	uint64_t networkAttrCount = cx_read_u64le(buffer + countBase + 32);
	uint32_t checksum = cx_read_u32le(buffer + countBase + 40);

	if (nodeCount != meta->nodeCount || edgeCount != meta->edgeCount ||
		nodeAttrCount != meta->nodeAttributeCount ||
		edgeAttrCount != meta->edgeAttributeCount ||
		networkAttrCount != meta->networkAttributeCount) {
		errno = EINVAL;
		return CXFalse;
	}

	if (checksum != expectedChecksum) {
		errno = EIO;
		return CXFalse;
	}

	return CXTrue;
}

static CXBool CXWrittenChunkListReserve(CXWrittenChunkList *list, size_t desired) {
	if (list->capacity >= desired) {
		return CXTrue;
	}
	size_t newCapacity = list->capacity > 0 ? list->capacity : 4;
	while (newCapacity < desired) {
		newCapacity *= 2;
	}
	CXWrittenChunk *newItems = realloc(list->items, newCapacity * sizeof(CXWrittenChunk));
	if (!newItems) {
		return CXFalse;
	}
	list->items = newItems;
	list->capacity = newCapacity;
	return CXTrue;
}

static CXBool CXWrittenChunkListAppend(CXWrittenChunkList *list, uint32_t chunkId, uint32_t flags, uint64_t offset, uint64_t length) {
	if (!list) {
		return CXFalse;
	}
	if (list->count >= (size_t)CX_NETWORK_FOOTER_MAX_LOCATORS) {
		return CXFalse;
	}
	if (!CXWrittenChunkListReserve(list, list->count + 1)) {
		return CXFalse;
	}
	list->items[list->count].chunkId = chunkId;
	list->items[list->count].flags = flags;
	list->items[list->count].offset = offset;
	list->items[list->count].length = length;
	list->count += 1;
	return CXTrue;
}

static void CXWrittenChunkListDestroy(CXWrittenChunkList *list) {
	if (!list) {
		return;
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static CXBool CXWriteSizedRaw(CXOutputStream *stream, const void *data, uint64_t size) {
	uint8_t lengthBytes[8];
	cx_write_u64le(size, lengthBytes);
	if (!CXOutputStreamWrite(stream, lengthBytes, sizeof(lengthBytes))) {
		return CXFalse;
	}
	if (size == 0 || !data) {
		return CXTrue;
	}
	const uint8_t *cursor = (const uint8_t *)data;
	uint64_t remaining = size;
	while (remaining > 0) {
		size_t chunk = remaining > 1 << 20 ? (1 << 20) : (size_t)remaining;
		if (!CXOutputStreamWrite(stream, cursor, chunk)) {
			return CXFalse;
		}
		cursor += chunk;
		remaining -= chunk;
	}
	return CXTrue;
}

static CXBool CXWriteChunkHeader(CXOutputStream *stream, uint32_t chunkId, uint32_t flags, uint64_t payloadSize) {
	uint8_t buffer[16];
	cx_write_u32le(chunkId, buffer + 0);
	cx_write_u32le(flags, buffer + 4);
	cx_write_u64le(payloadSize, buffer + 8);
	return CXOutputStreamWrite(stream, buffer, sizeof(buffer));
}

typedef CXBool (*CXSizedPayloadWriter)(CXSizedWriterContext *context, void *userData);

static CXBool CXWriteSizedPayload(CXOutputStream *stream, uint64_t payloadSize, CXSizedPayloadWriter writer, void *userData) {
	uint8_t lengthBytes[8];
	cx_write_u64le(payloadSize, lengthBytes);
	if (!CXOutputStreamWrite(stream, lengthBytes, sizeof(lengthBytes))) {
		return CXFalse;
	}
	if (payloadSize == 0) {
		return CXTrue;
	}
	CXSizedWriterContext ctx = {
		.stream = stream,
		.expectedBytes = payloadSize,
		.writtenBytes = 0
	};
	if (!writer(&ctx, userData)) {
		return CXFalse;
	}
	return ctx.writtenBytes == ctx.expectedBytes ? CXTrue : CXFalse;
}

static CXBool CXSizedWriteBytes(CXSizedWriterContext *context, const void *data, size_t length) {
	if (!context || !context->stream) {
		return CXFalse;
	}
	if (context->writtenBytes + length > context->expectedBytes) {
		return CXFalse;
	}
	if (!CXOutputStreamWrite(context->stream, data, length)) {
		return CXFalse;
	}
	context->writtenBytes += length;
	return CXTrue;
}

typedef struct {
	const CXAttributeEntry *entry;
} CXAttributeValueWriterContext;

static CXBool CXWriteAttributeValuesCallback(CXSizedWriterContext *context, void *userData) {
	CXAttributeValueWriterContext *valueCtx = (CXAttributeValueWriterContext *)userData;
	if (!context || !valueCtx || !valueCtx->entry) {
		return CXFalse;
	}
	CXAttributeRef attribute = valueCtx->entry->attribute;
	if (!attribute || !attribute->data) {
		return CXFalse;
	}

	const uint8_t *base = (const uint8_t *)attribute->data;
	uint8_t scratch[4096];
	size_t scratchSize = sizeof(scratch);
	size_t scratchOffset = 0;

	for (CXSize idx = 0; idx < attribute->capacity; idx++) {
		const uint8_t *entryBase = base + ((size_t)idx * attribute->stride);
		for (CXSize dim = 0; dim < attribute->dimension; dim++) {
			const uint8_t *elementPtr = entryBase + ((size_t)dim * attribute->elementSize);
			uint8_t encoded[8];
			size_t encodedSize = valueCtx->entry->storageWidth;

			switch (attribute->type) {
				case CXStringAttributeType: {
					if (scratchOffset > 0) {
						if (!CXSizedWriteBytes(context, scratch, scratchOffset)) {
							return CXFalse;
						}
						scratchOffset = 0;
					}
					const CXString *strings = (const CXString *)(entryBase);
					CXString value = strings ? strings[dim] : NULL;
					uint8_t lengthBytes[4];
					if (!value) {
						cx_write_u32le(UINT32_MAX, lengthBytes);
						if (!CXSizedWriteBytes(context, lengthBytes, sizeof(lengthBytes))) {
							return CXFalse;
						}
						continue;
					}
					size_t length = strlen(value);
					if ((uint64_t)length >= (uint64_t)UINT32_MAX) {
						errno = ERANGE;
						return CXFalse;
					}
					cx_write_u32le((uint32_t)length, lengthBytes);
					if (!CXSizedWriteBytes(context, lengthBytes, sizeof(lengthBytes))) {
						return CXFalse;
					}
					if (length > 0 && !CXSizedWriteBytes(context, value, length)) {
						return CXFalse;
					}
					continue;
				}
				case CXBooleanAttributeType:
					encoded[0] = *elementPtr;
					break;
				case CXFloatAttributeType: {
					union {
						float f;
						uint32_t u;
					} converter;
					memcpy(&converter.f, elementPtr, sizeof(float));
					cx_write_u32le(converter.u, encoded);
					break;
				}
				case CXDoubleAttributeType: {
					union {
						double d;
						uint64_t u;
					} converter;
					memcpy(&converter.d, elementPtr, sizeof(double));
					cx_write_u64le(converter.u, encoded);
					break;
				}
				case CXIntegerAttributeType: {
					union {
						int32_t s;
						uint32_t u;
					} converter;
					memcpy(&converter.s, elementPtr, sizeof(int32_t));
					cx_write_u32le(converter.u, encoded);
					break;
				}
				case CXUnsignedIntegerAttributeType: {
					uint32_t value = 0;
					memcpy(&value, elementPtr, sizeof(uint32_t));
					cx_write_u32le(value, encoded);
					break;
				}
				case CXBigIntegerAttributeType: {
					union {
						int64_t s;
						uint64_t u;
					} converter;
					memcpy(&converter.s, elementPtr, sizeof(int64_t));
					cx_write_u64le(converter.u, encoded);
					break;
				}
				case CXUnsignedBigIntegerAttributeType: {
					uint64_t value = 0;
					memcpy(&value, elementPtr, sizeof(uint64_t));
					cx_write_u64le(value, encoded);
					break;
				}
				case CXDataAttributeCategoryType: {
					uint32_t value = 0;
					memcpy(&value, elementPtr, sizeof(uint32_t));
					cx_write_u32le(value, encoded);
					break;
				}
				default:
					return CXFalse;
			}

			if (attribute->type == CXBooleanAttributeType) {
				if (scratchOffset == scratchSize) {
					if (!CXSizedWriteBytes(context, scratch, scratchOffset)) {
						return CXFalse;
					}
					scratchOffset = 0;
				}
				scratch[scratchOffset++] = encoded[0];
			} else {
				size_t required = encodedSize;
				if (scratchOffset + required > scratchSize) {
					if (!CXSizedWriteBytes(context, scratch, scratchOffset)) {
						return CXFalse;
					}
					scratchOffset = 0;
					if (required > scratchSize) {
						if (!CXSizedWriteBytes(context, encoded, required)) {
							return CXFalse;
						}
						continue;
					}
				}
				memcpy(scratch + scratchOffset, encoded, required);
				scratchOffset += required;
			}
		}
	}

	if (scratchOffset > 0) {
		if (!CXSizedWriteBytes(context, scratch, scratchOffset)) {
			return CXFalse;
		}
	}

	return CXTrue;
}

static CXBool CXWriteAttributeDefinitionsChunk(CXOutputStream *stream, CXWrittenChunkList *chunks, uint32_t chunkId, const CXAttributeList *attributes) {
	const size_t attributeCount = attributes ? attributes->count : 0;
	uint64_t payloadSize = CXSizedBlockLength(sizeof(uint32_t) * 2);

	for (size_t idx = 0; idx < attributeCount; idx++) {
		const CXAttributeEntry *entry = &attributes->items[idx];
		size_t nameLen = strlen(entry->name);
		payloadSize += CXSizedBlockLength((uint64_t)nameLen);
		payloadSize += CXSizedBlockLength(24);
		payloadSize += CXSizedBlockLength(0);
	}

	int64_t chunkOffset = stream->tell(stream->context);
	if (chunkOffset < 0) {
		return CXFalse;
	}

	if (!CXWriteChunkHeader(stream, chunkId, 0, payloadSize)) {
		return CXFalse;
	}

	uint8_t countData[8] = {0};
	cx_write_u32le((uint32_t)attributeCount, countData + 0);
	cx_write_u32le(0, countData + 4);
	if (!CXWriteSizedRaw(stream, countData, sizeof(countData))) {
		return CXFalse;
	}

	for (size_t idx = 0; idx < attributeCount; idx++) {
		const CXAttributeEntry *entry = &attributes->items[idx];
		size_t nameLen = strlen(entry->name);
		if (!CXWriteSizedRaw(stream, entry->name, (uint64_t)nameLen)) {
			return CXFalse;
		}

		uint8_t descriptor[24] = {0};
		descriptor[0] = (uint8_t)entry->attribute->type;
		descriptor[1] = 0;
		cx_write_u16le(entry->flags, descriptor + 2);
		cx_write_u32le((uint32_t)entry->attribute->dimension, descriptor + 4);
		cx_write_u32le(entry->storageWidth, descriptor + 8);
		cx_write_u32le(0, descriptor + 12);
		cx_write_u64le((uint64_t)entry->attribute->capacity, descriptor + 16);
		if (!CXWriteSizedRaw(stream, descriptor, sizeof(descriptor))) {
			return CXFalse;
		}

		if (!CXWriteSizedRaw(stream, NULL, 0)) {
			return CXFalse;
		}
	}

	if (!CXWrittenChunkListAppend(chunks, chunkId, 0, (uint64_t)chunkOffset, payloadSize)) {
		return CXFalse;
	}

	return CXTrue;
}

static CXBool CXWriteAttributeValuesChunk(CXOutputStream *stream, CXWrittenChunkList *chunks, uint32_t chunkId, const CXAttributeList *attributes) {
	const size_t attributeCount = attributes ? attributes->count : 0;
	uint64_t payloadSize = CXSizedBlockLength(sizeof(uint32_t) * 2);
	uint64_t *valueSizes = NULL;
	CXBool success = CXFalse;
	if (attributeCount > 0) {
		valueSizes = calloc(attributeCount, sizeof(uint64_t));
		if (!valueSizes) {
			goto cleanup;
		}
	}

	for (size_t idx = 0; idx < attributeCount; idx++) {
		const CXAttributeEntry *entry = &attributes->items[idx];
		size_t nameLen = strlen(entry->name);
		uint64_t capacity = (uint64_t)entry->attribute->capacity;
		uint64_t dimension = (uint64_t)entry->attribute->dimension;
		if (dimension != 0 && capacity > UINT64_MAX / dimension) {
			errno = ERANGE;
			goto cleanup;
		}
		uint64_t elementCount = capacity * dimension;
		uint64_t valueBytes = 0;
		if (entry->attribute->type == CXStringAttributeType) {
			const uint8_t *base = (const uint8_t *)entry->attribute->data;
			for (CXSize capIdx = 0; capIdx < entry->attribute->capacity; capIdx++) {
				const CXString *strings = base ? (const CXString *)(base + ((size_t)capIdx * entry->attribute->stride)) : NULL;
				for (CXSize dimIdx = 0; dimIdx < entry->attribute->dimension; dimIdx++) {
					CXString value = strings ? strings[dimIdx] : NULL;
					uint64_t addition = sizeof(uint32_t);
					if (value) {
						size_t length = strlen(value);
						if ((uint64_t)length >= (uint64_t)UINT32_MAX) {
							errno = ERANGE;
							goto cleanup;
						}
						addition += (uint64_t)length;
					}
					if (valueBytes > UINT64_MAX - addition) {
						errno = ERANGE;
						goto cleanup;
					}
					valueBytes += addition;
				}
			}
		} else {
			if (entry->storageWidth != 0 && elementCount > UINT64_MAX / entry->storageWidth) {
				errno = ERANGE;
				goto cleanup;
			}
			valueBytes = elementCount * entry->storageWidth;
		}
		if (valueSizes) {
			valueSizes[idx] = valueBytes;
		}
		payloadSize += CXSizedBlockLength((uint64_t)nameLen);
		payloadSize += CXSizedBlockLength(valueBytes);
	}

	int64_t chunkOffset = stream->tell(stream->context);
	if (chunkOffset < 0) {
		goto cleanup;
	}

	if (!CXWriteChunkHeader(stream, chunkId, 0, payloadSize)) {
		goto cleanup;
	}

	uint8_t countData[8] = {0};
	cx_write_u32le((uint32_t)attributeCount, countData + 0);
	cx_write_u32le(0, countData + 4);
	if (!CXWriteSizedRaw(stream, countData, sizeof(countData))) {
		goto cleanup;
	}

	for (size_t idx = 0; idx < attributeCount; idx++) {
		const CXAttributeEntry *entry = &attributes->items[idx];
		size_t nameLen = strlen(entry->name);
		uint64_t valueBytes = valueSizes ? valueSizes[idx] : 0;

		if (!CXWriteSizedRaw(stream, entry->name, (uint64_t)nameLen)) {
			goto cleanup;
		}

		CXAttributeValueWriterContext valueCtx = { entry };
		if (!CXWriteSizedPayload(stream, valueBytes, CXWriteAttributeValuesCallback, &valueCtx)) {
			goto cleanup;
		}
	}

	if (!CXWrittenChunkListAppend(chunks, chunkId, 0, (uint64_t)chunkOffset, payloadSize)) {
		goto cleanup;
	}
	success = CXTrue;

cleanup:
	free(valueSizes);
	return success;
}

static CXBool CXWriteMetaChunk(CXOutputStream *stream, CXWrittenChunkList *chunks, const CXMetaChunkPayload *payload) {
	if (!payload) {
		return CXFalse;
	}
	uint64_t chunkPayloadBytes = CXSizedBlockLength(64);

	int64_t chunkOffset = stream->tell(stream->context);
	if (chunkOffset < 0) {
		return CXFalse;
	}

	if (!CXWriteChunkHeader(stream, CX_NETWORK_CHUNK_META, 0, chunkPayloadBytes)) {
		return CXFalse;
	}

	uint8_t buffer[64] = {0};
	buffer[0] = payload->isDirected ? 1 : 0;
	cx_write_u64le(payload->nodeCount, buffer + 8);
	cx_write_u64le(payload->edgeCount, buffer + 16);
	cx_write_u64le(payload->nodeCapacity, buffer + 24);
	cx_write_u64le(payload->edgeCapacity, buffer + 32);
	cx_write_u64le(payload->nodeAttributeCount, buffer + 40);
	cx_write_u64le(payload->edgeAttributeCount, buffer + 48);
	cx_write_u64le(payload->networkAttributeCount, buffer + 56);

	if (!CXWriteSizedRaw(stream, buffer, sizeof(buffer))) {
		return CXFalse;
	}

	if (!CXWrittenChunkListAppend(chunks, CX_NETWORK_CHUNK_META, 0, (uint64_t)chunkOffset, chunkPayloadBytes)) {
		return CXFalse;
	}

	return CXTrue;
}

static CXBool CXWriteNodeChunk(CXOutputStream *stream, CXWrittenChunkList *chunks, CXNetworkRef network) {
	if (!stream || !network) {
		return CXFalse;
	}
	uint64_t activeBytes = (uint64_t)network->nodeCapacity;
	uint64_t chunkPayload = CXSizedBlockLength(activeBytes);

	int64_t chunkOffset = stream->tell(stream->context);
	if (chunkOffset < 0) {
		return CXFalse;
	}

	if (!CXWriteChunkHeader(stream, CX_NETWORK_CHUNK_NODE, 0, chunkPayload)) {
		return CXFalse;
	}

	if (!CXWriteSizedRaw(stream, network->nodeActive, activeBytes)) {
		return CXFalse;
	}

	if (!CXWrittenChunkListAppend(chunks, CX_NETWORK_CHUNK_NODE, 0, (uint64_t)chunkOffset, chunkPayload)) {
		return CXFalse;
	}
	return CXTrue;
}

typedef struct {
	const CXNetwork *network;
} CXEdgeWriterContext;

static CXBool CXWriteEdgesCallback(CXSizedWriterContext *context, void *userData) {
	CXEdgeWriterContext *edgeCtx = (CXEdgeWriterContext *)userData;
	if (!context || !edgeCtx || !edgeCtx->network) {
		return CXFalse;
	}

	const CXNetwork *network = edgeCtx->network;
	uint8_t scratch[4096];
	size_t scratchOffset = 0;

	for (CXSize idx = 0; idx < network->edgeCapacity; idx++) {
		CXEdge edge = network->edges[idx];
		uint8_t encoded[16];
		cx_write_u64le((uint64_t)edge.from, encoded + 0);
		cx_write_u64le((uint64_t)edge.to, encoded + 8);

		if (scratchOffset + sizeof(encoded) > sizeof(scratch)) {
			if (!CXSizedWriteBytes(context, scratch, scratchOffset)) {
				return CXFalse;
			}
			scratchOffset = 0;
		}
		memcpy(scratch + scratchOffset, encoded, sizeof(encoded));
		scratchOffset += sizeof(encoded);
	}

	if (scratchOffset > 0) {
		if (!CXSizedWriteBytes(context, scratch, scratchOffset)) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool CXWriteEdgeChunk(CXOutputStream *stream, CXWrittenChunkList *chunks, CXNetworkRef network) {
	if (!stream || !network) {
		return CXFalse;
	}
	uint64_t activityBytes = (uint64_t)network->edgeCapacity;
	if (network->edgeCapacity > 0 && (uint64_t)network->edgeCapacity > UINT64_MAX / 16u) {
		errno = ERANGE;
		return CXFalse;
	}
	uint64_t edgeBytes = (uint64_t)network->edgeCapacity * 16u;
	uint64_t chunkPayload = CXSizedBlockLength(activityBytes) + CXSizedBlockLength(edgeBytes);

	int64_t chunkOffset = stream->tell(stream->context);
	if (chunkOffset < 0) {
		return CXFalse;
	}

	if (!CXWriteChunkHeader(stream, CX_NETWORK_CHUNK_EDGE, 0, chunkPayload)) {
		return CXFalse;
	}

	if (!CXWriteSizedRaw(stream, network->edgeActive, activityBytes)) {
		return CXFalse;
	}

	CXEdgeWriterContext ctx = { network };
	if (!CXWriteSizedPayload(stream, edgeBytes, CXWriteEdgesCallback, &ctx)) {
		return CXFalse;
	}

	if (!CXWrittenChunkListAppend(chunks, CX_NETWORK_CHUNK_EDGE, 0, (uint64_t)chunkOffset, chunkPayload)) {
		return CXFalse;
	}

	return CXTrue;
}

static CXBool CXWriteHeader(CXOutputStream *stream, CXNetworkRef network, CXNetworkStorageCodec codec) {
	if (!stream || !network) {
		return CXFalse;
	}
	uint8_t header[CX_NETWORK_FILE_HEADER_SIZE] = {0};
	memcpy(header, CX_NETWORK_MAGIC_BYTES, sizeof(CX_NETWORK_MAGIC_BYTES) - 1);
	cx_write_u16le(CX_NETWORK_SERIAL_VERSION_MAJOR, header + 8);
	cx_write_u16le(CX_NETWORK_SERIAL_VERSION_MINOR, header + 10);
	cx_write_u32le(CX_NETWORK_SERIAL_VERSION_PATCH, header + 12);
	cx_write_u32le((uint32_t)codec, header + 16);

	uint32_t flags = network->isDirected ? 1u : 0u;
	cx_write_u32le(flags, header + 20);
	cx_write_u32le(0, header + 24);
	cx_write_u32le(0, header + 28);

	cx_write_u64le((uint64_t)network->nodeCount, header + 32);
	cx_write_u64le((uint64_t)network->edgeCount, header + 40);
	cx_write_u64le((uint64_t)network->nodeCapacity, header + 48);
	cx_write_u64le((uint64_t)network->edgeCapacity, header + 56);

	return CXOutputStreamWrite(stream, header, sizeof(header));
}

static CXBool CXWriteFooter(CXOutputStream *stream, const CXWrittenChunkList *chunks, uint32_t checksum, const CXMetaChunkPayload *meta) {
	if (!stream || !chunks || !meta) {
		return CXFalse;
	}

	uint8_t footer[CX_NETWORK_FILE_FOOTER_SIZE] = {0};
	memcpy(footer, CX_NETWORK_FOOTER_MAGIC_BYTES, sizeof(CX_NETWORK_FOOTER_MAGIC_BYTES) - 1);

	uint32_t chunkCount = (uint32_t)chunks->count;
	cx_write_u32le(chunkCount, footer + 8);
	cx_write_u32le(0, footer + 12);

	for (size_t idx = 0; idx < CX_NETWORK_FOOTER_MAX_LOCATORS; idx++) {
		size_t base = 16 + idx * 24;
		if (idx < chunks->count) {
			const CXWrittenChunk *chunk = &chunks->items[idx];
			cx_write_u32le(chunk->chunkId, footer + base + 0);
			cx_write_u32le(chunk->flags, footer + base + 4);
			cx_write_u64le(chunk->offset, footer + base + 8);
			cx_write_u64le(chunk->length, footer + base + 16);
		} else {
			cx_write_u32le(0, footer + base + 0);
			cx_write_u32le(0, footer + base + 4);
			cx_write_u64le(0, footer + base + 8);
			cx_write_u64le(0, footer + base + 16);
		}
	}

	size_t countBase = 16 + CX_NETWORK_FOOTER_MAX_LOCATORS * 24;
	cx_write_u64le(meta->nodeCount, footer + countBase + 0);
	cx_write_u64le(meta->edgeCount, footer + countBase + 8);
	cx_write_u64le(meta->nodeAttributeCount, footer + countBase + 16);
	cx_write_u64le(meta->edgeAttributeCount, footer + countBase + 24);
	cx_write_u64le(meta->networkAttributeCount, footer + countBase + 32);

	cx_write_u32le(checksum, footer + countBase + 40);
	cx_write_u32le(0, footer + countBase + 44);
	// reserved tail is already zeroed

	return CXOutputStreamWrite(stream, footer, sizeof(footer));
}

static CXBool CXNetworkSerialize(CXNetworkRef network, CXOutputStream *stream, CXNetworkStorageCodec codec) {
	if (!network || !stream) {
		return CXFalse;
	}

	CXBool result = CXFalse;
	CXAttributeList nodeAttributes = {0};
	CXAttributeList edgeAttributes = {0};
	CXAttributeList networkAttributes = {0};
	CXWrittenChunkList chunkList = {0};
	uint32_t checksum = crc32(0L, Z_NULL, 0);

	stream->crc = &checksum;

	if (!CXAttributeListCollect(network->nodeAttributes, &nodeAttributes)) {
		errno = ENOTSUP;
		goto cleanup;
	}
	if (!CXAttributeListCollect(network->edgeAttributes, &edgeAttributes)) {
		errno = ENOTSUP;
		goto cleanup;
	}
	if (!CXAttributeListCollect(network->networkAttributes, &networkAttributes)) {
		errno = ENOTSUP;
		goto cleanup;
	}

	for (size_t idx = 0; idx < nodeAttributes.count; idx++) {
		if (nodeAttributes.items[idx].flags & (CX_ATTR_FLAG_HAS_DICTIONARY | CX_ATTR_FLAG_HAS_JAVASCRIPT_SHADOW | CX_ATTR_FLAG_POINTER_PAYLOAD)) {
			errno = ENOTSUP;
			goto cleanup;
		}
	}
	for (size_t idx = 0; idx < edgeAttributes.count; idx++) {
		if (edgeAttributes.items[idx].flags & (CX_ATTR_FLAG_HAS_DICTIONARY | CX_ATTR_FLAG_HAS_JAVASCRIPT_SHADOW | CX_ATTR_FLAG_POINTER_PAYLOAD)) {
			errno = ENOTSUP;
			goto cleanup;
		}
	}
	for (size_t idx = 0; idx < networkAttributes.count; idx++) {
		if (networkAttributes.items[idx].flags & (CX_ATTR_FLAG_HAS_DICTIONARY | CX_ATTR_FLAG_HAS_JAVASCRIPT_SHADOW | CX_ATTR_FLAG_POINTER_PAYLOAD)) {
			errno = ENOTSUP;
			goto cleanup;
		}
	}

	CXMetaChunkPayload meta = {
		.isDirected = network->isDirected,
		.nodeCount = (uint64_t)network->nodeCount,
		.edgeCount = (uint64_t)network->edgeCount,
		.nodeCapacity = (uint64_t)network->nodeCapacity,
		.edgeCapacity = (uint64_t)network->edgeCapacity,
		.nodeAttributeCount = (uint64_t)nodeAttributes.count,
		.edgeAttributeCount = (uint64_t)edgeAttributes.count,
		.networkAttributeCount = (uint64_t)networkAttributes.count
	};

	if (!CXWriteHeader(stream, network, codec)) {
		goto cleanup;
	}

	if (!CXWriteMetaChunk(stream, &chunkList, &meta)) {
		goto cleanup;
	}
	if (!CXWriteNodeChunk(stream, &chunkList, network)) {
		goto cleanup;
	}
	if (!CXWriteEdgeChunk(stream, &chunkList, network)) {
		goto cleanup;
	}
	if (!CXWriteAttributeDefinitionsChunk(stream, &chunkList, CX_NETWORK_CHUNK_NODE_ATTR, &nodeAttributes)) {
		goto cleanup;
	}
	if (!CXWriteAttributeDefinitionsChunk(stream, &chunkList, CX_NETWORK_CHUNK_EDGE_ATTR, &edgeAttributes)) {
		goto cleanup;
	}
	if (!CXWriteAttributeDefinitionsChunk(stream, &chunkList, CX_NETWORK_CHUNK_NET_ATTR, &networkAttributes)) {
		goto cleanup;
	}
	if (!CXWriteAttributeValuesChunk(stream, &chunkList, CX_NETWORK_CHUNK_NODE_VALUES, &nodeAttributes)) {
		goto cleanup;
	}
	if (!CXWriteAttributeValuesChunk(stream, &chunkList, CX_NETWORK_CHUNK_EDGE_VALUES, &edgeAttributes)) {
		goto cleanup;
	}
	if (!CXWriteAttributeValuesChunk(stream, &chunkList, CX_NETWORK_CHUNK_NET_VALUES, &networkAttributes)) {
		goto cleanup;
	}

	uint32_t finalChecksum = checksum;
	stream->crc = NULL;

	if (!CXWriteFooter(stream, &chunkList, finalChecksum, &meta)) {
		goto cleanup;
	}

	if (stream->flush && !stream->flush(stream->context)) {
		goto cleanup;
	}

	result = CXTrue;

cleanup:
	stream->crc = NULL;
	CXAttributeListDestroy(&nodeAttributes);
	CXAttributeListDestroy(&edgeAttributes);
	CXAttributeListDestroy(&networkAttributes);
	CXWrittenChunkListDestroy(&chunkList);
	return result;
}

CXBool CXNetworkWriteBXNet(CXNetworkRef network, const char *path) {
	if (!network || !path) {
		errno = EINVAL;
		return CXFalse;
	}
	FILE *fp = fopen(path, "wb");
	if (!fp) {
		return CXFalse;
	}

	CXOutputStream stream = {
		.context = fp,
		.write = CXFileWrite,
		.tell = CXFileTell,
		.flush = CXFileFlush,
		.crc = NULL
	};

	CXBool ok = CXNetworkSerialize(network, &stream, CXNetworkStorageCodecBinary);
	int savedErr = errno;
	if (fclose(fp) != 0) {
		int closeErr = errno;
		remove(path);
		errno = closeErr;
		return CXFalse;
	}
	if (!ok) {
		remove(path);
		errno = savedErr;
		return CXFalse;
	}
	errno = savedErr;
	return CXTrue;
}

CXBool CXNetworkWriteZXNet(CXNetworkRef network, const char *path, int compressionLevel) {
    if (!network || !path) {
        errno = EINVAL;
        return CXFalse;
    }
    int level = compressionLevel;
    if (level < 0) level = 0;
    if (level > 9) level = 9;

    char mode[4] = {0};
    if (snprintf(mode, sizeof(mode), "w%d", level) < 0) {
        errno = EINVAL;
        return CXFalse;
    }

    BGZF *bgzf = bgzf_open(path, mode);
    if (!bgzf) {
        return CXFalse;
    }

    CXOutputStream stream = {
        .context = bgzf,
        .write = CXBGZFWrite,
        .tell = CXBGZFTell,
        .flush = CXBGZFFlush,
        .crc = NULL
    };

    CXBool ok = CXNetworkSerialize(network, &stream, CXNetworkStorageCodecBGZF);
    int savedErr = errno;
    if (bgzf_close(bgzf) != 0) {
        int closeErr = errno;
        remove(path);
        errno = closeErr;
        return CXFalse;
    }
    if (!ok) {
        remove(path);
        errno = savedErr;
        return CXFalse;
    }
    errno = savedErr;
    return CXTrue;
}

struct CXNetwork* CXNetworkReadBXNet(const char *path) {
	if (!path) {
		errno = EINVAL;
		return NULL;
	}

	FILE *fp = fopen(path, "rb");
	if (!fp) {
		return NULL;
	}

	CXInputStream stream = {
		.context = fp,
		.read = CXFileRead,
		.tell = CXFileTell,
		.seek = CXFileSeek,
		.crc = NULL
	};

	uint32_t checksum = crc32(0L, Z_NULL, 0);
	stream.crc = &checksum;

	CXParsedHeader header = {0};
	if (!CXReadHeaderBlock(&stream, &header)) {
		fclose(fp);
		return NULL;
	}
	if (header.codec != (uint32_t)CXNetworkStorageCodecBinary) {
		fclose(fp);
		errno = EINVAL;
		return NULL;
	}
	if (header.nodeCapacity > (uint64_t)CXSizeMAX || header.edgeCapacity > (uint64_t)CXSizeMAX) {
		fclose(fp);
		errno = ERANGE;
		return NULL;
	}
	if (header.nodeCount > header.nodeCapacity || header.edgeCount > header.edgeCapacity) {
		fclose(fp);
		errno = EINVAL;
		return NULL;
	}

	CXNetworkRef network = CXNewNetworkWithCapacity((header.flags & 1u) ? CXTrue : CXFalse, (CXSize)header.nodeCapacity, (CXSize)header.edgeCapacity);
	if (!network) {
		fclose(fp);
		return NULL;
	}

	CXAttributeLoadList nodeAttributes = {0};
	CXAttributeLoadList edgeAttributes = {0};
	CXAttributeLoadList networkAttributes = {0};
	CXMetaChunkPayload meta = {0};

	const uint32_t expectedChunks[] = {
		CX_NETWORK_CHUNK_META,
		CX_NETWORK_CHUNK_NODE,
		CX_NETWORK_CHUNK_EDGE,
		CX_NETWORK_CHUNK_NODE_ATTR,
		CX_NETWORK_CHUNK_EDGE_ATTR,
		CX_NETWORK_CHUNK_NET_ATTR,
		CX_NETWORK_CHUNK_NODE_VALUES,
		CX_NETWORK_CHUNK_EDGE_VALUES,
		CX_NETWORK_CHUNK_NET_VALUES
	};

	for (size_t idx = 0; idx < sizeof(expectedChunks) / sizeof(expectedChunks[0]); idx++) {
		uint32_t chunkId = 0;
		uint32_t flags = 0;
		uint64_t payloadSize = 0;
		if (!CXReadChunkHeader(&stream, &chunkId, &flags, &payloadSize)) {
			goto read_fail;
		}
		if (chunkId != expectedChunks[idx]) {
			errno = EINVAL;
			goto read_fail;
		}
		(void)flags; // currently unused

		switch (chunkId) {
			case CX_NETWORK_CHUNK_META:
				if (!CXReadMetaChunk(&stream, payloadSize, &meta)) {
					goto read_fail;
				}
				if (meta.nodeCount != header.nodeCount || meta.edgeCount != header.edgeCount ||
					meta.nodeCapacity != header.nodeCapacity || meta.edgeCapacity != header.edgeCapacity) {
					errno = EINVAL;
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_NODE:
				if (!CXReadNodeChunk(&stream, payloadSize, network)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_EDGE:
				if (!CXReadEdgeChunk(&stream, payloadSize, network)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_NODE_ATTR:
				if (!CXReadAttributeDefinitionsChunk(&stream, payloadSize, network, CXAttributeScopeNode, &nodeAttributes)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_EDGE_ATTR:
				if (!CXReadAttributeDefinitionsChunk(&stream, payloadSize, network, CXAttributeScopeEdge, &edgeAttributes)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_NET_ATTR:
				if (!CXReadAttributeDefinitionsChunk(&stream, payloadSize, network, CXAttributeScopeNetwork, &networkAttributes)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_NODE_VALUES:
				if (!CXReadAttributeValuesChunk(&stream, payloadSize, &nodeAttributes)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_EDGE_VALUES:
				if (!CXReadAttributeValuesChunk(&stream, payloadSize, &edgeAttributes)) {
					goto read_fail;
				}
				break;
			case CX_NETWORK_CHUNK_NET_VALUES:
				if (!CXReadAttributeValuesChunk(&stream, payloadSize, &networkAttributes)) {
					goto read_fail;
				}
				break;
			default:
				errno = EINVAL;
				goto read_fail;
		}
	}

	if (meta.nodeCapacity != (uint64_t)network->nodeCapacity || meta.edgeCapacity != (uint64_t)network->edgeCapacity) {
		errno = EINVAL;
		goto read_fail;
	}

	stream.crc = NULL;
	if (!CXReadFooter(&stream, &meta, checksum)) {
		goto read_fail;
	}

	if (meta.nodeCount > (uint64_t)CXSizeMAX || meta.edgeCount > (uint64_t)CXSizeMAX) {
		errno = ERANGE;
		goto read_fail;
	}

	CXSize nodeCount = (CXSize)meta.nodeCount;
	CXSize edgeCount = (CXSize)meta.edgeCount;
	if (CXCountActive(network->nodeActive, network->nodeCapacity) != nodeCount) {
		errno = EINVAL;
		goto read_fail;
	}
	if (CXCountActive(network->edgeActive, network->edgeCapacity) != edgeCount) {
		errno = EINVAL;
		goto read_fail;
	}

	network->nodeCount = nodeCount;
	network->edgeCount = edgeCount;

	if (!CXRebuildIndexManager(network->nodeIndexManager, network->nodeActive, network->nodeCapacity)) {
		goto read_fail;
	}
	if (!CXRebuildIndexManager(network->edgeIndexManager, network->edgeActive, network->edgeCapacity)) {
		goto read_fail;
	}
	if (!CXRebuildAdjacency(network)) {
		goto read_fail;
	}

	CXAttributeLoadListDestroy(&nodeAttributes);
	CXAttributeLoadListDestroy(&edgeAttributes);
	CXAttributeLoadListDestroy(&networkAttributes);

	if (fclose(fp) != 0) {
		CXFreeNetwork(network);
		return NULL;
	}

	return network;

read_fail:
	CXAttributeLoadListDestroy(&nodeAttributes);
	CXAttributeLoadListDestroy(&edgeAttributes);
	CXAttributeLoadListDestroy(&networkAttributes);
	CXFreeNetwork(network);
	fclose(fp);
	return NULL;
}

struct CXNetwork* CXNetworkReadZXNet(const char *path) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    BGZF *bgzf = bgzf_open(path, "r");
    if (!bgzf) {
        return NULL;
    }

    CXInputStream stream = {
        .context = bgzf,
        .read = CXBGZFRead,
        .tell = CXBGZFTell,
        .seek = CXBGZFSeek,
        .crc = NULL
    };

    uint32_t checksum = crc32(0L, Z_NULL, 0);
    stream.crc = &checksum;

    CXParsedHeader header = {0};
    if (!CXReadHeaderBlock(&stream, &header)) {
        bgzf_close(bgzf);
        return NULL;
    }
    if (header.codec != (uint32_t)CXNetworkStorageCodecBGZF) {
        bgzf_close(bgzf);
        errno = EINVAL;
        return NULL;
    }
    if (header.nodeCapacity > (uint64_t)CXSizeMAX || header.edgeCapacity > (uint64_t)CXSizeMAX) {
        bgzf_close(bgzf);
        errno = ERANGE;
        return NULL;
    }
    if (header.nodeCount > header.nodeCapacity || header.edgeCount > header.edgeCapacity) {
        bgzf_close(bgzf);
        errno = EINVAL;
        return NULL;
    }

    CXNetworkRef network = CXNewNetworkWithCapacity((header.flags & 1u) ? CXTrue : CXFalse, (CXSize)header.nodeCapacity, (CXSize)header.edgeCapacity);
    if (!network) {
        bgzf_close(bgzf);
        return NULL;
    }

    CXAttributeLoadList nodeAttributes = {0};
    CXAttributeLoadList edgeAttributes = {0};
    CXAttributeLoadList networkAttributes = {0};
    CXMetaChunkPayload meta = {0};

    const uint32_t expectedChunks[] = {
        CX_NETWORK_CHUNK_META,
        CX_NETWORK_CHUNK_NODE,
        CX_NETWORK_CHUNK_EDGE,
        CX_NETWORK_CHUNK_NODE_ATTR,
        CX_NETWORK_CHUNK_EDGE_ATTR,
        CX_NETWORK_CHUNK_NET_ATTR,
        CX_NETWORK_CHUNK_NODE_VALUES,
        CX_NETWORK_CHUNK_EDGE_VALUES,
        CX_NETWORK_CHUNK_NET_VALUES
    };

    for (size_t idx = 0; idx < sizeof(expectedChunks) / sizeof(expectedChunks[0]); idx++) {
        uint32_t chunkId = 0;
        uint32_t flags = 0;
        uint64_t payloadSize = 0;
        if (!CXReadChunkHeader(&stream, &chunkId, &flags, &payloadSize)) {
            goto read_fail_gz;
        }
        if (chunkId != expectedChunks[idx]) {
            errno = EINVAL;
            goto read_fail_gz;
        }
        (void)flags;

        switch (chunkId) {
            case CX_NETWORK_CHUNK_META:
                if (!CXReadMetaChunk(&stream, payloadSize, &meta)) {
                    goto read_fail_gz;
                }
                if (meta.nodeCount != header.nodeCount || meta.edgeCount != header.edgeCount ||
                    meta.nodeCapacity != header.nodeCapacity || meta.edgeCapacity != header.edgeCapacity) {
                    errno = EINVAL;
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_NODE:
                if (!CXReadNodeChunk(&stream, payloadSize, network)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_EDGE:
                if (!CXReadEdgeChunk(&stream, payloadSize, network)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_NODE_ATTR:
                if (!CXReadAttributeDefinitionsChunk(&stream, payloadSize, network, CXAttributeScopeNode, &nodeAttributes)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_EDGE_ATTR:
                if (!CXReadAttributeDefinitionsChunk(&stream, payloadSize, network, CXAttributeScopeEdge, &edgeAttributes)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_NET_ATTR:
                if (!CXReadAttributeDefinitionsChunk(&stream, payloadSize, network, CXAttributeScopeNetwork, &networkAttributes)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_NODE_VALUES:
                if (!CXReadAttributeValuesChunk(&stream, payloadSize, &nodeAttributes)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_EDGE_VALUES:
                if (!CXReadAttributeValuesChunk(&stream, payloadSize, &edgeAttributes)) {
                    goto read_fail_gz;
                }
                break;
            case CX_NETWORK_CHUNK_NET_VALUES:
                if (!CXReadAttributeValuesChunk(&stream, payloadSize, &networkAttributes)) {
                    goto read_fail_gz;
                }
                break;
            default:
                errno = EINVAL;
                goto read_fail_gz;
        }
    }

    if (meta.nodeCapacity != (uint64_t)network->nodeCapacity || meta.edgeCapacity != (uint64_t)network->edgeCapacity) {
        errno = EINVAL;
        goto read_fail_gz;
    }

    stream.crc = NULL;
    if (!CXReadFooter(&stream, &meta, checksum)) {
        goto read_fail_gz;
    }

    if (meta.nodeCount > (uint64_t)CXSizeMAX || meta.edgeCount > (uint64_t)CXSizeMAX) {
        errno = ERANGE;
        goto read_fail_gz;
    }

    CXSize nodeCount = (CXSize)meta.nodeCount;
    CXSize edgeCount = (CXSize)meta.edgeCount;
    if (CXCountActive(network->nodeActive, network->nodeCapacity) != nodeCount) {
        errno = EINVAL;
        goto read_fail_gz;
    }
    if (CXCountActive(network->edgeActive, network->edgeCapacity) != edgeCount) {
        errno = EINVAL;
        goto read_fail_gz;
    }

    network->nodeCount = nodeCount;
    network->edgeCount = edgeCount;

    if (!CXRebuildIndexManager(network->nodeIndexManager, network->nodeActive, network->nodeCapacity)) {
        goto read_fail_gz;
    }
    if (!CXRebuildIndexManager(network->edgeIndexManager, network->edgeActive, network->edgeCapacity)) {
        goto read_fail_gz;
    }
    if (!CXRebuildAdjacency(network)) {
        goto read_fail_gz;
    }

    CXAttributeLoadListDestroy(&nodeAttributes);
    CXAttributeLoadListDestroy(&edgeAttributes);
    CXAttributeLoadListDestroy(&networkAttributes);

    if (bgzf_close(bgzf) != 0) {
        CXFreeNetwork(network);
        return NULL;
    }

    return network;

read_fail_gz:
    CXAttributeLoadListDestroy(&nodeAttributes);
    CXAttributeLoadListDestroy(&edgeAttributes);
    CXAttributeLoadListDestroy(&networkAttributes);
    CXFreeNetwork(network);
    bgzf_close(bgzf);
    return NULL;
}
