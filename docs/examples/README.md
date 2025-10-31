# Examples

This directory collects runnable examples for both the browser and Node.js environments. Each platform contains topic-specific subfolders so you can jump straight to the workflow you want to explore:

- `browser/` – in-browser demos for basic usage, attributes, iteration, graph mutations, and saving/loading.
- `node/` – Node 18+ scripts covering the same topics with stdout logging.

Before running the examples ensure dependencies are installed (`npm install`) and the WASM artefacts are up to date (`npm run build && npm run build:wasm`), which produces the ESM bundle under `dist/`.

Refer to the README inside each platform directory for instructions plus direct links to each example.
