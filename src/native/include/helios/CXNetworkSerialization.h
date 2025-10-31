#ifndef CXNetwork_CXNetworkSerialization_h
#define CXNetwork_CXNetworkSerialization_h

#include "CXCommons.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CXNetwork;

/** Size in bytes of the fixed-length header written to .bxnet/.zxnet files. */
#define CX_NETWORK_FILE_HEADER_SIZE 64
/** Maximum number of chunk locators stored in the fixed-size footer. */
#define CX_NETWORK_FOOTER_MAX_LOCATORS 16
/** Size in bytes of the fixed-length footer appended to .bxnet/.zxnet files. */
#define CX_NETWORK_FILE_FOOTER_SIZE 512

/** FourCC helpers for chunk identifiers. */
#define CX_NETWORK_FOURCC(a, b, c, d) ((uint32_t)((uint8_t)(a) | ((uint8_t)(b) << 8) | ((uint8_t)(c) << 16) | ((uint8_t)(d) << 24)))

/** Chunk identifiers used by the serialization format. */
#define CX_NETWORK_CHUNK_META CX_NETWORK_FOURCC('M', 'E', 'T', 'A')
#define CX_NETWORK_CHUNK_NODE CX_NETWORK_FOURCC('N', 'O', 'D', 'E')
#define CX_NETWORK_CHUNK_EDGE CX_NETWORK_FOURCC('E', 'D', 'G', 'E')
#define CX_NETWORK_CHUNK_NODE_ATTR CX_NETWORK_FOURCC('N', 'A', 'T', 'T')
#define CX_NETWORK_CHUNK_EDGE_ATTR CX_NETWORK_FOURCC('E', 'A', 'T', 'T')
#define CX_NETWORK_CHUNK_NET_ATTR CX_NETWORK_FOURCC('G', 'A', 'T', 'T')
#define CX_NETWORK_CHUNK_NODE_VALUES CX_NETWORK_FOURCC('N', 'V', 'A', 'L')
#define CX_NETWORK_CHUNK_EDGE_VALUES CX_NETWORK_FOURCC('E', 'V', 'A', 'L')
#define CX_NETWORK_CHUNK_NET_VALUES CX_NETWORK_FOURCC('G', 'V', 'A', 'L')

typedef enum {
	CXNetworkStorageCodecBinary = 0,
	CXNetworkStorageCodecBGZF = 1
} CXNetworkStorageCodec;

typedef struct {
	uint32_t chunkId;
	uint32_t reserved;
	uint64_t offset;
	uint64_t length;
} CXNetworkChunkLocator;

typedef struct {
	uint8_t magic[8];
	uint16_t versionMajor;
	uint16_t versionMinor;
	uint32_t versionPatch;
	uint32_t codec;
	uint32_t flags;
	uint32_t reserved0;
	uint32_t reserved1;
	uint64_t nodeCount;
	uint64_t edgeCount;
	uint64_t nodeCapacity;
	uint64_t edgeCapacity;
} CXNetworkFileHeader;

typedef struct {
	uint8_t magic[8];
	uint32_t chunkCount;
	uint32_t reserved0;
	CXNetworkChunkLocator chunkDirectory[CX_NETWORK_FOOTER_MAX_LOCATORS];
	uint64_t nodeCount;
	uint64_t edgeCount;
	uint64_t nodeAttributeCount;
	uint64_t edgeAttributeCount;
	uint64_t networkAttributeCount;
	uint32_t checksum;
	uint32_t reserved1;
	uint8_t reservedTail[64];
} CXNetworkFileFooter;

CX_EXTERN CXBool CXNetworkWriteBXNet(struct CXNetwork *network, const char *path);
CX_EXTERN CXBool CXNetworkWriteZXNet(struct CXNetwork *network, const char *path, int compressionLevel);
CX_EXTERN struct CXNetwork* CXNetworkReadBXNet(const char *path);
CX_EXTERN struct CXNetwork* CXNetworkReadZXNet(const char *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetworkSerialization_h */
