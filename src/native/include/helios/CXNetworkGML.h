#ifndef CXNetwork_CXNetworkGML_h
#define CXNetwork_CXNetworkGML_h

#include "CXCommons.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CXNetwork;

/**
 * Reads a graph from a GML file.
 *
 * Accepts standard GML and a looser dialect that tolerates quoted keys and
 * unquoted scalar strings.
 *
 * @param path Path to the `.gml` file on disk.
 * @return Newly allocated network when successful, otherwise NULL.
 */
CX_EXTERN struct CXNetwork* CXNetworkReadGML(const char *path);

/**
 * Serializes a network as GML.
 *
 * Lossy cases (for example unsupported attribute payloads or renamed keys) are
 * reported via `CXNetworkSerializationLastWarningMessage()`.
 *
 * @param network Network to serialize.
 * @param path Output path for the `.gml` file.
 * @return CXTrue on success, CXFalse on failure.
 */
CX_EXTERN CXBool CXNetworkWriteGML(struct CXNetwork *network, const char *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetworkGML_h */
