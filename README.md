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
4. [Building](#building)
5. [Testing](#testing)
6. [JavaScript Usage](#javascript-usage)
7. [C API Documentation](#c-api-documentation)
8. [JavaScript API Documentation](#javascript-api-documentation)
9. [Generating Docs](#generating-docs)
10. [Contributing](#contributing)

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

## Building

### JavaScript Bundles

The `build` script produces both ESM and UMD bundles under `dist/` using Vite and Rollup. Rollup’s native binary is skipped for compatibility via an environment flag.

```bash
npm run build
```

Expected output (abridged):

```
vite v6.x building for production...
dist/helios-network.js         # ESM bundle
dist/helios-network.umd.cjs    # UMD bundle
```

### WebAssembly Core

`build:wasm` invokes Meson/Ninja to recompile the C sources to WebAssembly via `emcc`:

```bash
npm run build:wasm
```

This regenerates `compiled/CXNetwork.{mjs,wasm}`. The JS layer imports the `.mjs` wrapper dynamically.

> The script prepends `node_modules/.bin` to `PATH` so local Meson/Ninja symlinks are also detected.

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

Run browser tests separately (requires Playwright & dev server):

```bash
# Optional – only if you need browser coverage
npm run test:browser
# or the CI version
npm run test:browser:ci
```

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

// Selectors
const selector = net.createNodeSelector([newNodes[0], newNodes[1]]);
console.log(selector.toTypedArray());
selector.dispose();

net.dispose();
```

### Key Classes & Functions

- `HeliosNetwork.create(options)` → Promise resolving to a network instance.
- `HeliosNetwork.addNodes(count)` / `addEdges(list)` → return typed-array copies of new indices.
- Attribute helpers (`define*Attribute`, `get*AttributeBuffer`, `set*StringAttribute`, etc.).
- Selectors (`createNodeSelector`, `createEdgeSelector`) and the `Selector` helper API.
- Static export `AttributeType` enumerates supported attribute kinds.

#### Graph-level attributes

- Use `defineNetworkAttribute(name, type, dimension?)` to declare a graph property stored in linear memory.
- Primitive buffers expose index `0` as the graph slot (e.g., `getNetworkAttributeBuffer('weight').view[0] = 42`).
- `setNetworkStringAttribute`/`getNetworkStringAttribute` and JavaScript-managed attributes (`AttributeType.Javascript`) provide convenience accessors.

Full JSDoc comments inside `src/js/HeliosNetwork.js` describe signatures and behaviours. You can generate HTML docs as shown below.

---

## C API Documentation

The public headers under `src/native/include/helios/` define the C API, intended for native use or alternate language bindings. Highlights:

- **`CXNetwork.h`** – Core network struct and lifecycle functions (`CXNewNetwork`, `CXNetworkAddNodes`, attribute management).
- **`CXNeighborStorage.h`** – Abstractions for node adjacency storage (list vs. map) with iterators.
- **`CXIndexManager.h`** – Index recycling pool used internally for node/edge allocation.
- **`CXDictionary.h`, `CXSet.h`, `CXCommons.h`** – Support utilities (hash tables, sets, inline helpers) used across the codebase.

> **Strings in native code**: string attributes are stored as pointers to null-terminated UTF-8 buffers in the underlying linear memory. When you set a string via JS, the pointer is written into the attribute buffer, so C consumers can access the text directly via `char *`.

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

1. Fork & clone the repo.
2. `npm install` and `npm run build:wasm` to set up.
3. Follow the coding style of existing C/JS sources. Prefer short inline comments; rely on JSDoc/Doxygen for public APIs.
4. Run `npm test` before submitting changes.
5. Open a pull request describing your changes and testing.

Bug reports and feature requests are welcome via GitHub issues!

---

© 2024–2025 Helios Network contributors. Licensed under MIT.
