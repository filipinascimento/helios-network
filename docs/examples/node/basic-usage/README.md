# Node: Basic Usage & Initialization

This script bootstraps a small Helios Network instance, adds a few nodes and edges, and inspects the immediate topology.

## Run

```bash
node docs/examples/node/basic-usage/main.mjs
```

## Highlights

- Creates a directed network with two seed nodes.
- Adds additional nodes and edges to demonstrate adjacency growth.
- Fetches the out-neighbours of a node to illustrate traversal helpers.
- Cleans up via `dispose()` to release the underlying WASM resources.
