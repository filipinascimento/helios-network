# Visualization Buffers (Dense Packing)

Dense buffers give WebGL/WebGPU a tightly packed snapshot of attributes and ids without forcing the core to stay dense at all times. They pack active nodes/edges into contiguous storage, aligned across attributes and index buffers, and expose valid source ranges so you can slice the original sparse buffers when needed.

## Core concepts

- **Registration**: declare which attributes you want dense snapshots of.
  - C: `CXNetworkAddDenseNodeAttribute`, `CXNetworkAddDenseEdgeAttribute`
  - JS: `addDenseNodeAttributeBuffer(name)`, `addDenseEdgeAttributeBuffer(name)`
- **Ordering**: set one order per scope (nodes/edges). All dense attributes + the index buffer use the same order unless you override per-call.
  - C: `CXNetworkSetDenseNodeOrder(order,count)`, `CXNetworkSetDenseEdgeOrder(...)`
  - JS: `setDenseNodeOrder(order)`, `setDenseEdgeOrder(order)`
- **Packing**: refresh dense buffers on demand; returns metadata.
  - C: `CXNetworkUpdateDenseNodeAttribute`, `CXNetworkUpdateDenseEdgeAttribute`
  - JS: `updateDenseNodeAttributeBuffer(name[, order])`, `updateDenseEdgeAttributeBuffer(...)`
  - Index buffers: `updateDenseNodeIndexBuffer([order])`, `updateDenseEdgeIndexBuffer([order])`
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
const pos = net.getNodeAttributeBuffer('position').view;
for (let i = 0; i < nodes.length; i++) {
  const base = nodes[i] * 4;
  pos[base + 0] = i;
  pos[base + 1] = i + 0.5;
  pos[base + 2] = 0;
  pos[base + 3] = 1;
}

net.addDenseNodeAttributeBuffer('position');

// Use a custom order; applied to all node dense buffers by default
const order = Uint32Array.from(nodes).reverse();
net.setDenseNodeOrder(order);

const densePos = net.updateDenseNodeAttributeBuffer('position'); // uses setDenseNodeOrder
const denseIds = net.updateDenseNodeIndexBuffer();               // same order

const posFloats = new Float32Array(
  densePos.view.buffer,
  densePos.pointer,
  densePos.count * 4
);
const ids = new Uint32Array(denseIds.view.buffer, denseIds.pointer, denseIds.count);

// valid range of source indices (original ids)
console.log(net.nodeValidRange); // { start: 0, end: 3 }
```

## Edge example (capacities + ids + index buffer)

```js
net.defineEdgeAttribute('capacity', AttributeType.Float, 1);
const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }, { from: nodes[1], to: nodes[2] }]);
const cap = net.getEdgeAttributeBuffer('capacity').view;
cap[edges[0]] = 1.5;
cap[edges[1]] = 2.5;

net.addDenseEdgeAttributeBuffer('capacity');
net.setDenseEdgeOrder(edges); // optional

const denseCap = net.updateDenseEdgeAttributeBuffer('capacity'); // respects edge order
const denseEdgeIds = net.updateDenseEdgeIndexBuffer();

const capFloats = new Float32Array(denseCap.view.buffer, denseCap.pointer, denseCap.count);
const edgeIds = new Uint32Array(denseEdgeIds.view.buffer, denseEdgeIds.pointer, denseEdgeIds.count);
console.log(edgeIds); // ids in the packed order (aligned with denseCap)
```

## Propagate node attributes to edges

When edge shaders need endpoint data (positions, sizes, colors, etc.), register a dense node→edge buffer and pack it alongside your edge index buffer:

```js
net.defineNodeAttribute('size', AttributeType.Float, 1);
// ...write sizes into the sparse node buffer...

net.addDenseNodeToEdgeAttributeBuffer('size');
const denseSizes = net.updateDenseNodeToEdgeAttributeBuffer('size'); // respects setDenseEdgeOrder
const edgeIndices = net.updateDenseEdgeIndexBuffer();               // same order as denseSizes

const sizes = new Float32Array(denseSizes.view.buffer, denseSizes.pointer, denseSizes.count * 2);
const indices = new Uint32Array(edgeIndices.view.buffer, edgeIndices.pointer, edgeIndices.count);
// sizes is [fromSize, toSize, ...] aligned with indices
```

`updateDenseNodeToEdgeAttributeBuffer` returns a descriptor with `dirty` status; use `peekDenseNodeToEdgeAttributeBuffer` / `peekDenseEdgeIndexBuffer` when you just need the last-packed views without forcing an update.

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
