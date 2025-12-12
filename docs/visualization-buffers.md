# Visualization Buffers (Dense Packing)

Dense buffers give WebGL/WebGPU a tightly packed snapshot of attributes and ids without forcing the core to stay dense at all times. They pack active nodes/edges into contiguous storage, aligned across attributes and index buffers, and expose valid source ranges so you can slice the original sparse buffers when needed.

## Core concepts

- **Registration**: declare which attributes you want dense snapshots of.
  - C: `CXNetworkAddDenseNodeAttribute`, `CXNetworkAddDenseEdgeAttribute`
  - JS: `addDenseNodeAttributeBuffer(name)`, `addDenseEdgeAttributeBuffer(name)`
- **Ordering**: set one order per scope (nodes/edges). All dense attributes + the index buffer use the same order.
  - C: `CXNetworkSetDenseNodeOrder(order,count)`, `CXNetworkSetDenseEdgeOrder(...)`
  - JS: `setDenseNodeOrder(order)`, `setDenseEdgeOrder(order)`
- **Packing**: refresh dense buffers on demand, then read typed views.
  - C: `CXNetworkUpdateDenseNodeAttribute`, `CXNetworkUpdateDenseEdgeAttribute`
  - JS: `updateDenseNodeAttributeBuffer(name)`, `updateDenseEdgeAttributeBuffer(name)` + `getDenseNodeAttributeView(name)` / `getDenseEdgeAttributeView(name)`
  - Index buffers: `updateDenseNodeIndexBuffer()`, `updateDenseEdgeIndexBuffer()` + `getDenseNodeIndexView()` / `getDenseEdgeIndexView()`
- **Dirty tracking**: dense buffers are auto-marked dirty on structural edits. Mark them yourself after attribute writes:
  - JS: `markDenseNodeAttributeDirty(name)`, `markDenseEdgeAttributeDirty(name)`
- **Valid ranges** (network-level): `network.nodeValidRange` / `network.edgeValidRange` return `{start,end}` based on active ids. Use these to slice original sparse attribute buffers when you want to avoid unused capacity. Dense descriptors keep `count`/`stride` for packed views; valid ranges refer to source indices.

Dense descriptor shape in JS:
```js
{
  view: Uint8Array,      // backing storage
  pointer: number,       // wasm pointer for TypedArray views
  count: number,         // packed entries
  stride: number,        // bytes per packed entry
  capacity: number,      // bytes allocated
  validStart: number,    // smallest original index copied
  validEnd: number,      // largest original index copied + 1
  dirty: boolean         // true if needs rebuild
}
```

## Node example (positions + ids)

```js
import HeliosNetwork, { AttributeType } from 'helios-network';

const net = await HeliosNetwork.create({ directed: false });
const nodes = net.addNodes(3);
net.defineNodeAttribute('position', AttributeType.Float, 4);
net.withBufferAccess(() => {
  const pos = net.getNodeAttributeBuffer('position').view;
  for (let i = 0; i < nodes.length; i++) {
    const base = nodes[i] * 4;
    pos[base + 0] = i;
    pos[base + 1] = i + 0.5;
    pos[base + 2] = 0;
    pos[base + 3] = 1;
  }
});

net.addDenseNodeAttributeBuffer('position');

// Use a custom order; applied to all node dense buffers by default
const order = Uint32Array.from(nodes).reverse();
net.setDenseNodeOrder(order);

net.updateDenseNodeAttributeBuffer('position'); // may allocate/grow
net.updateDenseNodeIndexBuffer();               // same order

net.withBufferAccess(() => {
  const densePos = net.getDenseNodeAttributeView('position'); // typed view
  const denseIds = net.getDenseNodeIndexView();

  const posFloats = densePos.view; // Float32Array sized to active count
  const ids = denseIds.view;       // Uint32Array of packed ids

  // valid range of source indices (original ids)
  console.log(net.nodeValidRange); // { start: 0, end: 3 }
});
```

## Edge example (capacities + ids + index buffer)

```js
net.defineEdgeAttribute('capacity', AttributeType.Float, 1);
const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }, { from: nodes[1], to: nodes[2] }]);
net.withBufferAccess(() => {
  const cap = net.getEdgeAttributeBuffer('capacity').view;
  cap[edges[0]] = 1.5;
  cap[edges[1]] = 2.5;
});

net.addDenseEdgeAttributeBuffer('capacity');
net.setDenseEdgeOrder(edges); // optional

net.updateDenseEdgeAttributeBuffer('capacity'); // may allocate/grow
net.updateDenseEdgeIndexBuffer();
const denseCap = net.getDenseEdgeAttributeView('capacity'); // typed view
const denseEdgeIds = net.getDenseEdgeIndexView();

const capFloats = denseCap.view;
const edgeIds = denseEdgeIds.view;
console.log(edgeIds); // ids in the packed order (aligned with denseCap)
```

## Propagate node attributes to edges

When edge shaders need endpoint data (positions, sizes, colors, etc.), use a **passthrough edge attribute** (recommended) or the legacy dedicated dense buffer.

### Recommended: passthrough edge attribute (no sparse edge buffer required)

```js
net.defineNodeAttribute('size', AttributeType.Float, 1);
// ...write sizes into the sparse node buffer...

net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both'); // defines edge attr + dense buffer
net.updateDenseEdgeAttributeBuffer('size_passthrough'); // copies from nodes into edge attr, then packs; respects setDenseEdgeOrder
net.updateDenseEdgeIndexBuffer();                      // same order as denseSizes
const denseSizes = net.getDenseEdgeAttributeView('size_passthrough');
const edgeIndices = net.getDenseEdgeIndexView();

const sizes = denseSizes.view; // Float32Array [from,to,from,to...]
const indices = edgeIndices.view;
// sizes is [fromSize, toSize, ...] aligned with indices
```

`endpoints`: `"both"`/`-1` copies `[from,to]`; `"source"`/`0` or `"destination"`/`1` copies one endpoint. `doubleWidth` (default `true`) duplicates a single endpoint into a double-width layout. Dirtying the source node attribute via `markDenseNodeAttributeDirty` auto-dirties all dependent passthrough edge attributes; structural edits also mark them dirty. Passthrough edge names must be unused edge attributes; remove an existing edge attribute first if you want to reuse the name. Cleanup options: `removeNodeToEdgeAttribute(edgeAttr)` unregisters a passthrough; `removeEdgeAttribute`/`removeNodeAttribute`/`removeNetworkAttribute` drop sparse attributes (and associated dense tracking); `removeDense*` calls remove only dense registrations.

If you want a sparse edge buffer seeded from nodes (to tweak afterward), call:

```js
net.copyNodeAttributeToEdgeAttribute('size', 'size_edge', 'destination', /* doubleWidth */ false);
// ... mutate edge attribute if desired ...
net.updateDenseEdgeAttributeBuffer('size_edge');
const dense = net.getDenseEdgeAttributeView('size_edge');
```

Both helpers use the native copy path; dense packing still honours `setDenseEdgeOrder`.

## Stable buffer access

- `updateDense*` methods may grow memory; call them **before** grabbing views.
- Use `getDense*View` (or `withDenseBufferViews`) to materialize typed views without repacking. Sparse attribute views (`getNodeAttributeBuffer`, `getEdgeAttributeBuffer`) are WASM-backed too; if you need them stable during a render/update, obtain them inside a buffer access block as well.
- Wrap render-time access in `withBufferAccess`/`startBufferAccess` + `endBufferAccess` to block accidental allocations. Allocation-prone calls inside the block throw immediately. This protects both dense and sparse views from being invalidated by heap growth mid-use.

Example:

```js
net.updateDenseNodeAttributeBuffer('position');
net.updateDenseEdgeAttributeBuffer('capacity');
net.updateDenseNodeIndexBuffer();

net.withDenseBufferViews([['node', 'position'], ['edge', 'capacity'], ['node', 'index']], (buffers) => {
  // Safe: any allocation attempt here throws.
  const positions = buffers.node.position.view;
  const capacities = buffers.edge.capacity.view;
  const nodeIndices = buffers.node.index.view;
  const posSparse = net.getNodeAttributeBuffer('position').view; // also safe here
  // ...upload to GPU...
});
```

## Source slicing via valid ranges

If you want a view over the original sparse buffer without walking full capacity, use the network-level valid range:

```js
const { view: activeSlice, start, end } = net.getNodeAttributeBufferSlice('position');
// activeSlice is already trimmed to the valid range; use start/end for ids if needed
```

`start`/`end` are always in source index space (original node/edge ids), independent of dense position.

Or call the helpers that slice for you and return metadata:

```js
const slice = net.getNodeAttributeBufferSlice('position');
// for edges: net.getEdgeAttributeBufferSlice('capacity')
// slice.view is already trimmed to the valid [start,end) range
console.log(slice.start, slice.end, slice.stride); // min/max ids, stride bytes
```
