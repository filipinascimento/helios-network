# Vendored zstd

This directory contains the decompression subset of facebook/zstd v1.5.7.

Source: https://github.com/facebook/zstd/releases/tag/v1.5.7
License: BSD-3-Clause OR GPL-2.0-only, as provided by upstream in LICENSE and COPYING.

Only the public headers, common sources, and decompression sources needed to read `.zst` frames are vendored here. Helios uses this code to auto-decompress `.gt.zst` graph-tool files before parsing the `.gt` payload.
