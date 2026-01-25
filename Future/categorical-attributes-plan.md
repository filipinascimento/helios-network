# Plan: Categorical Attributes (Dictionary + Sorting)

## Goals
- Serialize categorical attributes with dictionaries across all supported formats.
- Keep XNET 1.0.0 human-readable while preserving category dictionaries.
- Preserve legacy XNET behavior by writing categories as strings.
- Add C-core conversion helpers that map string attributes to categorical
  codes with optional sorting policies.

## Constraints and Principles
- WASM memory views can become invalid after allocation; allocate first, view
  second.
- Avoid JS-side duplication of large buffers; prefer WASM-backed buffers.
- Keep attribute namespace stable; avoid ad-hoc reserved attribute names if
  possible.

## Serialization Formats

### XNET 1.0.0 (human-readable)
- Add a categorical type token (proposed: `c` for scalar categorical; `cN` for vectors if needed).
- Extend the grammar to include a scoped dictionary stanza per categorical attribute.
  Use a count of dictionary entries and allow sparse labels via explicit IDs:
  - `#vdict "AttributeName" count`
  - `#edict "AttributeName" count`
  - `#gdict "AttributeName" count`
- Proposed human-readable form:
  - Attribute declaration uses the categorical token, e.g. `#v "Category" c`.
  - Follow with a dictionary stanza matching the scope (v/e/g) and name.
  - Each dictionary line contains an explicit category ID and string literal:
    - `0 "Category0"`
    - `30 "Category30"`
    - ...
  - Missing IDs imply unlabeled categories; use a fallback like `id-<number>` (which could be customized somehow).
- Values remain categorical integers in the value block (dense numeric codes).
- On load:
  - Read the dictionary stanza if present (v/e/g-specific).
  - Populate the categorical dictionary in C-core.
- On save:
  - Emit dictionary stanza when the dictionary is present.
  - Optional: emit even when empty if requested by an option.

### Legacy XNET
- Always write category values as strings (current legacy behavior).
- On read:
  - Convert strings to categorical dictionary + codes if the attribute is
    declared categorical, or treat as string attribute if the declaration
    is string.

### BXNET / ZXNET
- Implement the reserved dictionary block in `docs/serialization-format.md`.
- Encoding:
  - Store dictionary count and UTF-8 strings in a compact binary block.
  - Keep the attribute values as categorical integers (uint32).
- Reader:
  - Load dictionary block into `categoricalDictionary`.
  - Validate that codes reference dictionary size if strict mode is enabled.

## C-Core API Additions

### Attribute Dictionary Access
- `CXNetworkGetAttributeCategoryDictionary(scope, name)`
  - Returns dictionary (string array + count) or a dictionary handle.
- `CXNetworkSetAttributeCategoryDictionary(scope, name, strings, count)`
  - Sets dictionary and remaps categorical codes if requested.

### String <-> Category Conversion
- `CXNetworkCategorizeAttribute(scope, name, options)`
  - Input: string attribute (scalar string per element).
  - Output: categorical attribute (uint32 codes) + dictionary.
- `CXNetworkDecategorizeAttribute(scope, name, options)`
  - Input: categorical attribute + dictionary.
  - Output: string attribute (optionally allocate string payloads).

### Sorting Options
- Add enum `CXCategorySortOrder`:
  - `CX_CATEGORY_SORT_NONE`
  - `CX_CATEGORY_SORT_FREQUENCY`
  - `CX_CATEGORY_SORT_ALPHABETICAL`
  - `CX_CATEGORY_SORT_NATURAL`
- Behavior:
  - NONE: insertion order of first appearance.
  - FREQUENCY: sort by descending count; tie-break alphabetical for stability.
  - ALPHABETICAL: locale-neutral bytewise UTF-8 ordering.
  - NATURAL: numeric-aware ordering (e.g., `file2` < `file10`), locale-neutral.

## Algorithms and Data Structures

### Categorization (String -> Codes)
1. First pass: scan strings, collect unique categories and frequencies.
   - Use a string dictionary map (hash table) from value -> index.
   - Track frequency counts for each dictionary entry.
2. If sorting is requested:
   - Build an array of dictionary entries with frequency and string pointer.
   - Sort by selected policy.
   - Build remap table old_index -> new_index.
3. Second pass: write categorical codes into the destination buffer using
   the remap table if sorting was applied.

### Sorting Implementation
- Prefer a single allocation per category array plus pointer list.
- Use the CX array introsort helpers (median-of-three + 3-way partition + heapsort fallback) implemented in the C core if possible.
  - For string ordering, use a comparator that mirrors the C-core string compare rules.
  - For frequency sorting, compare `count` descending, then compare strings for deterministic results.
  - For natural ordering, compare digit runs as numbers (skip leading zeros), fall back to bytewise compare for non-digits.
  - The C-core already provides `CXStringCompareNatural` in `src/native/include/helios/CXCommons.h`.

### Memory Considerations
- Avoid allocating per-element copies of strings.
- Use existing string storage if possible; otherwise allocate once per unique
  category.
- Keep dictionary storage owned by the attribute to avoid JS copies.

## Integration Points
- Extend native parsing/writing for XNET and BXNET/ZXNET.
- Update JS wrapper to expose new categorization helpers, but keep C-core as
  the source of truth.
- Update docs:
  - `docs/xnet-format.md`
  - `docs/serialization-format.md`
  - `docs/saving-and-loading.md`

## Testing Plan
- Native tests:
  - Round-trip XNET 1.0.0 categorical dictionary and codes.
  - Legacy XNET save/load with strings for category attributes.
  - BXNET/ZXNET round-trip with dictionary.
  - Sorting modes: NONE, FREQUENCY, ALPHABETICAL.
- JS tests:
  - `HeliosNetwork` helpers call into C-core and produce expected mappings.

## Open Questions
- Should XNET 1.0.0 dictionary be optional on save? Default behavior?
- Should decategorize allocate full strings or provide view-only access?
- How strict should code validation be when dictionary is missing?
