# CHANGES

## 2026-03-06

- Removed the legacy packed-buffer API surface and the related documentation/tests.
- `nodeIndices` / `edgeIndices` now expose persistent WASM-backed active-index views instead of materializing cached JS copies.
- Simplified visualization guidance around sparse attribute buffers, active-index views, and `withBufferAccess(...)`.

## 2026-03-03

- Added connected-components support across native C, JS/WASM, and Python:
  - weakly-connected mode
  - strongly-connected mode for directed graphs (undirected graphs normalize to weak mode)
- Added native one-shot measurement API:
  - `CXNetworkMeasureConnectedComponents(...)` (per-node component ids + largest component size).
- Added native steppable session API for progress-aware execution:
  - `CXConnectedComponentsSessionCreate/Step/GetProgress/Finalize/Destroy`.
- Added JS APIs:
  - `measureConnectedComponents(...)`
  - `createConnectedComponentsSession(...)` with progress, cancellation-on-topology-change, and finalize support (optional attribute write).
  - `extractConnectedComponents(...)`
  - `extractLargestConnectedComponent(...)`
  - `ConnectedComponentsMode` export.
- Added Python binding:
  - `measure_connected_components(mode=...)`.
  - `extract_connected_components(...)` wrapper helper.
  - `extract_largest_connected_component(...)` wrapper helper.
- Added coreness (k-core index) support across native C, JS/WASM, and Python:
  - Native: `CXNetworkMeasureCoreness(...)` + steppable `CXCorenessSessionCreate/Step/GetProgress/Finalize/Destroy`.
  - JS: `measureCoreness(...)` and `createCorenessSession(...)` with topology-change cancellation and optional unsigned-integer node-attribute output.
  - Python: `measure_coreness(direction=..., execution_mode=...)`.
- Exported the new native symbols in the WASM export list.
- Added regression coverage in:
  - `tests/native/test_measurements.c`
  - `tests/network_measurements.test.js`
  - `python/tests/test_network.py`

## 2026-02-27

- Added native filtered-subgraph helpers for selectors:
  - `CXNetworkBuildFilteredSubgraph` to build induced node/edge outputs from optional node/edge filters.
  - New selector utilities: clear, active-filter, and in-place intersection for node/edge selectors.
- Added JS API `filterSubgraph(options)` on `HeliosNetwork`:
  - Supports node/edge query filters (`nodeQuery`, `edgeQuery`) and/or selector/indices inputs (`nodeSelector`/`nodeSelection`, `edgeSelector`/`edgeSelection`).
  - Always enforces induced-edge semantics (edges are removed when either endpoint is filtered out).
  - Supports optional ordering for nodes/edges (`orderNodesBy`, `orderEdgesBy`) by id or numeric attribute component.
  - Supports selector output mode (`asSelector: true`) for reuse in visualization pipelines.
- Exported the new native functions in the WASM export list.
- Added JS regression tests covering filtered subgraph behavior, induced-edge enforcement, selector intersections, and ordering.

## 2026-02-21

- Added neighbor traversal APIs across native C, JS, and Python:
  - One-hop neighbor collection for one or multiple source nodes.
  - Concentric traversal helpers for exact level (`at level`) and cumulative level (`up to level`) queries.
  - Python wrapper convenience methods for `Network` and `NodeSelector`.
- Added regression tests for the new neighbor/concentric traversal APIs in native, JS, and Python suites.

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

## 2026-02-21

- Added new graph measurements in the C core, JS API, and Python bindings: degree, strength (sum/average/max/min), local clustering coefficient (unweighted, Onnela, Newman), eigenvector centrality, and betweenness centrality (weighted and unweighted).
- Added execution-mode controls so callers can request single-thread or parallel execution per measurement; JS wrappers default centrality runs to single-thread mode, while Python/native can use parallel mode.
- Added chunk/decomposition support for heavy centrality metrics:
  - Betweenness can be accumulated over source-node batches.
  - Eigenvector centrality accepts an initial vector for iterative stepping.
- Added regression tests with canonical graphs (triangle, star, path, directed toy graphs, weighted shortest-path toy graphs) across native C, JS, and Python test suites to validate expected metric values.

## 2026-01-25

- Added multi-category attributes (weighted or unweighted) backed by CSR-style buffers.
- Added XNET `m`/`mw` tokens and legacy `__multicategory*` prefix support, plus BXNET/ZXNET serialization for multi-category values.
- Added JS and native tests covering multi-category buffers, weights, and serialization round-trips.

## 2026-01-22

- Fixed node→edge passthrough updates to refresh derived edge buffers when the source node attribute version changes (version-based refresh, not just dirty flags).

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

- Added `getBufferMemoryUsage()` to report WASM buffer memory usage (topology buffers including node/edge free lists and sparse attributes) plus total WASM heap size.

## 2025-12-23

- Integer attribute storage changed: `AttributeType.Integer`/`AttributeType.UnsignedInteger` now use 32-bit typed arrays (`Int32Array`/`Uint32Array`) instead of BigInt-backed 64-bit arrays.
- Added 64-bit integer attribute types: `AttributeType.BigInteger` and `AttributeType.UnsignedBigInteger` (backed by `BigInt64Array`/`BigUint64Array`).
- Serialization updated:
  - XNET: `i`/`u` now represent 32-bit integers; new `I`/`U` tokens represent 64-bit big integers.
  - BXNet/ZXNet: integer attribute storage widths now reflect 32-bit vs 64-bit types.
- Integer and big-integer attribute storage typing was aligned across the buffer APIs; color encoding remains limited to 32-bit integer attributes.

## 2025-02-20

- Added stable version counters for sparse attributes and topology. Versions start at 0, wrap at `Number.MAX_SAFE_INTEGER`, and are exposed on sparse wrappers plus active-index views via `version`.
- Introduced versioning helpers: `getTopologyVersions()`, `get*AttributeVersion(...)`, and manual bumpers on attribute wrappers / `bump*AttributeVersion` APIs for direct buffer writes.
- Documented the new flow in `docs/versioning.md` around versioning-first change detection.

## 2025-02-02 (developer notes)

- Removed public `nodeActivityView` / `edgeActivityView` accessors. Active tracking remains in the C core (bitsets), but JS no longer exposes raw masks.
- Added active index helpers:
  - `nodeIndices` / `edgeIndices`: expose WASM-backed active id views in native order.
  - `hasNodeIndex` / `hasEdgeIndex` and batch `hasNodeIndices` / `hasEdgeIndices`: check activity via the native bitsets.
- Added full-coverage selectors `network.nodes` / `network.edges` that represent all active ids without storing masks; they materialize ids via the active-index writers.
- Updated docs and tests to reflect selector usage instead of activity views; compact now remaps using active index lists.
- Browser and Node tests updated to cover the new helpers and the selector lifecycle.
