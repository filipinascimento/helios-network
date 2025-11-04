# File Persistence Guide

Helios exposes high-level helpers for writing and reading the human-readable `.xnet` format alongside the binary `.bxnet` container and its compressed sibling `.zxnet`. The same API works in Node.js and in browsers thanks to the Emscripten virtual filesystem that ships with the WASM build. This guide shows the typical workflows for saving network snapshots and restoring them in both environments, including handling `Uint8Array`, `ArrayBuffer`, `Blob`, `Response`, and Base64 payloads.

> Need the raw file layout? See the [XNET format specification](./xnet-format.md) for the authoritative grammar covering both 1.0.0 and legacy dialects.

---

## Quick Reference

- `saveBXNet(options?)`, `saveZXNet(options?)`, and `saveXNet(options?)` serialize the active network. Without a `path`, they resolve to a `Uint8Array` (or another format when you pass `format`).
- `fromBXNet(source, options?)`, `fromZXNet(source, options?)`, and `fromXNet(source, options?)` hydrate a new `HeliosNetwork` instance from bytes, a browser blob/response, or a Node.js file path.
- Use `.zxnet` when you want a compressed payload; `.bxnet` stays uncompressed and is cheaper to load.
- Always call `dispose()` on networks you no longer need to free native memory.

> **Note:** `saveXNet` compacts the network so node identifiers become contiguous (`0..n-1`). The original IDs are preserved in the `_original_ids_` vertex attribute.

When the serialization exports are not present in the current WASM build, the helpers throw with a clear message. Rebuild the artefacts via `npm run build:wasm` if that happens.

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
