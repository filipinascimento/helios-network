# Node: Roundtrip Validation

This example walks through building a small network, mutating it, and verifying that the `.bxnet`, `.zxnet`, and `.xnet` formats faithfully preserve integer, float, and string attributes.

## Run

```bash
node docs/examples/node/roundtrip-validation/main.mjs
```

## Highlights

- Builds a directed network, adds mixed-type attributes, and tweaks values after creation.
- Demonstrates node and edge deletions before persisting snapshots.
- Saves the graph to all supported formats and reloads each variant.
- Verifies that counts, numeric values, and string attributes match the original network.
