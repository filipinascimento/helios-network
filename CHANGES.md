# CHANGES

## 2026-02-21

- Added multiscale dimension measurements to the native core with FW/BK/CE/LS estimators (`order` support), including local node curves and global aggregate curves.
- Added `CXNetworkMeasureNodeDimension` and `CXNetworkMeasureDimension` to the C API and WASM exports.
- Added JS APIs `measureNodeDimension()` / `measureDimension()` plus `DimensionDifferenceMethod`.
- Added JS `createDimensionSession()` for incremental, progress-aware dimension measurement with optional node outputs for max dimension and full per-level dimension curves (vector or JSON string encoding).
- Added Python bindings `measure_node_dimension()` / `measure_dimension()` and `DimensionMethod`.
- Added toroidal regular-network regression tests covering expected capacities and dimensional behavior.
- Added Node and Python toroidal dimension examples for 1D/2D/3D/4D regular networks, including a steppable JS session example and a Python plotting script that outputs local-dimension curves plus max-dimension tables.

## 2026-02-01

- Added Python bindings (meson-python) with a `Network` class, attribute access, serialization helpers, and networkx/igraph conversion utilities.
- Added Python node/edge selectors with iterator access (`network.nodes`, `network.edges`), plus edge-pair iteration helpers.
- Added network-scope attribute access (`network["attr"]`, `network.attributes["attr"]`) and vector attribute batch assignment helpers.
- Added Python helpers for categorical attributes (categorize/decategorize and category dictionary get/set).
- Added attribute auto-definition on assignment and collection-level `define_attribute` helpers.
- Added Python tests and examples covering basic graph edits, attributes, and XNET round-trips.

## 2026-01-28

- Added native + JS accessors for categorical attribute dictionaries (`get*AttributeCategoryDictionary` / `set*AttributeCategoryDictionary`).
- Added tests covering category dictionary retrieval after categorization.
- Fixed missing-category dictionary encoding so `-1` entries no longer decode as null.

## 2026-01-26

- Leiden community outputs are categorical by default (`categoricalCommunities=true`), with an opt-out to keep integer storage.
- Added `getPackageVersion()` on `HeliosNetwork` (static + instance) to report the helios-network package version.

## 2026-01-25

- Added multi-category attributes (weighted or unweighted) backed by CSR-style buffers.
- Added XNET `m`/`mw` tokens and legacy `__multicategory*` prefix support, plus BXNET/ZXNET serialization for multi-category values.
- Added JS and native tests covering multi-category buffers, weights, and serialization round-trips.

## 2026-01-22

- Fixed node→edge passthrough updates to refresh dense edge buffers when the source node attribute version changes (version-based refresh, not just dirty flags).

## 2026-01-19

- Added allow/ignore attribute filters for BXNet/ZXNet/XNet serialization so callers can include or exclude specific node/edge/network attributes per save.

## 2026-01-14

- Added an EventTarget-based events system to `HeliosNetwork` (helpers: `on`, `off`, `emit`, `onAny`, `listen`) for binding external consumers to topology and attribute changes.
- New events emitted for topology mutations (`nodes:added`, `nodes:removed`, `edges:added`, `edges:removed`, `topology:changed`) and attribute lifecycle/versioning (`attribute:defined`, `attribute:removed`, `attribute:changed`).
- Added docs in `docs/events.md` and regression tests in `tests/events_system.test.js`.

## 2026-01-12

- Added a generic steppable-session runner (`run`) for long-running WASM computations, allowing cooperative yielding between chunks (no worker required).
- Added worker execution for supported steppable sessions (`runWorker`), which snapshots only the required topology/attributes and runs the native session in a separate WASM instance.
- Steppable sessions now auto-cancel if the underlying network becomes out-of-sync (tracked topology/attribute versions change); each session type defines its own cancellation policy internally (e.g. Leiden cancels on topology changes and weight attribute version bumps when weighted).
- Leiden session progress now reports `progressCurrent`/`progressTotal` (instead of a non-monotonic `progress01`), and exposes convenience helpers like `isComplete()` and `isFinalized()`.
- Updated Leiden session tests to cover `run()` and version-based cancellation.

## 2026-01-24

- Replaced array sorting with introsort (median-of-three, 3-way partition, heapsort fallback, insertion cutoff) across integer/float/double/unsigned array helpers, and standardized NaN ordering for float/double sorts.
- Added native and WASM test coverage for array sorting helpers and index-paired variants.
- Added a natural-order string comparator for categorical-style sorting (numeric-aware string ordering) with native tests.

## 2026-01-24

- Added categorical attribute dictionaries to XNET and BXNET/ZXNET serialization, including dictionary stanzas in XNET and dictionary blocks in the binary formats.
- Categorical codes are now signed 32-bit integers; `-1` is treated as the missing-value sentinel.
- Legacy XNET loader now recognizes `__category` string attributes and converts them into categorical attributes sorted by frequency; the `__NA__` label maps to id `-1`.

## 2026-01-02

- Added `getBufferMemoryUsage()` to report WASM buffer memory usage (topology buffers including node/edge free lists, sparse attributes, dense buffers) plus total WASM heap size.

## 2025-12-23

- Integer attribute storage changed: `AttributeType.Integer`/`AttributeType.UnsignedInteger` now use 32-bit typed arrays (`Int32Array`/`Uint32Array`) instead of BigInt-backed 64-bit arrays.
- Added 64-bit integer attribute types: `AttributeType.BigInteger` and `AttributeType.UnsignedBigInteger` (backed by `BigInt64Array`/`BigUint64Array`).
- Serialization updated:
  - XNET: `i`/`u` now represent 32-bit integers; new `I`/`U` tokens represent 64-bit big integers.
  - BXNet/ZXNet: integer attribute storage widths now reflect 32-bit vs 64-bit types.
- Dense buffers follow the same typing changes (dense views for integer attributes now return 32-bit arrays; big integers return BigInt arrays). Color-encoding remains limited to 32-bit integer attributes.

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
