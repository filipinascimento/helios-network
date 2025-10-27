# Browser Example

This example shows how to load the Helios Network ESM build directly in a browser without any additional bundling.

## Prerequisites

- Dependencies installed via `npm install`.
- Artefacts generated with `npm run build` and `npm run build:wasm` so the `dist/` directory contains the latest bundle.

## Running

1. From the project root start a static file server (any tool works). A built-in option is:
   ```bash
   python3 -m http.server 4173
   ```
2. Navigate to <http://localhost:4173/docs/examples/browser/> in your browser.
3. Open the developer console to see the log output from the script as it creates nodes, edges, and attributes.

The page (`index.html`) loads `main.js`, which imports `helios-network.js` from the local `dist/` bundle and exercises a minimal workflow.
