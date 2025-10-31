# Browser Examples

Browse-ready demos that exercise Helios Network from an unbundled ESM build. Serve the repository (e.g. `npm run dev` or `python3 -m http.server 4173`) and open <http://localhost:5173/docs/examples/browser/> or the equivalent port for your server.

> **Note:** The scripts prefer the bundled file under `dist/`. During local development the bundle might be missing; in that case the examples automatically fall back to `src/helios-network.js` and log a warning in the console.

## Catalogue

- `basic-usage/` – Initialize a network, add nodes/edges, and inspect immediate neighbours.
- `attributes/` – Register node/edge/network attributes and write values inside the browser.
- `iteration/` – Iterate across selectors, query degrees, and traverse neighbours client-side.
- `modifying-graphs/` – Remove nodes/edges and optionally compact the structure.
- `saving-loading/` – Serialize to `.bxnet`/`.zxnet` formats using in-memory blobs or Base64.

Each folder contains an `index.html` entry point plus a `main.js` module that logs progress to the page and the browser console.
