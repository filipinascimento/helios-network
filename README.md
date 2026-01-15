# Helios Network v2

Helios Network is a high–performance graph store written in C17/C23 and compiled to WebAssembly. It provides:

- **Native core** optimized for linear-memory storage of nodes, edges, and attributes.
- **JavaScript bindings** that expose the WASM module through a modern, asynchronous API.
- **Selectors** and attribute utilities for efficient analytics workflows, including WebGPU-friendly buffers.

The project is designed to run inside the browser (via ES modules) and under Node.js for testing or server-side execution.

---

## Table of Contents

1. [Project Structure](#project-structure)
2. [Prerequisites](#prerequisites)
3. [Setup & Installation](#setup--installation)
4. [Editor Integration](#editor-integration)
5. [Building](#building)
6. [Testing](#testing)
7. [JavaScript Usage](#javascript-usage)
8. [Examples](#examples)
9. [C API Documentation](#c-api-documentation)
10. [JavaScript API Documentation](#javascript-api-documentation)
11. [Generating Docs](#generating-docs)
12. [Contributing](#contributing)
13. [Packaging](#packaging)

---

## Project Structure

```
.
├── compiled/                     # Generated wasm+mjs artefacts
├── dist/                         # Bundled JS (vite build)
├── src/
│   ├── native/                   # C source and headers for the core engine
│   │   ├── include/helios/       # Public headers (CXNetwork.h, ...)
│   │   └── src/                  # Implementation files
│   ├── js/HeliosNetwork.js       # JS wrapper with HeliosNetwork class
│   └── helios-network.js         # Entry point re-exporting JS API
├── tests/
│   ├── node_helios.test.js       # Node-based Vitest suite
│   └── core_features.test.js     # Frontend-level unit tests
├── package.json
├── Makefile / meson.build        # Build orchestration
└── README.md
```

---

## Prerequisites

- **Node.js** 18 or newer (tested on 22.x).
- **npm** (ships with Node) for package management.
- **Emscripten** latest SDK (see <https://emscripten.org/docs/getting_started/index.html>). After installing, ensure `emcc` is in your PATH.
- **Python 3** (for Meson/Ninja) if you plan to rebuild the WASM artefacts.

Optional (auto-installed via pip or npm in this repo’s workflow):
- Meson >= 1.9
- Ninja >= 1.13
- JSDoc 4.0 for JS reference generation

---

## Setup & Installation

```bash
# install JS dependencies
echo "Installing npm packages" && npm install

# (Optional) install Meson/Ninja via pip (if not already available)
python3 -m pip install --user meson ninja

# For convenience, expose pip-installed tools to the repo
ln -s ~/Library/Python/3.9/bin/meson node_modules/.bin/meson
ln -s ~/Library/Python/3.9/bin/ninja node_modules/.bin/ninja
```

> The symlinks are only required if Meson/Ninja aren’t globally on your PATH.

---

## Editor Integration

- Set the `EMSDK` environment variable to your local Emscripten SDK root (for example `export EMSDK=$HOME/emsdk`) so VS Code can resolve toolchain headers without per-machine absolute paths.
- Run `npm run build:wasm` after cloning to generate `builddir/compile_commands.json`. The C/C++ extension consumes this file to mirror the exact compiler flags used by `emcc`, which unlocks accurate diagnostics and go-to-definition for the native sources.
- The checked-in `.vscode` settings now point IntelliSense at `src/native/include` and the Meson build directory. Once the compile commands file exists, header navigation and symbol lookup should work across both the C and JavaScript layers.

---

## Building

### JavaScript Bundles

The `build` script produces both ESM and UMD bundles under `dist/` using Vite and Rollup. Rollup’s native binary is skipped for compatibility via an environment flag.

```bash
npm run build
```

Expected output (abridged):

```
vite v6.x building for production...
dist/helios-network.js            # ESM bundle
dist/helios-network.umd.cjs       # UMD bundle
dist/helios-network.inline.js     # ESM bundle with the WASM inlined as base64
```

### WebAssembly Core

`build:wasm` invokes Meson/Ninja to recompile the C sources to WebAssembly via `emcc`:

```bash
npm run build:wasm
```

This regenerates `compiled/CXNetwork.{mjs,wasm}`. The JS layer imports the `.mjs` wrapper dynamically.

> The script prepends `node_modules/.bin` to `PATH` so local Meson/Ninja symlinks are also detected.

### Native C Core

The same C sources can be compiled into host libraries when you need a native build instead of WebAssembly.

- **CMake**  
  Configure once with the desired library types and build:
  ```bash
  cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release \
    -DHELIOS_BUILD_STATIC=ON -DHELIOS_BUILD_SHARED=ON
  cmake --build build/native
  ```
  Disable one of the toggles (`HELIOS_BUILD_STATIC`/`HELIOS_BUILD_SHARED`) if you only want a static or shared artefact. The resulting `libhelios.{a,dylib|so}` lives under `build/native/`; install headers and libraries via `cmake --install build/native --prefix <dest>`.

- **Makefile**  
  The GNUmakefile wraps the same flow: `make native` builds both flavours, or call `make native-static` / `make native-shared` individually. Outputs land in `build/native/`.

- **Meson (WASM)**  
  The checked-in `meson.build` is tuned for the Emscripten toolchain and is what powers `npm run build:wasm`. Invoking it with the system compiler (`meson setup … && meson compile …`) will use strict `-std=c17`, which on macOS hides extensions such as `srandomdev`/`vasprintf` and causes the build to fail unless you add something like `-Dc_args=-D_DARWIN_C_SOURCE`. If you need a host build, prefer the CMake or Make targets above.

### Makefile Shortcut

For convenience, a legacy `make compile` target exists. It directly calls `emcc` with equivalent flags; use it only if you prefer Make over Meson.

---

## Testing

Vitest is configured for Node execution. The `test` script excludes Playwright browser tests by default; the Node-based suite is the authoritative check for the WASM bindings.

```bash
npm test
```

Tests currently cover:
- Node/edge creation/removal
- Primitive, string, and JavaScript-managed attributes
- Selector behaviour and typed-array exports
- Event subscriptions (`on`/`listen`/`AbortSignal`) and change notifications

Additional docs:
- `docs/events.md` for the HeliosNetwork events system

Run the Playwright browser suite (this command starts the Vite dev server automatically):

```bash
# Optional – only if you need browser coverage
npm run test:browser
# CI-friendly alias (same flow, headless)
npm run test:browser:ci
```

Pass extra flags (e.g. `--headed`) via `npx playwright test --headed` if you prefer an interactive run.

---

## JavaScript Usage

The JS entrypoint exports `HeliosNetwork` as the default export plus helper enums and selectors. `addEdges` accepts several input shapes: flattened `Uint32Array`/`number[]`, `[from, to]` tuples, or `{from, to}` objects. Typical workflow:

```js
import HeliosNetwork, { AttributeType } from 'helios-network';

const net = await HeliosNetwork.create({ directed: true, initialNodes: 10 });

// Add nodes/edges
const newNodes = net.addNodes(5);
const edges = net.addEdges([
  { from: newNodes[0], to: newNodes[1] },
  { from: newNodes[1], to: newNodes[2] },
]);
// or: net.addEdges([[newNodes[0], newNodes[1]], [newNodes[1], newNodes[2]]]);
// or: net.addEdges([newNodes[0], newNodes[1], newNodes[1], newNodes[2]]);

// Define an attribute
net.defineNodeAttribute('weight', AttributeType.Float, 1);
const { view } = net.getNodeAttributeBuffer('weight');
view[newNodes[0]] = 1.25;

// Work with strings / JS-managed attributes
net.defineEdgeAttribute('label', AttributeType.String);
net.setEdgeStringAttribute('label', edges[0], 'a→b');

net.defineNetworkAttribute('metadata', AttributeType.Javascript);
const meta = net.getNetworkAttributeBuffer('metadata');
meta.set(0, { description: 'Halo subgraph' });

// Neighbour iteration
const { nodes, edges: incidentEdges } = net.getOutNeighbors(newNodes[0]);

// Selectors are iterable proxies that surface attributes & topology helpers
const selector = net.createNodeSelector([newNodes[0], newNodes[1]]);
console.log([...selector]);                 // -> node indices
console.log(selector.weight);               // -> [1.25, ...] via attribute proxy
const { nodes: neigh } = selector.neighbors({ includeEdges: false });
console.log(Array.from(neigh));             // -> neighbour node ids
const edgeSelector = net.createEdgeSelector(edges);
console.log(edgeSelector.label);            // -> ['a→b', null, ...]
selector.dispose();
edgeSelector.dispose();

net.dispose();
```

### Fixed active index buffers

Use the new zero-copy helpers to avoid per-frame allocations when building geometry:

```js
// Stable view that only reallocates when the native buffer grows
const { view, count } = net.getActiveEdgeIndexView();
const activeEdges = view.subarray(0, count);

// Or, fill a caller-managed buffer and grow on demand
const ptr = net.module._malloc(1024 * Uint32Array.BYTES_PER_ELEMENT);
const scratch = new Uint32Array(net.module.HEAPU32.buffer, ptr, 1024);
let needed = net.writeActiveEdges(scratch);
if (needed > scratch.length) {
  // reallocate + retry with the reported size
}
net.module._free(ptr);

// Move the heavy vec4 copy for active edges into the WASM core
const segmentPtr = net.module._malloc(8 * 4 * 1024);
const segmentBuffer = new Float32Array(net.module.HEAPF32.buffer, segmentPtr, 8 * 1024);
const edgesWritten = net.writeActiveEdgeSegments(net.getNodeAttributeBuffer('position').view, segmentBuffer, 4);
net.module._free(segmentPtr);

// Or use dense packing to propagate node attributes to edges without manual copies (passthrough)
net.defineNodeToEdgeAttribute('position', 'position_endpoints', 'both');
net.updateDenseEdgeAttributeBuffer('position_endpoints'); // aligned with updateDenseEdgeIndexBuffer
const denseEdgePositions = net.getDenseEdgeAttributeView('position_endpoints');
const posPairs = denseEdgePositions.view;
const lastEdgeIds = net.getDenseEdgeIndexView();
```

`writeActiveNodes` / `writeActiveEdges` return the required length; nothing is written when the provided buffer is too small. `writeActiveEdgeSegments` writes two vectors per edge into `segments` using the requested stride (`componentsPerNode`). For dense workflows you now have two paths:

- **Passthrough edges (recommended for render data)**: `defineNodeToEdgeAttribute(sourceNodeAttr, edgeAttr, endpoints='both', doubleWidth=true)` declares an edge attribute derived from nodes. Calling `updateDenseEdgeAttributeBuffer(edgeAttr)` will copy from nodes into the edge buffer (using native code) and then pack it densely. Bump the source node attribute version (or call `bumpNodeAttributeVersion`) after writes so dependents see the change.
- **Manual sparse copy**: `copyNodeAttributeToEdgeAttribute(sourceNodeAttr, edgeAttr, endpoints='both', doubleWidth=true)` fills the sparse edge attribute once (honouring endpoint/duplication), then you can mutate it further and call `updateDenseEdgeAttributeBuffer` yourself.

Cleanup: remove dense tracking with `removeDenseNodeAttributeBuffer` / `removeDenseEdgeAttributeBuffer` or unregister a passthrough via `removeNodeToEdgeAttribute(edgeAttr)`. To delete sparse attributes entirely (and their dense buffers), call `removeNodeAttribute`, `removeEdgeAttribute`, or `removeNetworkAttribute`. A passthrough edge name must not already exist as a defined edge attribute.

Prefer the dense APIs for render paths; the `writeActive*` helpers remain available for low-level, caller-managed buffers when you need direct WASM writes without packing.

### Browser bundlers & inline WASM

Helios now ships two JavaScript entry points:

- `helios-network` (default) loads the `.wasm` artefact from `dist/compiled/` at runtime. This is ideal for Node.js and environments where you control asset delivery.
- `helios-network/inline` embeds the WASM bytes in the bundle as a base64 string. This build is exposed through the `browser` export condition, so bundlers such as Vite, Webpack, and Rspack pick it automatically without custom configuration.

Explicitly opt into the inline build if you want to avoid the automatic condition:

```js
import HeliosNetwork from 'helios-network/inline';

const net = await HeliosNetwork.create();
```

Both builds export the same API surface. The inline version increases the JS payload slightly but removes the need for `assetsInclude`/`optimizeDeps.exclude` tweaks when consuming the library.

#### Importing the package in different environments

- **Vite / modern ESM bundlers** – Install via npm and import the package normally:
  ```js
  import HeliosNetwork from 'helios-network';
  ```
  Because the package.json exposes a `browser` condition, Vite automatically switches to the inline build so you do not need extra `assetsInclude` or `optimizeDeps.exclude` configuration. If you explicitly want the inline build (for example to pin the behaviour in a monorepo), import `helios-network/inline`.
- **Node.js (ESM or CommonJS)** – The default entry reuses the on-disk WASM, so simply `import HeliosNetwork from 'helios-network';` or `const HeliosNetwork = require('helios-network');`. No bundler tweaks are required because Node can read `dist/compiled/CXNetwork.wasm` directly.
- **UMD/browser scripts** – Use the file exposed through `unpkg`/`jsdelivr` (`dist/helios-network.umd.cjs`). Example:
  ```html
  <script src="https://cdn.jsdelivr.net/npm/helios-network/dist/helios-network.umd.cjs"></script>
  <script>
    HeliosNetwork.default.create().then((net) => {
      // …
      net.dispose();
    });
  </script>
  ```
  In UMD mode the module exports both named and default exports on the global `HeliosNetwork` object.

Once the WebAssembly module has been initialised (for example by awaiting `HeliosNetwork.create()` or `getHeliosModule()` once), additional networks can be created synchronously through `HeliosNetwork.createSync()` if you prefer to stay in synchronous code.

### Key Classes & Functions

- `HeliosNetwork.create(options)` → Promise resolving to a network instance.
- `HeliosNetwork.createSync(options)` → Synchronous factory that reuses an already loaded WASM module.
- `HeliosNetwork.addNodes(count)` / `addEdges(list)` → return typed-array copies of new indices.
- Attribute helpers (`define*Attribute`, `get*AttributeBuffer`, `set*StringAttribute`, etc.).
- Selectors (`createNodeSelector`, `createEdgeSelector`) now behave as iterable proxies with attribute accessors and topology helpers.
- Static export `AttributeType` enumerates supported attribute kinds.

#### Graph-level attributes

- Use `defineNetworkAttribute(name, type, dimension?)` to declare a graph property stored in linear memory.
- Primitive buffers expose index `0` as the graph slot (e.g., `getNetworkAttributeBuffer('weight').view[0] = 42`).
- `setNetworkStringAttribute`/`getNetworkStringAttribute` and JavaScript-managed attributes (`AttributeType.Javascript`) provide convenience accessors.

### Serialization & Persistence

The WASM core exposes the human-readable `.xnet` (XNET 1.0.0) format alongside the native `.bxnet` (binary) and `.zxnet` (BGZF-compressed) containers directly to JavaScript.

- `saveBXNet(options?)` / `saveZXNet(options?)` / `saveXNet(options?)` return serialized bytes (default `Uint8Array`). In Node you may also pass `{ path: '/tmp/graph.xnet' }` (or `.bxnet` / `.zxnet`) to persist the file on disk. Optional `format` values include `'arraybuffer'`, `'base64'`, and `'blob'` (browser-friendly).
- `HeliosNetwork.fromBXNet(source)` / `HeliosNetwork.fromZXNet(source)` / `HeliosNetwork.fromXNet(source)` hydrate a new network from a `Uint8Array`, `ArrayBuffer`, `Blob`/`Response` (browser), or filesystem path (Node).
- `compact({ nodeOriginalIndexAttribute?, edgeOriginalIndexAttribute? })` rewrites the network so node/edge IDs become contiguous while preserving JavaScript-managed and string attribute stores. When attribute names are provided, the original indices are copied into unsigned integer buffers for audit trails.

#### Node.js example

```js
import fs from 'node:fs/promises';
import HeliosNetwork, { AttributeType } from 'helios-network';

const net = await HeliosNetwork.create({ directed: true });
const nodes = net.addNodes(2);
net.addEdges([{ from: nodes[0], to: nodes[1] }]);
net.defineNodeAttribute('weight', AttributeType.Float);
net.getNodeAttributeBuffer('weight').view[nodes[0]] = 2.5;

// Persist a compressed `.zxnet` file to disk.
await net.saveZXNet({ path: './data/graph.zxnet', compressionLevel: 6 });

// Capture a binary `.bxnet` payload in memory (Uint8Array by default).
const payload = await net.saveBXNet();

// Hydrate a fresh network from the serialized bytes.
const restored = await HeliosNetwork.fromBXNet(payload);
console.log(restored.nodeCount, restored.edgeCount); // 2 nodes, 1 edge

restored.dispose();
net.dispose();
```

#### Browser example

```js
import HeliosNetwork from 'helios-network';

const network = await HeliosNetwork.create();
network.addNodes(4);

// Downloadable blob – ideal for user-triggered "Save" buttons.
const blob = await network.saveBXNet({ format: 'blob' });
const url = URL.createObjectURL(blob);
const anchor = Object.assign(document.createElement('a'), {
  href: url,
  download: 'graph.bxnet',
});
anchor.click();
URL.revokeObjectURL(url);

// Round-trip the blob back into a live network.
const rehydrated = await HeliosNetwork.fromBXNet(blob);
console.log(rehydrated.nodeCount); // 4

rehydrated.dispose();
network.dispose();
```

For full Node.js and browser walkthroughs—saving to disk, generating downloads, and round-tripping Base64/typed-array payloads—see `docs/saving-and-loading.md`.

Full JSDoc comments inside `src/js/HeliosNetwork.js` describe signatures and behaviours. You can generate HTML docs as shown below.

Selectors, traversal helpers, and dense index conveniences are covered in [`docs/selectors.md`](docs/selectors.md).

---

## Examples

Ready-to-run samples live in [`docs/examples`](docs/examples/README.md). Each platform contains topic-specific folders for:

- **Browser** – ESM demos covering basic usage, attributes, iteration, mutations, and saving/loading (`docs/examples/browser/*`).
- **Node.js** – CLI scripts that mirror the same topics with stdout logging (`docs/examples/node/*`).

Follow the instructions in each subdirectory README to build artefacts and run the samples.

---

## C API Documentation

The public headers under `src/native/include/helios/` define the C API, intended for native use or alternate language bindings. Highlights:

- **`CXNetwork.h`** – Core network struct and lifecycle functions (`CXNewNetwork`, `CXNetworkAddNodes`, attribute management).
- **Version helpers** – `CXNetworkVersionString()` (native) and the `CXNETWORK_VERSION_*` macros expose the semantic version baked into the library so native consumers can assert compatibility.
- **`CXNeighborStorage.h`** – Abstractions for node adjacency storage (list vs. map) with iterators.
- **`CXIndexManager.h`** – Index recycling pool used internally for node/edge allocation.
- **`CXDictionary.h`, `CXSet.h`, `CXCommons.h`** – Support utilities (hash tables, sets, inline helpers) used across the codebase.

> **Strings in native code**: string attributes are stored as pointers to null-terminated UTF-8 buffers in the underlying linear memory. When you set a string via JS, the pointer is written into the attribute buffer, so C consumers can access the text directly via `char *`.

### Active index helpers

- **Fill APIs**: `CXNetworkWriteActiveNodes` / `CXNetworkWriteActiveEdges` write active indices into caller-provided `uint32_t*` buffers. If `capacity` is insufficient, the required size is returned and no writes occur.
- **Edge copies**: `CXNetworkWriteActiveEdgeSegments(network, positions, componentsPerNode, dst, dstCapacityEdges)` copies two position vectors per active edge (`componentsPerNode` floats each, typically vec4) directly into `dst`. `CXNetworkWriteEdgeNodeAttributesInOrder` mirrors this for arbitrary node attributes and follows the stored dense edge order.
- **Lifetime**: Buffers you supply remain under your control; the functions are read-only over the network state and will not resize your allocations.

### Dense attribute buffers

Advanced usage for render-ready dense buffers (packing, ordering, versioning / legacy dirty flags, valid ranges) lives in [`docs/visualization-buffers.md`](docs/visualization-buffers.md).

---

## JavaScript API Documentation

The JavaScript bindings (`src/js/HeliosNetwork.js` and the package entry point) now include exhaustive JSDoc comments detailing every public class, method, parameter, and return value alongside error conditions. HTML documentation is generated with the [`better-docs`](https://github.com/SoftwareBrothers/better-docs) template and the `jsdoc-typeof-plugin` for improved type inference.

```bash
npm run docs:js
```

The output is written to `docs/api/index.html`. Customize the layout or scope by editing `docs/jsdoc.json`.

---

## Generating Docs

### JSDoc (JavaScript)

1. Install dependencies (`npm install`) – this repo already includes `jsdoc`, `better-docs`, and supporting plugins.
2. Run `npm run docs:js`.
3. Open `docs/api/index.html` in your browser.

### Doxygen (C)

1. Install Doxygen (e.g., `brew install doxygen`).
2. Run `doxygen docs/doxygen.conf`.
3. Open `docs/c/html/index.html`.

Both doc commands are documented in the respective config files. You can add npm scripts (`npm run docs:js`, `npm run docs:c`) to automate them if desired.

---

## Contributing

See `docs/CONTRIBUTING.md` for the full development and release guide, including testing expectations, version bump workflow, and publishing steps for npm, vcpkg, and Conan. Bug reports and feature requests are welcome via GitHub issues!

---

© 2024–2025 Filipi Nascimento Silva. Licensed under MIT.
## Packaging

Native packaging overlays for vcpkg and Conan live under `packaging/`, backed by the root `CMakeLists.txt`. See `docs/packaging.md` for build commands, overlay usage, and guidance on upstreaming new ports/recipes.
