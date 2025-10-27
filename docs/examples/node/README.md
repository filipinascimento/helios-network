# Node.js Example

This script demonstrates how to use the Helios Network ES module directly inside Node.js.

## Prerequisites

- Dependencies installed via `npm install`.
- Artefacts generated with `npm run build` and `npm run build:wasm` so the ESM bundle under `dist/` is up to date.
- Node.js 18 or newer (the package targets Node's native ES module support).

## Running

Execute the script from the project root:

```bash
node docs/examples/node/run.mjs
```

The script logs each operation to stdout while it creates nodes, edges, and attributes, mirroring what a typical analytics workload might do.
