# Node: Saving & Loading Networks

This example focuses on serialization helpers so you can persist networks to disk or in-memory artefacts and hydrate them back.

## Run

```bash
node docs/examples/node/saving-loading/main.mjs
```

## Highlights

- Writes a `.bxnet` payload to memory and restores it with `HeliosNetwork.fromBXNet`.
- Saves a `.zxnet` archive to the OS temporary directory and reloads it via `fromZXNet`.
- Demonstrates alternative outputs such as Base64 strings for transport over text channels.
- Ensures temporary files are removed and WASM resources are disposed.
