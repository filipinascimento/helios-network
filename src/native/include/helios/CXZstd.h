#ifndef CXNetwork_CXZstd_h
#define CXNetwork_CXZstd_h

#include "CXCommons.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Streaming reader that transparently handles plain files and zstd frames.
 *
 * The GT reader uses this abstraction so `.gt` and `.gt.zst` paths share the
 * same parser. The stream owns its file handle and decompression state until
 * `CXZstdInputStreamClose` is called.
 */
typedef struct CXZstdInputStream CXZstdInputStream;

/**
 * Opens a file for transparent plain/zstd streaming.
 *
 * @param path Filesystem path to a plain or zstd-compressed payload.
 * @return Stream handle on success, otherwise NULL.
 */
CXZstdInputStream* CXZstdInputStreamOpen(const char *path);

/**
 * Reads exactly `size` bytes into `dst`.
 *
 * @param stream Stream returned by `CXZstdInputStreamOpen`.
 * @param dst Destination buffer.
 * @param size Number of decoded bytes required.
 * @return CXTrue when all requested bytes were read.
 */
CXBool CXZstdInputStreamRead(CXZstdInputStream *stream, void *dst, size_t size);

/**
 * Skips decoded bytes from the stream.
 *
 * @param stream Stream returned by `CXZstdInputStreamOpen`.
 * @param size Number of decoded bytes to discard.
 * @return CXTrue when all requested bytes were skipped.
 */
CXBool CXZstdInputStreamSkip(CXZstdInputStream *stream, uint64_t size);

/**
 * Reports whether the opened stream is zstd-compressed.
 *
 * @param stream Stream returned by `CXZstdInputStreamOpen`.
 * @return CXTrue for zstd input, CXFalse for plain input or NULL streams.
 */
CXBool CXZstdInputStreamIsCompressed(const CXZstdInputStream *stream);

/**
 * Closes a stream and releases its decompression state.
 *
 * @param stream Stream to close; NULL is ignored.
 */
void CXZstdInputStreamClose(CXZstdInputStream *stream);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXZstd_h */
