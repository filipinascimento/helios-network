# CHANGES

## 2025-02-20

- Added stable version counters for sparse attributes, dense buffers (including color-encoded), and topology. Versions start at 0, wrap at `Number.MAX_SAFE_INTEGER`, and are exposed on all dense descriptors (index views also expose `topologyVersion`) plus sparse wrappers.
- Introduced versioning helpers: `getTopologyVersions()`, `get*AttributeVersion(...)`, and manual bumpers on attribute wrappers / `bump*AttributeVersion` APIs for direct buffer writes.
- Deprecated dirty-flag workflows; `markDense*Dirty` now emits a warning. Dirty booleans remain for compatibility, but consumers should cache and compare versions.
- Documented the new flow in `docs/versioning.md` and refreshed dense buffer docs to highlight versioning-first change detection.

## 2025-02-10

- Added dense color-encoded node/edge buffers (u8x4/u32x4) with `(value + 1)` packing, dirty tracking, and dense-order support for GPU-friendly picking; exposed JS APIs (`define/update/getDenseColorEncoded*Attribute`) and `DenseColorEncodingFormat`.
- Index convenience now uses the literal source name `"$index"` to encode ids without defining an attribute (was `index` previously).
- Documented the new buffers in `docs/dense-buffer-sessions.md` and updated exports.
- Added regression coverage in `tests/node_helios.test.js` for color-encoded buffers.

## 2025-02-02 (developer notes)

- Removed public `nodeActivityView` / `edgeActivityView` accessors. Active tracking remains in the C core (bitsets), but JS no longer exposes raw masks.
- Added active index helpers:
  - `nodeIndices` / `edgeIndices`: return copied `Uint32Array`s of active ids in native order (allocated on call; use outside `withBufferAccess`).
  - `hasNodeIndex` / `hasEdgeIndex` and batch `hasNodeIndices` / `hasEdgeIndices`: check activity via the native bitsets.
- Active index copies are now cached per topology version and regenerated only when structure changes (add/remove/compact).
- Fastest ways to iterate active ids:
  - For general iteration (e.g. to check/modify sparse attributes): use the cached `nodeIndices` / `edgeIndices` snapshots (regenerated on topology changes).
  - For render-aligned order: call `updateDense*IndexBuffer()` once after edits, then iterate `getDense*IndexView().view` (respects `setDense*Order` and only repacks when dirty).
- Added full-coverage selectors `network.nodes` / `network.edges` that represent all active ids without storing masks; they materialize ids via the active-index writers.
- Updated docs (`docs/selectors.md`, `docs/visualization-buffers.md`) and tests to reflect selector usage instead of activity views; compact now remaps using active index lists.
- Browser and Node tests updated to cover the new helpers and the selector lifecycle.
