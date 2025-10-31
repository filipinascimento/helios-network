# Node: Iterating Through the Graph

This script focuses on traversal helpers—selectors, degrees, and neighbour queries—so you can see how Helios exposes graph iteration primitives.

## Run

```bash
node docs/examples/node/iteration/main.mjs
```

## Highlights

- Creates a node selector targeted at a subset of nodes and iterates over it.
- Computes out-degrees via `selector.degree({ mode: 'out' })`.
- Uses `getOutNeighbors` to inspect adjacency for each selected node.
- Demonstrates how to dispose selector instances once finished.
