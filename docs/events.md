# Events

Helios Network exposes a small event system directly on `HeliosNetwork` so external objects can subscribe to structural and attribute changes.

It follows the browser-standard `EventTarget` model:

- `addEventListener` / `removeEventListener` / `dispatchEvent` are the primitives.
- `HeliosNetwork` adds convenience helpers: `on`, `off`, `emit`, `onAny`, and `listen`.
- Payloads are always in `event.detail` (like `CustomEvent`).

> Note on portability
>
> In browsers, `CustomEvent` is used.
> In Node.js, the code uses `CustomEvent` when available, otherwise falls back to `Event` plus a best-effort `detail` property.
> If neither `EventTarget` nor `Event` exist (uncommon), Helios Network provides minimal fallbacks so the API still works.

---

## Quick usage

```js
import HeliosNetwork, { AttributeType } from 'helios-network';

const net = await HeliosNetwork.create();

// Subscribe
const offNodes = net.on(HeliosNetwork.EVENTS.nodesAdded, (e) => {
  const { indices, nodeCount } = e.detail;
  console.log('added nodes', indices.length, 'new count', nodeCount);
});

// AbortSignal-aware subscription
const controller = new AbortController();
net.on(HeliosNetwork.EVENTS.topologyChanged, (e) => {
  console.log('topology changed', e.detail);
}, { signal: controller.signal });

// Mutate
net.addNodes(3);

// Stop
offNodes();
controller.abort();
```

---

## API

### `net.on(type, handler, options?)`

- Wraps `addEventListener`.
- If `options.signal` is provided, the listener is automatically removed when the signal aborts.
- Returns `unsubscribe()`.

Handlers are consistently `handler(event)` and read data from `event.detail`.

### `net.off(type, handler, options?)`

Thin wrapper over `removeEventListener`.

### `net.emit(type, detail)`

Creates an event whose payload is in `event.detail`, dispatches it, and forwards it to `onAny` handlers.

### `net.onAny(handler, { signal }?)`

Registers a handler that runs for every `emit(...)` call.

The handler signature is:

```js
handler({ type, detail, event, target })
```

### `net.listen("type.namespace", handlerOrNull, options?)`

Namespaced binding helper.

- Accepts strings like `"nodes:added.sidebar"`.
- Guarantees only one active listener per `(type, namespace)`.
- Calling again replaces the previous handler.
- Calling with `handler = null` removes that binding.

This is useful for UI panels or components that want “one listener per purpose” without tracking unsubscribe closures.

---

## Event names

Available event names are centralized on:

- `HeliosNetwork.EVENTS`

Current names:

- `topology:changed`
- `nodes:added`
- `nodes:removed`
- `edges:added`
- `edges:removed`
- `attribute:defined`
- `attribute:removed`
- `attribute:changed`

---

## Payloads (`event.detail`)

### Topology

#### `nodes:added`

```ts
{
  indices: Uint32Array,
  count: number,
  oldNodeCount: number,
  nodeCount: number,
  oldEdgeCount: number,
  edgeCount: number,
  topology: { node: number, edge: number },
  oldTopology: { node: number, edge: number }
}
```

#### `nodes:removed`

Same shape as `nodes:added`, but `indices` is the input removal list.

#### `edges:added`

Same shape as `nodes:added`, but `indices` are new edge indices.

#### `edges:removed`

Same shape as `nodes:removed`.

#### `topology:changed`

Emitted as a generic “something structural changed” signal.

```ts
{
  kind: 'nodes' | 'edges' | 'network',
  op: 'added' | 'removed' | 'compacted',
  topology: { node: number, edge: number },
  oldTopology: { node: number, edge: number },

  // present for compact() events
  oldNodeCount?: number,
  nodeCount?: number,
  oldEdgeCount?: number,
  edgeCount?: number
}
```

### Attributes

#### `attribute:defined`

```ts
{
  scope: 'node' | 'edge' | 'network',
  name: string,
  type: number,        // AttributeType
  dimension: number,
  version: number
}
```

#### `attribute:removed`

```ts
{
  scope: 'node' | 'edge' | 'network',
  name: string,
  previousVersion: number
}
```

#### `attribute:changed`

This is emitted when Helios Network itself changes attribute data or metadata through a managed API (e.g. string attributes, JS-managed attributes, node→edge copy, categorize/decategorize) or when user code explicitly bumps a version.

```ts
{
  scope: 'node' | 'edge' | 'network',
  name: string,
  version: number,
  op: 'set' | 'delete' | 'clear' | 'copy' | 'bump' | 'categorize' | 'decategorize',
  index: number | null
}
```

---

## Important note: direct buffer writes

Most numeric attributes are exposed as WASM-backed typed arrays (e.g. `Float32Array`). If you write directly to those views:

```js
net.defineNodeAttribute('weight', AttributeType.Float);
const { view, bumpVersion } = net.getNodeAttributeBuffer('weight');
view[nodeIndex] = 42;

// Notify downstream systems that depend on change detection:
bumpVersion();
```

Helios Network cannot automatically detect arbitrary typed-array writes, so you should call `bumpVersion()` (or `net.bumpNodeAttributeVersion('weight')`) when you want change detection / bindings to react.

---

## WASM buffer access sessions

If you are consuming WASM-backed typed arrays (dense buffers, sparse buffers, index buffers, etc.), remember that WASM memory can grow and invalidate previous views.

Use the existing buffer access helpers to avoid surprises:

- `net.withBufferAccess(() => { ... })`
- `net.startBufferAccess()` / `net.endBufferAccess()`

Inside buffer access, allocation-prone calls (like structural mutations and dense repacks) will throw.
