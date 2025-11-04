#ifndef CXNetwork_CXNetworkXNet_h
#define CXNetwork_CXNetworkXNet_h

#include "CXCommons.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CXNetwork;

/**
 * Reads a graph from an `.xnet` (XNET 1.0.0 or legacy) container.
 *
 * @param path Path to the XNET file on disk.
 * @return Newly allocated network when successful, otherwise NULL.
 */
CX_EXTERN struct CXNetwork* CXNetworkReadXNet(const char *path);

/**
 * Serializes a network using the XNET 1.0.0 human-readable container.
 *
 * Performs compaction to ensure contiguous node and edge indices before
 * writing. Adds the `_original_ids_` vertex attribute to preserve the
 * original node identifiers.
 *
 * @param network Network to serialize.
 * @param path Output path for the `.xnet` file.
 * @return CXTrue on success, CXFalse on failure.
 */
CX_EXTERN CXBool CXNetworkWriteXNet(struct CXNetwork *network, const char *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetworkXNet_h */
