# Node.js Examples

Executable scripts that showcase Helios Network features from Node 18+. Run them from the repository root after installing dependencies (`npm install`) and building the WASM bundle (`npm run build && npm run build:wasm`).

> **Note:** If `dist/helios-network.js` is missing, the scripts automatically fall back to `src/helios-network.js` and emit a console warning. Run `npm run build` to regenerate the distributable bundle when necessary.

## Catalogue

- `basic-usage/` – Bootstraps a network, adds nodes/edges, and inspects neighbours.
- `attributes/` – Demonstrates numeric and JavaScript-backed attribute stores.
- `iteration/` – Works with selectors, degrees, and traversal helpers.
- `modifying-graphs/` – Performs removals and optional compaction to recycle capacity.
- `saving-loading/` – Serializes to `.bxnet`/`.zxnet` and loads the artefacts back.

From the project root execute any script via:

```bash
node docs/examples/node/<example>/main.mjs
```

Each script logs its progress to stdout so you can follow along with the underlying mutations and counts.
