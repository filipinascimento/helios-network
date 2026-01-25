# Helios Network Serialization Format

The Helios native core can persist and restore networks using the binary
`.bxnet` and BGZF-compressed `.zxnet` container formats. Both formats share the
same logical layout; `.zxnet` applies BGZF compression on the byte stream via a
vendored htslib subset while `.bxnet` writes the bytes directly.

## Endianness

All multi-byte integers and floating point values are stored in little-endian
byte order. Readers on big-endian architectures must perform appropriate byte
swaps. The in-memory implementation performs the conversion automatically.

## File Structure

```
Header (64 bytes, fixed)
Chunk Stream (ordered sequence of chunk records)
Footer (512 bytes, fixed)
```

### Header

| Offset | Field                | Size | Description                                      |
|-------:|----------------------|------|--------------------------------------------------|
| 0      | Magic (`ZXNETFMT`)   | 8    | Identifies Helios serialization stream           |
| 8      | Version (major,minor,patch) | 8 | Serialization format version (currently 1.0.0) |
| 16     | Codec                | 4    | `0` = `.bxnet`, `1` = `.zxnet`                   |
| 20     | Flags                | 4    | Bit `0` indicates directed graph                 |
| 24     | Reserved             | 8    | Zero                                             |
| 32     | Node count           | 8    | Active node total                                 |
| 40     | Edge count           | 8    | Active edge total                                 |
| 48     | Node capacity        | 8    | Allocated nodes                                   |
| 56     | Edge capacity        | 8    | Allocated edges                                   |

### Chunk Stream

Each chunk begins with a 16-byte header followed by its payload.

| Field          | Size | Description                        |
|----------------|------|------------------------------------|
| Chunk ID       | 4    | FourCC identifier                  |
| Chunk Flags    | 4    | Reserved (written as zero)         |
| Payload Length | 8    | Size in bytes of the following payload |

The current chunk order is fixed and must be processed sequentially:

| Chunk ID | Purpose                                   |
|----------|-------------------------------------------|
| `META`   | Graph-level metadata                      |
| `NODE`   | Node activity bitmap                      |
| `EDGE`   | Edge activity bitmap and edge endpoints   |
| `NATT`   | Node attribute declarations               |
| `EATT`   | Edge attribute declarations               |
| `GATT`   | Graph (network) attribute declarations    |
| `NVAL`   | Node attribute values                     |
| `EVAL`   | Edge attribute values                     |
| `GVAL`   | Graph attribute values                    |

Each chunk payload is composed of one or more `[size, data]` blocks. The size is
stored as an unsigned 64-bit little-endian integer followed by the raw bytes.

#### META Chunk

Single block of 64 bytes:

| Field                  | Size | Description                   |
|------------------------|------|-------------------------------|
| Directed flag          | 1    | Non-zero when graph directed  |
| Reserved               | 7    | Zero                          |
| Node count             | 8    | Matches header value          |
| Edge count             | 8    | Matches header value          |
| Node capacity          | 8    | Matches header value          |
| Edge capacity          | 8    | Matches header value          |
| Node attribute count   | 8    | Number of node attributes     |
| Edge attribute count   | 8    | Number of edge attributes     |
| Graph attribute count  | 8    | Number of network attributes  |

#### NODE Chunk

One block containing the node activity bitmap. The number of bytes equals the
node capacity recorded in the header. A non-zero byte indicates an active node.

#### EDGE Chunk

Two blocks:

1. Edge activity bitmap (one byte per edge, length equals edge capacity).
2. Edge endpoint table. Each record stores `from` and `to` indices as
   little-endian 64-bit unsigned integers. Records are tightly packed and the
   block length equals `edgeCapacity * 16`.

#### Attribute Declaration Chunks (`NATT`, `EATT`, `GATT`)

Each declaration chunk begins with a block containing the attribute count
(`uint32 count`, `uint32 reserved`). For each attribute:

1. Name block containing UTF-8 bytes (no null terminator).
2. Descriptor block (24 bytes):
   - `uint8  type` (`CXAttributeType`)
   - `uint8  reserved`
   - `uint16 flags` (currently zero; non-zero indicates unsupported features)
   - `uint32 dimension`
   - `uint32 storage width` (number of bytes per element on disk)
   - `uint32 reserved`
   - `uint64 capacity`
3. Dictionary block (categorical dictionaries). The block is empty unless the
   attribute type is categorical and a dictionary is present. The payload is:
   - `uint32 entry_count`
   - For each entry:
     - `int32 id` (signed category id)
     - `uint32 byte_length`
     - UTF-8 bytes for the label

The current implementation supports scalar numeric attribute types:
boolean, float, double, 32-bit signed integer, 32-bit unsigned integer,
64-bit signed/unsigned big integers, and categorical integers (signed 32-bit).
Attribute
types that rely on pointer payloads (strings, raw data, Javascript-backed
attributes) are not yet serialized. Declarations containing unsupported
flags are rejected by the reader.

#### Attribute Value Chunks (`NVAL`, `EVAL`, `GVAL`)

Start with a block containing the attribute count. Each attribute contributes:

1. Name block (matching the declaration block).
2. Value block containing `capacity * dimension` elements. Elements are stored
   using the `storage width` recorded in the descriptor and appear in the same
   order as the declaration chunk. Numeric values are little-endian; boolean
   values are raw bytes.

### Footer

The footer is a fixed-size (512 byte) block appended after the chunk stream. It
contains a chunk directory and integrity metadata and is not included in the CRC
calculation.

| Offset | Field                        | Size | Description                          |
|-------:|------------------------------|------|--------------------------------------|
| 0      | Magic (`ZXFOOTER`)           | 8    | Footer identifier                     |
| 8      | Chunk count                  | 4    | Number of directory entries used      |
| 12     | Reserved                     | 4    | Zero                                  |
| 16     | Chunk directory              | 384  | Up to 16 entries                      |
| 400    | Node count                   | 8    | Mirrors META chunk                    |
| 408    | Edge count                   | 8    | Mirrors META chunk                    |
| 416    | Node attribute count         | 8    | Mirrors META chunk                    |
| 424    | Edge attribute count         | 8    | Mirrors META chunk                    |
| 432    | Graph attribute count        | 8    | Mirrors META chunk                    |
| 440    | CRC32 checksum               | 4    | Header + chunk stream checksum        |
| 444    | Reserved                     | 4    | Zero                                  |
| 448    | Reserved tail                | 64   | Zero                                  |

Each chunk directory entry encodes:

| Field    | Size | Description                                         |
|----------|------|-----------------------------------------------------|
| Chunk ID | 4    | FourCC identifier                                   |
| Flags    | 4    | Flags value recorded in the chunk header            |
| Offset   | 8    | Stream offset (from start of file) of the chunk header |
| Length   | 8    | Payload size associated with the chunk              |

Offsets are measured in the native stream's coordinate system (byte offsets for
`.bxnet`, BGZF virtual offsets for `.zxnet`) and allow readers to seek directly
to individual chunks when random access is desirable.

The CRC32 field reflects the result of running `crc32(0, …)` over the header and
chunk stream bytes (excluding the footer itself). Readers recompute the checksum
and compare against the stored value.

## Limitations

- Serialization currently supports numeric and boolean attributes. Pointer-based
  attribute types (strings, raw data blobs, Javascript-backed payloads) are not
  yet persisted.
- The chunk order is fixed; readers expect the sequence listed above.
- `.zxnet` uses BGZF compression but retains the same logical layout. Offsets
  recorded in the footer are BGZF virtual offsets, suitable for `bgzf_seek`.

Extensions to the format must increment the serialization version and remain
backwards compatible by adding new chunks or fields in a forward-compatible
fashion.
