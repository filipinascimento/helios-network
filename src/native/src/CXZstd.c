#include "CXZstd.h"

#include "zstd.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char CXZstdMagic[] = { 0x28, 0xb5, 0x2f, 0xfd };

struct CXZstdInputStream {
	FILE *file;
	CXBool compressed;
	CXBool eof;
	ZSTD_DStream *decoder;
	unsigned char *input;
	size_t inputSize;
	size_t inputPos;
	size_t inputLimit;
	unsigned char *output;
	size_t outputSize;
	size_t outputPos;
	size_t outputLimit;
};

static CXBool CXZstdFileStartsWithMagic(FILE *file) {
	if (!file) {
		return CXFalse;
	}
	unsigned char magic[sizeof(CXZstdMagic)] = { 0 };
	size_t read = fread(magic, 1, sizeof(magic), file);
	rewind(file);
	return read == sizeof(magic) && memcmp(magic, CXZstdMagic, sizeof(CXZstdMagic)) == 0;
}

CXZstdInputStream* CXZstdInputStreamOpen(const char *path) {
	if (!path) {
		return NULL;
	}
	FILE *file = fopen(path, "rb");
	if (!file) {
		return NULL;
	}
	CXZstdInputStream *stream = calloc(1, sizeof(CXZstdInputStream));
	if (!stream) {
		fclose(file);
		return NULL;
	}
	stream->file = file;
	stream->compressed = CXZstdFileStartsWithMagic(file);
	if (!stream->compressed) {
		return stream;
	}
	stream->decoder = ZSTD_createDStream();
	stream->inputSize = ZSTD_DStreamInSize();
	stream->outputSize = ZSTD_DStreamOutSize();
	stream->input = stream->inputSize > 0 ? malloc(stream->inputSize) : NULL;
	stream->output = stream->outputSize > 0 ? malloc(stream->outputSize) : NULL;
	if (!stream->decoder || !stream->input || !stream->output) {
		CXZstdInputStreamClose(stream);
		return NULL;
	}
	size_t init = ZSTD_initDStream(stream->decoder);
	if (ZSTD_isError(init)) {
		CXZstdInputStreamClose(stream);
		return NULL;
	}
	return stream;
}

static CXBool CXZstdInputStreamReadCompressed(CXZstdInputStream *stream, void *dst, size_t size) {
	if (!stream || !dst) {
		return CXFalse;
	}
	unsigned char *target = (unsigned char *)dst;
	size_t written = 0;
	while (written < size) {
		if (stream->outputPos < stream->outputLimit) {
			size_t available = stream->outputLimit - stream->outputPos;
			size_t wanted = size - written;
			size_t chunk = available < wanted ? available : wanted;
			memcpy(target + written, stream->output + stream->outputPos, chunk);
			stream->outputPos += chunk;
			written += chunk;
			continue;
		}

		stream->outputPos = 0;
		stream->outputLimit = 0;
		if (stream->inputPos >= stream->inputLimit && !stream->eof) {
			stream->inputLimit = fread(stream->input, 1, stream->inputSize, stream->file);
			stream->inputPos = 0;
			if (stream->inputLimit < stream->inputSize) {
				if (ferror(stream->file)) {
					return CXFalse;
				}
				stream->eof = CXTrue;
			}
		}
		if (stream->inputPos >= stream->inputLimit && stream->eof) {
			return CXFalse;
		}

		ZSTD_inBuffer input = {
			.src = stream->input,
			.size = stream->inputLimit,
			.pos = stream->inputPos
		};
		ZSTD_outBuffer output = {
			.dst = stream->output,
			.size = stream->outputSize,
			.pos = 0
		};
		size_t result = ZSTD_decompressStream(stream->decoder, &output, &input);
		if (ZSTD_isError(result)) {
			return CXFalse;
		}
		stream->inputPos = input.pos;
		stream->outputLimit = output.pos;
		if (stream->outputLimit == 0 && stream->eof && result != 0) {
			return CXFalse;
		}
	}
	return CXTrue;
}

CXBool CXZstdInputStreamRead(CXZstdInputStream *stream, void *dst, size_t size) {
	if (size == 0) {
		return CXTrue;
	}
	if (!stream || !stream->file || !dst) {
		return CXFalse;
	}
	if (!stream->compressed) {
		return fread(dst, 1, size, stream->file) == size;
	}
	return CXZstdInputStreamReadCompressed(stream, dst, size);
}

CXBool CXZstdInputStreamSkip(CXZstdInputStream *stream, uint64_t size) {
	if (!stream || !stream->file) {
		return CXFalse;
	}
	if (size == 0) {
		return CXTrue;
	}
	if (!stream->compressed) {
		while (size > 0) {
			long chunk = size > (uint64_t)LONG_MAX ? LONG_MAX : (long)size;
			if (fseek(stream->file, chunk, SEEK_CUR) != 0) {
				return CXFalse;
			}
			size -= (uint64_t)chunk;
		}
		return CXTrue;
	}
	unsigned char buffer[8192];
	while (size > 0) {
		size_t chunk = size > sizeof(buffer) ? sizeof(buffer) : (size_t)size;
		if (!CXZstdInputStreamRead(stream, buffer, chunk)) {
			return CXFalse;
		}
		size -= (uint64_t)chunk;
	}
	return CXTrue;
}

CXBool CXZstdInputStreamIsCompressed(const CXZstdInputStream *stream) {
	return stream && stream->compressed;
}

void CXZstdInputStreamClose(CXZstdInputStream *stream) {
	if (!stream) {
		return;
	}
	if (stream->decoder) {
		ZSTD_freeDStream(stream->decoder);
	}
	free(stream->input);
	free(stream->output);
	if (stream->file) {
		fclose(stream->file);
	}
	free(stream);
}
