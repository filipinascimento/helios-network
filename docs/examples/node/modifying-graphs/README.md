# Node: Modifying Graph Structure

This script walks through node/edge removals and demonstrates how `compact()` can reclaim capacity after heavy mutations.

## Run

```bash
node docs/examples/node/modifying-graphs/main.mjs
```

## Highlights

- Builds a simple path graph, then selectively removes edges and nodes.
- Keeps attribute views (`score`) in sync after removals.
- Calls `compact()` when the native function is available to reindex active entities.
- Logs the resulting counts so you can verify capacity changes on the fly.
