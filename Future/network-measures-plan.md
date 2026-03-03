# Future Plan: Missing Core Network Measures

Date: 2026-03-03
Owner: TBD
Scope: helios-network-v2 (C core + WASM + JS + Python)

## Goal
Implement the high-impact missing network-science measures with the same design pattern already used in Helios:
- Native first (C core as source of truth).
- Parallel execution where useful.
- Capacity-aware outputs (nodeCapacity or edgeCapacity-sized vectors).
- Steppable sessions for WASM with progress reporting, cancellation, and worker support.
- Python parity for both one-shot measurements and progress-capable execution.

## Environment note
- Python editable installs use `mesonpy`; run:
- `python -m pip install -e python --no-build-isolation`
- If needed first: `python -m pip install meson ninja meson-python`.

## Current status
- Connected components is now implemented across native C, JS/WASM, and Python.
- k-core / coreness is now implemented across native C, JS/WASM, and Python.
- Implemented modes:
- Weak connected components.
- Strong connected components for directed graphs (undirected graphs normalize to weak mode).
- Implemented helpers:
- JS: `extractConnectedComponents(...)` and `extractLargestConnectedComponent(...)`.
- Python wrapper: `extract_connected_components(...)` and `extract_largest_connected_component(...)`.
- k-core implementation details:
- One-shot: `measureCoreness(...)` / `measure_coreness(...)` and native `CXNetworkMeasureCoreness(...)`.
- Steppable WASM/native session: `createCorenessSession(...)` and native `CXCorenessSession*` APIs with progress reporting.
- Direction policy: supports `out|in|both` for directed graphs (normalized for undirected graphs).

## Measures in this plan
- PageRank.
- Closeness centrality and harmonic centrality.
- k-core decomposition (coreness / shell index).
- Connected components (and giant component statistics).
- Shortest-path global stats (eccentricity, radius, diameter, average path length).
- Assortativity (degree and numeric attribute).
- Global clustering / transitivity and explicit triangle counts.
- Edge betweenness centrality.
- Stress centrality.

## Non-negotiables
- WASM memory safety rules stay unchanged: allocate first, view second, no stale cached TypedArray views across allocation-prone calls.
- Avoid large JS-side copies; keep heavy buffers in native/WASM.
- New APIs should mirror existing naming and behavior style from `CXNetworkMeasure*`, `measure*`, and steppable session patterns.

## Standard Implementation Template

### 1) Native one-shot API shape
- Add one `CXNetworkMeasure*` entry point per metric in `CXNetwork.h`.
- Follow current output conventions:
- Node-scoped outputs: `float* out...` sized to `CXNetworkNodeCapacity(network)`.
- Edge-scoped outputs: `float* out...` sized to `CXNetworkEdgeCapacity(network)`.
- Return `CXBool` for success/failure or `CXSize` when processed counts are meaningful.
- Add `CXMeasurementExecutionMode` support for heavy computations.

### 2) Native steppable API shape
- Add a session type per heavy metric:
- `CX<Metric>SessionCreate(...)`
- `CX<Metric>SessionStep(session, budget)`
- `CX<Metric>SessionGetProgress(...)`
- `CX<Metric>SessionFinalize(...)`
- `CX<Metric>SessionDestroy(session)`
- Use phased enums like Leiden (`Invalid`, compute phases, `Done`, `Failed`).
- `budget` units must be meaningful and monotonic (node visits, edge blocks, source nodes, or iterations).

### 3) Progress and cancellation model
- Progress fields must always expose:
- `progressCurrent`, `progressTotal`
- `phase`
- Metric-specific counters (for example `processedSources`, `iterations`, `frontierSize`, `componentsFound`)
- Native cancel behavior can be modeled as session disposal.
- WASM cancellation is enforced by JS session wrappers using version baselines and `cancelOn` policies.

### 4) Python parity
- Add one-shot wrappers in `python/src/helios_network/_core.c` and convenience access in `_wrapper.py`.
- Add progress-capable wrappers for sessions for heavy metrics.
- Default Python execution mode should remain parallel for one-shot heavy metrics unless explicitly overridden.
- Release the GIL during long native calls/step loops where applicable.

### 5) WASM/JS parity
- One-shot JS methods in `src/js/HeliosNetwork.js` follow existing `measure*` structure.
- Add `create<Metric>Session(...)` for heavy metrics using `WasmSteppableSession`.
- Expose `step`, `getProgress`, `run`, `runWorker`, `finalize`, `dispose`, and cancellation semantics.
- Export all new native functions in `scripts/exported-functions.txt`.

### 6) Worker support
- For each steppable metric, add worker session support in `src/js/sessions/worker/HeliosSessionWorkerCore.js`.
- Snapshot only required topology and attributes.
- Apply results back to the main network after finalize.

## Recommended implementation order

### Phase 0: Shared foundations
- Add reusable native helpers for:
- Multi-source BFS/SSSP stepping and queue/work buffers.
- Reduction helpers (edge-wise and node-wise parallel reductions).
- Standard progress struct filling pattern for sessions.
- Add JS worker/session plumbing once, then plug in each metric.

### Phase 1: Topology-first, lower-risk metrics
- Connected components.
- k-core decomposition.
- Global clustering/transitivity plus triangle counts.

### Phase 2: Shortest-path family
- Closeness and harmonic centrality.
- Shortest-path global stats (eccentricity/radius/diameter/avg path length).
- Edge betweenness centrality.
- Stress centrality.

### Phase 3: Spectral and mixing
- PageRank.
- Assortativity.

This order minimizes duplicated work by reusing shortest-path/session infrastructure before advanced centrality variants.

## Metric-by-metric plan

### A) Connected Components (Completed)
- Native one-shot:
- `CXNetworkMeasureConnectedComponents(...)` writes `outNodeComponentId` and summary stats.
- Session:
- Budget unit: visited nodes/edges.
- Progress: `visitedNodes / activeNodes`, `componentCount`.
- Python:
- `measure_connected_components(...)` plus optional session wrapper.
- JS/WASM:
- `measureConnectedComponents(...)` and `createConnectedComponentsSession(...)`.
- Notes:
- Implemented with `weak`/`strong` modes, steppable progress phases for WASM, and component extraction helpers.

### B) k-core / Coreness (Completed)
- Native one-shot:
- `CXNetworkMeasureCoreness(...)` writes node coreness and global max core.
- Session:
- Budget unit: peeled nodes.
- Progress: `peeledNodes / activeNodes`, current `k`.
- Python:
- `measure_coreness(...)`, optional session.
- JS/WASM:
- `measureCoreness(...)`, `createCorenessSession(...)`.
- Notes:
- Implemented with directed degree policy options (`in`, `out`, `both`), one-shot native/JS/Python APIs, and steppable session progress for WASM/native.

### C) Global Clustering / Transitivity / Triangles
- Native one-shot:
- `CXNetworkMeasureTriangles(...)` and/or `CXNetworkMeasureTransitivity(...)`.
- Session:
- Budget unit: processed anchor nodes or wedge intersections.
- Progress: `processedNodes / activeNodes`.
- Python:
- `measure_transitivity(...)`, `measure_triangles(...)`.
- JS/WASM:
- `measureTransitivity(...)`, `measureTriangleCounts(...)`, with session for large graphs.
- Notes:
- Keep local clustering API unchanged; this adds global aggregates and explicit triangle metrics.

### D) Closeness and Harmonic Centrality
- Native one-shot:
- `CXNetworkMeasureClosenessCentrality(...)` with option for `harmonic`.
- Session:
- Budget unit: processed source nodes.
- Progress: `processedSources / totalSources`.
- Python:
- `measure_closeness_centrality(...)` with `variant="closeness|harmonic"`.
- JS/WASM:
- `measureClosenessCentrality(...)`, `createClosenessSession(...)`.
- Notes:
- Reuse BFS for unweighted and Dijkstra for weighted mode.

### E) Shortest-Path Global Stats
- Native one-shot:
- `CXNetworkMeasurePathStatistics(...)` producing:
- per-node eccentricity vector
- scalar radius
- scalar diameter
- scalar average path length
- optional reachable-pairs counts
- Session:
- Budget unit: processed source nodes.
- Progress: `processedSources / totalSources`.
- Python:
- `measure_path_statistics(...)`.
- JS/WASM:
- `measurePathStatistics(...)`, `createPathStatisticsSession(...)`.
- Notes:
- For disconnected graphs, define explicit semantics in docs (reachable-only averages vs infinity handling).

### F) Edge Betweenness Centrality
- Native one-shot:
- `CXNetworkMeasureEdgeBetweennessCentrality(...)` writing edgeCapacity-sized output.
- Session:
- Budget unit: processed source nodes.
- Progress: `processedSources / totalSources`.
- Python:
- `measure_edge_betweenness_centrality(...)`.
- JS/WASM:
- `measureEdgeBetweennessCentrality(...)`, `createEdgeBetweennessSession(...)`.
- Notes:
- Reuse existing node betweenness infrastructure where possible.

### G) Stress Centrality
- Native one-shot:
- `CXNetworkMeasureStressCentrality(...)` writing nodeCapacity-sized output.
- Session:
- Budget unit: processed source nodes.
- Progress: `processedSources / totalSources`.
- Python:
- `measure_stress_centrality(...)`.
- JS/WASM:
- `measureStressCentrality(...)`, `createStressSession(...)`.
- Notes:
- Legacy reference exists in `src/Old/CXNetworkCentrality.c`; port semantics carefully.

### H) PageRank
- Native one-shot:
- `CXNetworkMeasurePageRank(...)` with damping, tolerance, maxIterations.
- Session:
- Budget unit: iteration steps.
- Progress: `iteration / maxIterations`, plus current residual delta.
- Python:
- `measure_pagerank(...)`.
- JS/WASM:
- `measurePageRank(...)`, `createPageRankSession(...)`.
- Notes:
- Support weighted transitions when edge weight attribute is provided.

### I) Assortativity
- Native one-shot:
- `CXNetworkMeasureAssortativity(...)` supporting:
- degree assortativity
- numeric node-attribute assortativity
- Session:
- Budget unit: processed edges.
- Progress: `processedEdges / activeEdges`.
- Python:
- `measure_assortativity(...)`.
- JS/WASM:
- `measureAssortativity(...)`, `createAssortativitySession(...)`.
- Notes:
- This was already flagged as future in steppable-session docs; align with that cancellation policy.

## API surface and file touch map
- Native header/API: `src/native/include/helios/CXNetwork.h`
- Native implementation: `src/native/src/CXNetworkMeasurement.c` and/or new focused files per metric
- WASM exports: `scripts/exported-functions.txt`
- JS API/session wrappers: `src/js/HeliosNetwork.js`
- Shared steppable engine: `src/js/sessions/WasmSteppableSession.js`
- Worker execution: `src/js/sessions/worker/HeliosSessionWorkerCore.js`
- Python C extension: `python/src/helios_network/_core.c`
- Python wrapper: `python/src/helios_network/_wrapper.py`
- Tests:
- native: `tests/native/test_measurements.c`
- JS: `tests/network_measurements.test.js`
- Python: `python/tests/test_network.py`
- Docs:
- `docs/steppable-sessions.md`
- `python/docs/API.md`
- `README.md`

## Done criteria per metric
- Native one-shot API implemented and validated.
- Native session API implemented for heavy metrics with monotonic progress reporting.
- WASM export + JS one-shot wrapper.
- WASM steppable session wrapper with `run` and `runWorker`.
- Version-based cancellation rules wired in JS session wrapper.
- Python one-shot API parity; session parity for heavy metrics.
- Native, JS, and Python tests added or extended.
- Docs updated with semantics and edge-case behavior.

## Open decisions to resolve before implementation
- Disconnected-graph semantics for closeness and path-length statistics.
- Directed-graph policy defaults for coreness and clustering/transitivity variants.
- Whether strong components ship in the first connected-components delivery or immediately after weak components.
- Exact weighted semantics for PageRank and shortest-path-derived metrics.
- Which metrics get one-shot only vs mandatory session support in the first iteration.
