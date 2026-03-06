# Selectors (Nodes & Edges)

Selectors provide iterable views over node/edge ids plus helpers for attributes,
neighbors, and incident edges. They can be populated explicitly or represent the
full active set without storing ids in the selector itself.

## Creating selectors

- `network.createNodeSelector(indices?)` / `network.createEdgeSelector(indices?)`
  create selectors seeded from an array or typed array. Omit `indices` to fill
  with all active ids at creation time.
- Attribute projection: `selector.attribute(name)` or property access via the
  proxy (`selector.someAttribute`).
- Topology helpers (nodes): `selector.neighbors(...)`,
  `selector.neighborsAtLevel(...)`, `selector.neighborsUpToLevel(...)`,
  `selector.degree(...)`, `selector.incidentEdges(...)`.
- Topology helpers (edges): `selector.sources(...)`, `selector.targets(...)`,
  `selector.nodes(...)`.
- Clean up with `selector.dispose()` when done for selectors created via
  `create*Selector`.

## Storage and ordering

- Regular selectors store indices in a contiguous `uint32` buffer in WASM.
- `network.nodes` / `network.edges` are full-coverage selectors that do not keep
  their own id list. They read the active ids from the network on demand.
- Active ids always follow native traversal order (increasing id scan).

## Active index views

- `network.nodeIndices` / `network.edgeIndices` expose WASM-backed `Uint32Array`
  views of the active ids in native order.
- They are backed by persistent Helios-managed buffers, so repeated reads do not
  materialize JS copies.
- They follow the same safety rules as attribute buffers:
  - do allocation-prone work before reading the view
  - use `withBufferAccess(...)` when a block of code needs the view to stay
    stable
  - do not cache the typed array across calls that may grow WASM memory
- The views carry a `version` property that tracks the corresponding topology
  version.

## Membership checks

- `network.hasNodeIndex(idx)` / `network.hasEdgeIndex(idx)` test a single id.
- `network.hasNodeIndices(list)` / `network.hasEdgeIndices(list)` return
  booleans aligned with the provided array or iterable.
