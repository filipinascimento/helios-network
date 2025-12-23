# Buffer & Topology Versioning

Every mutable buffer in Helios now carries a monotonically increasing `version` counter so consumers can cache-and-compare without relying on dirty flags. Versions start at `0` (never built) and wrap to `1` after `Number.MAX_SAFE_INTEGER`, keeping `0` reserved for “uninitialized”.

## What increments?

- **Sparse attributes** (node/edge/network): grow/shrink operations, slot clears during add/remove, string/JS-handle updates, and any explicit version bumps.
- **Dense attributes**: every pack/resize of a dense node/edge/color buffer bumps its `version`.
- **Topology**: structural edits (`add/remove`), dense order changes, and index-buffer packs bump `nodeTopologyVersion` / `edgeTopologyVersion`. Dense index descriptors also expose these.

## Reading versions

- Dense descriptor views (`getDense*AttributeView`, `getDenseColorEncoded*AttributeView`) include `version`. Dense index views also carry `topologyVersion`.
- Sparse attribute wrappers from `getNode/Edge/NetworkAttributeBuffer` include `version` plus a `bumpVersion()` helper.
- Call `getTopologyVersions()` for `{ node, edge }` when you only need topology.

## Manual bumps

Direct writes to sparse buffers (e.g., mutating a typed array view) should be paired with a manual bump so downstream consumers see the change:

```js
const { view, bumpVersion, version } = net.getNodeAttributeBuffer('weight');
// mutate view...
bumpVersion(); // or net.bumpNodeAttributeVersion('weight')
```

Attribute-level bumps automatically mark derived dense buffers as dirty for compatibility and are the stable “did this change?” signal going forward.

## Recommended pattern

Cache the last seen version alongside your GPU buffer or derived computation:

```js
let lastPositionsVersion = 0;

function uploadPositions(net) {
  net.updateDenseNodeAttributeBuffer('position');
  const dense = net.getDenseNodeAttributeView('position');
  if (dense.version !== lastPositionsVersion) {
    gl.bufferData(GL_ARRAY_BUFFER, dense.view, GL_STATIC_DRAW);
    lastPositionsVersion = dense.version;
  }
}
```

Use the same approach for topology-sensitive data (compare `topologyVersion`) and sparse sources.

## Dirty flags are deprecated

Dirty booleans remain on descriptors for backward compatibility, and `markDense*Dirty` is still available, but calls now log a deprecation warning. Prefer version checks for change detection.

## Quick transition guide

- Replace dirty checks with version comparisons (`dense.version`, `dense.topologyVersion`, `get*AttributeVersion`).
- After writing sparse buffers directly, call `bump*AttributeVersion` (or `buffer.bumpVersion()`).
- Keep using `updateDense*` / `updateDenseColorEncoded*` to repack; consume views via `getDense*View` and cache the returned `version`.
- You can ignore dirty flags unless migrating old code; they’re kept only for compatibility.

## Version bump propagation (what triggers what?)

- **Topology edits** (add/remove nodes/edges, index repacks)
  - Bumps `nodeTopologyVersion` or `edgeTopologyVersion`.
  - Index dense buffers repack on next `updateDense*IndexBuffer`, stamping `sourceVersion` with the topology version.
  - Color-encoded `$index` buffers repack on next update (source = topology version).
- **Dense order changes** (`setDenseNodeOrder`, `setDenseEdgeOrder`)
  - Do *not* bump topology; they invalidate dense buffers by resetting `sourceVersion` so the next update repacks with the same source versions.
  - Color-encoded buffers for that scope are invalidated the same way.
- **Sparse attribute edits** (writes to typed views, string/JS handle changes, slot clears on add/remove)
  - Bump the attribute’s `version`.
  - Dependent dense buffers repack on next `updateDense*AttributeBuffer` when `buffer.sourceVersion` differs.
  - Passthrough edge attributes: bumping the source node attribute also invalidates dependent edge dense buffers (`sourceVersion` reset).
  - Color-encoded buffers using that attribute repack on next update (sourceVersion comparison).
- **Manual bumps** (`bump*AttributeVersion`)
  - Same propagation as sparse edits: marks dependent dense/color/passthrough buffers for repack by resetting their `sourceVersion`.
- **Color-encoded buffers**
  - Track their own `version` and `sourceVersion`; repack only when the source attribute/topology version changes or on first build.
- **Dense buffers**
  - Track `version` and `sourceVersion`; repack only when source changes, order changes (sourceVersion reset), or first build.
