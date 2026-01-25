# Plan: Multi-Category Attributes (Weighted Sets)

## Goals
- Support attributes where each element can have multiple categories.
- Optionally associate weights per category (e.g. categoryA:0.5).
- Keep serialization compact and interoperable.
- Preserve WASM-first performance and avoid JS duplication.

## Data Model
- A multi-category attribute is a sparse set of category IDs per element.
- Optional weights are float values aligned to each category ID.
- Storage is CSR-like (offsets + flat IDs [+ optional weights]); avoid per-entry Sets/Hashes.
- Consider per-entry ID sorting (optional) to allow binary-search membership without hashes.
- Proposed representation uses two buffers:
  - Offsets (length = element_count + 1) into a flat category index buffer.
  - Category IDs (flat uint32 array).
  - Optional weights (flat float array) parallel to category IDs.

## C-Core Structures
- New attribute type: `CXDataAttributeMultiCategoryType` (or similar).
- Attribute payload holds:
  - `offsets` buffer (uint32 or uint64, depending on size constraints).
  - `categoryIds` buffer (uint32).
  - `weights` buffer (float32), optional.
- Provide a categorical dictionary identical to `Category` type.

## API Additions
- `CXNetworkDefineMultiCategoryAttribute(scope, name, hasWeights)`
- `CXNetworkGetMultiCategoryBuffers(scope, name, &offsets, &ids, &weights)`
- `CXNetworkSetMultiCategoryBuffers(scope, name, offsets, ids, weights)` (bulk/fast path)
- `CXNetworkSetMultiCategoryEntry(scope, name, index, ids, count, weights)`
- `CXNetworkClearMultiCategoryEntry(scope, name, index)`
- JS convenience helpers (no JS-side copies of large data):
  - `setMultiCategoryEntryByName(scope, name, index, categories[, weights])`
  - `setMultiCategoryForNodes(scope, name, nodeIndices, categories[, weight])`
  - `getMultiCategoryEntryRange(scope, name, index)` -> `{start,end}` to slice views

## Serialization

### XNET 1.0.0 (human-readable)
- New attribute declaration token, e.g. `mc` or `mw` (weighted):
  - `#v "Tags" mc`
  - `#v "Topics" mw`
- Value lines per element:
  - Unweighted: `"catA" "catB" "catC"`
  - Weighted: `"catA":0.5 "catB":0.45 "catC":0.05`
- Dictionary stanza (same approach as categorical attributes).

### BXNET / ZXNET
- Add dictionary block for multi-category attributes.
- Add a new value encoding block:
  - Offsets array + category IDs (+ optional weights).
- Keep block lengths explicit in chunk headers for streaming.

## Conversion Helpers
- `CXNetworkCategorizeMultiAttribute(scope, name, options)`
  - Input: string list per element (with optional weights).
  - Output: multi-category buffers + dictionary.
- Sorting policies:
  - Sort dictionary as in categorical plan (NONE/FREQUENCY/ALPHABETICAL).
  - Within each element, optionally sort category IDs for stable iteration.

## Algorithms and Performance
- Build dictionary using hash table (string -> ID).
- Build offsets by prefix-summing per-element counts.
- Fill category ID and weight buffers in a second pass.
- Avoid per-entry JS `Set`/hash structures after build; keep flat buffers.
- For weights, validate that sums are optional and do not require
  normalization unless requested.

## Memory Considerations
- Store offsets and IDs in WASM memory only.
- Do not materialize JS arrays of IDs/weights for large graphs.
- Expose views via `withBufferAccess` patterns.

## Tests
- Native tests:
  - Round-trip XNET 1.0.0 for unweighted and weighted attributes.
  - BXNET/ZXNET round-trip with dictionary.
  - Conversion from strings to multi-category buffers.
- JS tests:
  - Wrapper functions and view access consistency.

## Open Questions
- Offsets width: uint32 vs uint64 for very large graphs.
- Should element-level empty sets serialize as empty line or explicit token?
- Should weights be optional per entry (mixed) or all-or-nothing?
- Should we enforce weight sum normalization (1.0) or leave to user?
