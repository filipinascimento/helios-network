#ifndef CXNetwork_CXNetworkGT_h
#define CXNetwork_CXNetworkGT_h

#include "CXCommons.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CXNetwork;

/**
 * Reads a graph-tool `.gt` binary graph file.
 *
 * Supports the v1 graph-tool wire format, including endian-aware topology
 * loading and the scalar/vector property-map types that map cleanly to Helios
 * attributes. Unsupported or lossy property maps are skipped and reported via
 * `CXNetworkSerializationLastWarningMessage()`.
 *
 * @param path Path to the `.gt` file on disk.
 * @return Newly allocated network when successful, otherwise NULL.
 */
CX_EXTERN struct CXNetwork* CXNetworkReadGT(const char *path);

/**
 * Serializes a network as a graph-tool `.gt` binary graph file.
 *
 * `.gt` is an interoperability format. Helios-specific state and unsupported
 * attributes may be skipped or converted, with warnings reported via
 * `CXNetworkSerializationLastWarningMessage()`.
 *
 * @param network Network to serialize.
 * @param path Output path for the `.gt` file.
 * @return CXTrue on success, CXFalse on failure.
 */
CX_EXTERN CXBool CXNetworkWriteGT(struct CXNetwork *network, const char *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetworkGT_h */
