# Selectors (Nodes & Edges)

Selectors provide iterable views over node/edge ids, plus helpers for attributes, neighbours, and incident edges. They can be populated explicitly or represent the full active set without storing ids.

## Creating selectors

- `network.createNodeSelector(indices?)` / `network.createEdgeSelector(indices?)` create selectors seeded from an array/typed array; omit `indices` to fill with all active ids at creation time.
- Attribute projection: `selector.attribute(name)` or property access via the proxy (`selector.someAttribute`).
- Topology helpers (nodes): `selector.neighbors({ includeEdges, asSelector })`, `selector.degree({ mode })`, `selector.incidentEdges({ mode, asSelector })`.
- Topology helpers (edges): `selector.sources({ asSelector, unique })`, `selector.targets(...)`, `selector.nodes({ asSelector, unique })`.
- Clean up with `selector.dispose()` when done (for non-proxy selectors created via `create*Selector`).

### Storage and ordering

- Regular selectors store indices in a contiguous `uint32` buffer in WASM; iteration reads directly from that buffer.
- `network.nodes` / `network.edges` are full-coverage selectors that **do not** store ids. They materialize a fresh copy of active ids each time via the native active-index writers (`writeActiveNodes` / `writeActiveEdges`), so you always see up-to-date membership after graph edits without manual dirty flags.
- Active-index copies use the native traversal order (increasing id scan), independent of any dense ordering you set for rendering. Dense index buffers (`getDense*IndexView`) continue to respect `setDense*Order`.

## Full coverage selectors (`network.nodes` / `network.edges`)

- `network.nodes` / `network.edges` expose selectors that represent **all active** nodes/edges without storing an index array in the selector itself.
- Iteration/materialization pulls fresh active ids via the native active-index writers (`nodeIndices` / `edgeIndices`), so results stay current after graph edits without storing the mask in JS.
- Because these helpers allocate/grow WASM memory, **do not iterate these inside `withBufferAccess`/`startBufferAccess`**; materialize the ids before entering a buffer-access block.

## Dense index copies

- `network.nodeIndices` / `network.edgeIndices` return copied `Uint32Array`s of **active ids in native order** (independent of dense order). They materialize via the native active-index writers and are cached until topology changes (adds/removes/compaction). Treat the returned array as read-only if you rely on caching.
- These helpers allocate and therefore cannot be called inside `withBufferAccess`; call them beforehand or manage dense buffers manually if you need views during a buffer-access block.

## Membership checks

- `network.hasNodeIndex(idx)` / `network.hasEdgeIndex(idx)` test a single id against the native activity bitset.
- `network.hasNodeIndices(list)` / `network.hasEdgeIndices(list)` return booleans aligned with the provided array/iterable.

## Notes on ordering and dirtiness

- Dense index buffers respect `setDenseNodeOrder` / `setDenseEdgeOrder`; selectors based on dense indices inherit that ordering.
- Dense buffers are dirtied automatically on structural changes; calling `updateDense*` on a clean buffer is a cheap no-op in native code.
