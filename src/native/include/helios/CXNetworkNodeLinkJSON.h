#ifndef CXNetwork_CXNetworkNodeLinkJSON_h
#define CXNetwork_CXNetworkNodeLinkJSON_h

#include "CXCommons.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CXNetwork;

/**
 * Serializes a network as node-link JSON compatible with common D3/NetworkX
 * style payloads.
 *
 * Lossy cases are reported via `CXNetworkSerializationLastWarningMessage()`.
 *
 * @param network Network to serialize.
 * @param path Output path for the `.json` file.
 * @return CXTrue on success, CXFalse on failure.
 */
CX_EXTERN CXBool CXNetworkWriteNodeLinkJSON(struct CXNetwork *network, const char *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetworkNodeLinkJSON_h */
