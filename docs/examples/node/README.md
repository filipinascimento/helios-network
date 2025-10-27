# Node.js Example

This script demonstrates how to use the Helios Network ES module directly inside Node.js.

## Prerequisites

- Dependencies installed via `npm install`.
- Artefacts generated with `npm run build` and `npm run build:wasm` so the ESM bundle under `dist/` is up to date.
- Node.js 18 or newer (the package targets Node's native ES module support).

## Running

Execute the scripts from the project root:

```bash
node docs/examples/node/run.mjs
node docs/examples/node/attributes.mjs
```

Each script logs its operations to stdout: `run.mjs` focuses on basic mutations, while `attributes.mjs` exercises every attribute scope and type (including multi-dimensional and JavaScript-backed payloads).
