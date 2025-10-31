# Node: Working with Attributes

This example registers every attribute scope (node, edge, network) and showcases primitive as well as JavaScript-managed payloads.

## Run

```bash
node docs/examples/node/attributes/main.mjs
```

## Highlights

- Registers scalar node and edge attributes with `AttributeType.Float` and `AttributeType.Boolean`.
- Demonstrates a JavaScript-backed network attribute for arbitrary metadata.
- Writes to the underlying WASM-backed views and reads the results back.
- Emphasises that attribute buffers must be accessed via the typed views returned by `get*AttributeBuffer`.
