# Examples

This directory contains minimal end-to-end examples that demonstrate how to consume Helios Network from both the browser and a Node.js runtime. Each example includes runnable code plus step-by-step instructions so you can verify your toolchain quickly.

- `browser/` – loads the ESM bundle inside a plain HTML page.
- `node/` – executes the WASM-backed API from a small Node.js script.

Both examples assume you have already installed dependencies (`npm install`) and built the artefacts (`npm run build` and `npm run build:wasm`), which places the distributable files under `dist/`.

Refer to the README inside each subdirectory for detailed usage.
