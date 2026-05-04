# Node: Roundtrip Walkthrough (Sequential)

This guide mirrors the automated script in `main.mjs`, but breaks the flow into small chunks you can run one after another from the repository root. Everything below assumes you already installed dependencies (`npm install`) and generated the WASM artefacts (`npm run build:wasm`).

---

## Step 0 — Open a Node REPL

```bash
node
```

The Node 18+ REPL supports top-level `await`, so you can paste each snippet below directly. Keep the session open until the last step.

---

## Step 1 — Load Helios and Node utilities

```js
const { loadHelios } = await import('./docs/examples/node/utils/load-helios.mjs');
const { default: HeliosNetwork, AttributeType } = await loadHelios({ preferSource: true });
// If you installed the package via npm, use:
// const { default: HeliosNetwork, AttributeType } = await import('helios-network');

const fs = await import('node:fs/promises');
const { join } = await import('node:path');
const { tmpdir } = await import('node:os');
```

*Why?* We reuse `HeliosNetwork`, the attribute enum, and a few Node helpers to handle temporary files.

---

## Step 2 — Create a directed network and seed topology

```js
const net = await HeliosNetwork.create({ directed: true });

const nodes = net.addNodes(4); // -> Uint32Array [0,1,2,3]
const edges = net.addEdges([
  { from: nodes[0], to: nodes[1] },
  { from: nodes[1], to: nodes[2] },
  { from: nodes[2], to: nodes[3] },
]);
```

*Why?* This gives us a small chain to exercise attribute storage and persistence.

---

## Step 3 — Populate mixed-type attributes

```js
net
  .nodeAttribute('rank', [1, 2, 3, 4], { type: AttributeType.Integer })
  .nodeAttribute('score', [1.25, 2.5, 3.75, 5.0], { type: AttributeType.Float })
  .nodeAttribute('label', ['Alpha', 'Beta', 'Gamma', 'Delta'])
  .edgeAttribute('weight', [1.5, 2.0, 0.75], { type: AttributeType.Float })
  .edgeAttribute('status', ['active', 'active', 'experimental'])
  .networkAttribute('title', 'Roundtrip Example');
```

*Why?* The chainable writers define missing attributes, infer string fields, and keep attribute versioning managed for normal writes.

---

## Step 4 — Tweak values after creation

```js
net
  .nodeAttribute('label', (current, id) => (id === nodes[1] ? 'Beta (updated)' : current))
  .edgeAttribute('weight', (current, id) => (id === edges[1] ? 2.25 : current))
  .edgeAttribute('status', (current, id) => {
    if (id === edges[1]) return 'updated';
    if (id === edges[2]) return 'deprecated';
    return current;
  });
```

*Why?* Demonstrates that edits to strings and floats propagate to the persisted payloads.

---

## Step 6 — Remove an edge and a node

```js
net.removeEdges([edges[2]]);
net.removeNodes([nodes[3]]);

net.nodeCount; // 3
net.edgeCount; // 2
```

*Why?* Ensures sparse structures (with tombstoned nodes/edges) round-trip correctly.

---

## Step 7 — Save to `.bxnet`, `.zxnet`, and `.xnet`

```js
const tempDir = await fs.mkdtemp(join(tmpdir(), 'helios-roundtrip-doc-'));
const snapshotPaths = {
  bxnet: join(tempDir, 'example.bxnet'),
  zxnet: join(tempDir, 'example.zxnet'),
  xnet: join(tempDir, 'example.xnet'),
};

await net.saveBXNet({ path: snapshotPaths.bxnet });
await net.saveZXNet({ path: snapshotPaths.zxnet, compressionLevel: 5 });
await net.saveXNet({ path: snapshotPaths.xnet });

await Promise.all(
  Object.entries(snapshotPaths).map(async ([key, filePath]) => {
    const { size } = await fs.stat(filePath);
    console.log(`${key}: ${size} bytes`);
  })
);
```

*Why?* This exercises all container formats and shows the generated file sizes.

---

## Step 8 — Reload each snapshot and inspect attributes

```js
const restoredBx = await HeliosNetwork.fromBXNet(snapshotPaths.bxnet);
const restoredZx = await HeliosNetwork.fromZXNet(snapshotPaths.zxnet);
const restoredX = await HeliosNetwork.fromXNet(snapshotPaths.xnet);

const inspect = (label, netRef) => {
  console.log(label, {
    directed: netRef.directed,
    nodeCount: netRef.nodeCount,
    edgeCount: netRef.edgeCount,
    node1Label: netRef.getNodeStringAttribute('label', 1),
    edge1Status: netRef.getEdgeStringAttribute('status', 1),
    title: netRef.getNetworkStringAttribute('title'),
    ...netRef.withBufferAccess(() => ({
      score1: netRef.getNodeAttributeBuffer('score').view[1],
      weight0: netRef.getEdgeAttributeBuffer('weight').view[0],
    })),
  });
};

inspect('BX', restoredBx);
inspect('ZX', restoredZx);
inspect('X', restoredX);
```

*Why?* Quick sanity check that counts and attributes match across all formats.

---

## Step 9 — Dispose restored networks and clean up files

```js
restoredBx.dispose();
restoredZx.dispose();
restoredX.dispose();

await fs.rm(tempDir, { recursive: true, force: true });
```

*Why?* Releases WASM resources and removes the temporary directory.

---

## Step 10 — Dispose the working network and exit

```js
net.dispose();
.exit
```

*Why?* Leaves the REPL clean and ensures the module’s internal memory is reclaimed.

---

You should now have exercised node creation/removal, mixed attribute types, and all three serialization formats entirely from the repository root.
