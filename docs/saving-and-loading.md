# File Persistence Guide

Helios exposes high-level helpers for writing and reading the human-readable `.xnet` format alongside the binary `.bxnet` container and its compressed sibling `.zxnet`. It also adds `.gml` import/export plus node-link JSON import/export for interop with common graph tooling. The same API works in Node.js and in browsers thanks to the Emscripten virtual filesystem that ships with the WASM build. This guide shows the typical workflows for saving network snapshots and restoring them in both environments, including handling `Uint8Array`, `ArrayBuffer`, `Blob`, `Response`, and Base64 payloads.

> Need the raw file layout? See the [XNET format specification](./xnet-format.md) for the authoritative grammar covering both 1.0.0 and legacy dialects.

---

## Quick Reference

- `saveBXNet(options?)`, `saveZXNet(options?)`, `saveXNet(options?)`, `saveGML(options?)`, and `saveNodeLinkJSON(options?)` serialize the active network. Without a `path`, they resolve to a `Uint8Array` (or another format when you pass `format`).
- `fromBXNet(source, options?)`, `fromZXNet(source, options?)`, `fromXNet(source, options?)`, `fromGML(source, options?)`, and `fromNodeLinkJSON(source, options?)` hydrate a new `HeliosNetwork` instance from bytes, a browser blob/response, a Node.js file path, or for node-link JSON a parsed plain object / JSON string.
- Use `.zxnet` when you want a compressed payload; `.bxnet` stays uncompressed and is cheaper to load.
- Always call `dispose()` on networks you no longer need to free native memory.
- Categorical attributes serialize their dictionaries in XNET/BXNET/ZXNET; missing values use the `-1` sentinel and default label `__NA__`.
- Multi-category attributes (including weighted sets) serialize in XNET/BXNET/ZXNET using their CSR-like buffers and categorical dictionaries.
- GML and node-link JSON are intentionally lossy for some Helios-specific payloads. When keys must be renamed or unsupported attributes are skipped, the bindings emit a warning (`console.warn` in JS, `UserWarning` in Python, `CXNetworkSerializationLastWarningMessage()` in native C).

> **Note:** `saveXNet` compacts the network so node identifiers become contiguous (`0..n-1`). The original IDs are preserved in the `_original_ids_` vertex attribute.

When the serialization exports are not present in the current WASM build, the helpers throw with a clear message. Rebuild the artefacts via `npm run build:wasm` if that happens.

### Attribute Filters

You can optionally filter which attributes are written by passing `allowAttributes` and/or `ignoreAttributes`. Each accepts an object keyed by scope:

```js
await net.saveBXNet({
  allowAttributes: {
    node: ['score', 'label'],
    edge: ['weight'],
    network: ['title'],
  },
  ignoreAttributes: {
    edge: ['debugFlag'],
  },
});
```

- `allowAttributes` keeps only the listed names per scope.
- `ignoreAttributes` removes the listed names per scope.
- If both are provided, the allow-list is applied first, then the ignore-list.
- If neither is provided, all supported attributes are saved.
- `graph` is accepted as an alias for `network`.
- `saveXNet` still writes `_original_ids_` to preserve compaction metadata.
- Attribute filters currently apply to BXNet/ZXNet/XNet only. GML and node-link JSON export all supported attributes.

### GML and Node-Link JSON Notes

- `saveGML()` / `fromGML()` target broad GML interoperability and accept looser inputs such as quoted keys and unquoted scalar strings.
- GML export always prefers safe identifiers when possible. Invalid keys are sanitized, for example `label with spaces` becomes `label_with_spaces`; collisions are deduplicated and reserved tokens are prefixed, with a warning when the export had to rename or skip something.
- `saveNodeLinkJSON()` writes a common node-link structure with top-level `graph`, `nodes`, and `links` collections, and `fromNodeLinkJSON()` reads the same layout back.
- Reserved node/link keys such as `id`, `source`, and `target` are moved into a nested `"attributes"` object during node-link JSON export.
- During node-link JSON load, Helios always preserves external node identifiers in the `_original_ids_` node string attribute.

### Node-Link JSON Schema

Helios uses a D3 / NetworkX-style node-link document:

```json
{
  "directed": true,
  "multigraph": false,
  "graph": {
    "title": "Example graph"
  },
  "nodes": [
    {
      "id": "left",
      "label": "Alpha",
      "coords": [1, 2]
    },
    {
      "id": "right",
      "attributes": {
        "score": 2.5
      }
    }
  ],
  "links": [
    {
      "source": "left",
      "target": "right",
      "weight": 3.5,
      "attributes": {
        "kind": "bridge"
      }
    }
  ]
}
```

- Top-level fields:
  - `directed`: boolean, used to create the Helios network orientation.
  - `multigraph`: accepted for compatibility and currently ignored.
  - `graph`: optional object of network-scope attributes.
  - `network`: accepted as an alias for `graph` during load.
  - `nodes`: array of node records.
  - `links`: array of edge records.
  - `edges`: accepted as an alias for `links` during load.
- Node fields:
  - `id`: the external node identifier. It can be a string, number, boolean, or JSON value. Helios stores the imported value in `_original_ids_` as a string representation.
  - Any non-reserved inline key becomes a Helios node attribute.
  - `attributes`: optional nested object whose keys are merged into the node attribute set.
- Link fields:
  - `source` / `target`: endpoint identifiers referencing node `id` values.
  - Any non-reserved inline key becomes a Helios edge attribute.
  - `attributes`: optional nested object whose keys are merged into the edge attribute set.
- Attribute type mapping on load:
  - JSON strings -> `String`
  - JSON booleans -> `Boolean`
  - JSON integers -> `Integer`
  - JSON non-integer numbers -> `Double`
  - Numeric arrays -> fixed-dimension numeric vectors
  - In JS/WASM only, arrays of strings and arrays of `{ label, weight? }` records load as multi-category attributes
  - Nested objects, mixed-type arrays, empty arrays, and other unsupported shapes are stringified as JSON text and warned about
- Tolerant load behavior:
  - Missing node records referenced by `source` / `target` are synthesized automatically and warned about.
  - Missing node `id` values are synthesized automatically and warned about.
  - Duplicate node ids are renamed with a `#<n>` suffix and warned about.
  - Non-object node/link entries are skipped with a warning.

---

## Node.js Workflows

All examples assume you are running in an ESM-capable environment on Node.js 18+. For a runnable script, see [`docs/examples/node/saving-loading`](../docs/examples/node/saving-loading/README.md).

### Save To Disk

```js
import HeliosNetwork from 'helios-network';

const net = await HeliosNetwork.create({ directed: true });
net.addNodes(4);

await net.saveZXNet({
  path: './snapshots/graph.zxnet',
  compressionLevel: 9, // optional, 0-9
});

net.dispose();
```

Passing `path` skips returning bytes to JavaScript and writes the payload straight to the filesystem. The helper creates parent directories when needed.

### Save To Memory (Uint8Array, ArrayBuffer, Base64)

```js
const raw = await net.saveBXNet(); // Uint8Array by default
const asArrayBuffer = await net.saveXNet({ format: 'arraybuffer' });
const asBase64 = await net.saveZXNet({ format: 'base64' });
```

If you need a Node.js `Buffer`, wrap the returned `Uint8Array`:

```js
const buffer = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength);
```

### Hydrate From Disk

```js
const restored = await HeliosNetwork.fromZXNet('./snapshots/graph.zxnet');
console.log(restored.nodeCount, restored.edgeCount);
restored.dispose();
```

When you pass a string in Node.js, the helpers treat it as a path (resolved relative to `process.cwd()` or the current module URL).

### Hydrate From Memory

```js
import fs from 'node:fs/promises';
import HeliosNetwork from 'helios-network';

const bytes = await fs.readFile('./snapshots/graph.bxnet');
const net = await HeliosNetwork.fromBXNet(
  bytes instanceof Uint8Array
    ? bytes
    : new Uint8Array(bytes),
);
```

### Hydrate From Node-Link JSON

```js
const restored = await HeliosNetwork.fromNodeLinkJSON('./snapshots/graph.json');
console.log(restored.getNodeStringAttribute('_original_ids_', 0));
restored.dispose();
```

The loader accepts `Uint8Array`, any typed-array view (`Float32Array`, etc.), or an `ArrayBuffer`.

### Hydrate From Base64

Convert the string yourself before handing it to Helios:

```js
const base64 = await net.saveBXNet({ format: 'base64' });
const decoded = Buffer.from(base64, 'base64');
const roundTripped = await HeliosNetwork.fromBXNet(decoded);
```

---

## Browser Workflows

The browser build bundles a virtual filesystem, so serialization works without touching the host disk. The helpers can emit `Blob` instances that you can plug into download buttons or upload flows. A full browser demo lives under [`docs/examples/browser/saving-loading`](../docs/examples/browser/saving-loading/).

### Trigger A Download

```js
import HeliosNetwork from 'helios-network';

const network = await HeliosNetwork.create();
network.addNodes(8);

const blob = await network.saveZXNet({ format: 'blob' });
const url = URL.createObjectURL(blob);

const anchor = Object.assign(document.createElement('a'), {
  href: url,
  download: 'graph.zxnet',
});
anchor.click();
URL.revokeObjectURL(url);

network.dispose();
```

### Store Bytes In Memory

```js
const arrayBuffer = await network.saveBXNet({ format: 'arraybuffer' });
const base64 = await network.saveBXNet({ format: 'base64' });
// Default (no format) returns a Uint8Array backed by the ArrayBuffer above.
```

### Load From User Uploads

```js
const input = document.querySelector('input[type="file"]');

input.addEventListener('change', async () => {
  const file = input.files?.[0];
  if (!file) return;

  const network = await HeliosNetwork.fromZXNet(file);
  console.log(network.edgeCount);
  // Remember to dispose when the network is no longer needed.
  network.dispose();
});
```

`File` extends `Blob`, so you can pass it directly. The helper reads the bytes via `blob.arrayBuffer()`.

### Load From Fetch Responses

```js
const response = await fetch('/assets/graphs/sample.bxnet');
const loaded = await HeliosNetwork.fromBXNet(response);
```

Any object that implements `arrayBuffer()` works—`Response`, `Blob`, and even custom wrappers.

### Load From Base64 Strings

Convert the string into a `Uint8Array` first:

```js
function fromBase64(base64) {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

const base64 = await network.saveBXNet({ format: 'base64' });
const decoded = fromBase64(base64);
const cloned = await HeliosNetwork.fromBXNet(decoded);
```

---

## Tips & Troubleshooting

- `.zxnet` files are smaller on disk but take longer to serialize/deserialize because of compression. Use `.bxnet` if you optimize for speed.
- The helpers run in Web Workers as long as the calling code can `await`. Just ensure you forward the `Blob`/`ArrayBuffer` across the worker boundary.
- If you are bundling for the browser, keep the `Blob` option handy—it prevents pulling large `Uint8Array` buffers into your main thread.
- Networks are independent snapshots; remember to call `dispose()` on temporary instances created via `fromBXNet`/`fromZXNet`/`fromXNet` to avoid leaking linear memory.
