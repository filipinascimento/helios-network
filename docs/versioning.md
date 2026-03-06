# Versioning

- Sparse attributes expose versions through `get*AttributeVersion(...)` and the
  wrappers returned by `get*AttributeBuffer(...)`.
- Topology changes update `nodeTopologyVersion` / `edgeTopologyVersion`.
- `nodeIndices` / `edgeIndices` carry the matching topology version on the
  returned `Uint32Array` via a `version` property.

Use version comparisons for change detection. If you mutate WASM-backed buffers
directly, call the relevant `bump*AttributeVersion(...)` helper afterward.
