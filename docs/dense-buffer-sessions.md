# Dense Buffer Sessions & Views (TL;DR)

- `updateDense*` **only repacks** now (and may grow WASM memory). They no longer return views.
- Fetch views with the new getters:
  - `getDenseNodeAttributeView(name)`, `getDenseEdgeAttributeView(name)`
  - `getDenseNodeIndexView()`, `getDenseEdgeIndexView()`
  - Views are already correctly typed (`Float32Array`, `Uint32Array`, etc.).
- Batch helper: `updateAndGetDenseBufferViews(requests)` runs all needed updates first, then returns typed views in one go. Use name `"index"` to request index buffers.
- Use guards to keep views stable while you write/read (dense **and** sparse):
  - `withBufferAccess(fn)` or `startBufferAccess()` / `endBufferAccess()`
  - Allocation-prone calls inside a buffer access block throw immediately, protecting WASM-backed views from heap growth.
- Batch-safe access helper:
  ```js
  net.updateDenseNodeAttributeBuffer('position');
  net.updateDenseEdgeAttributeBuffer('capacity');
  net.updateDenseNodeIndexBuffer();

  net.withDenseBufferViews([['node', 'position'], ['edge', 'capacity'], ['node', 'index']], ({ node, edge }) => {
    const positions = node.position.view; // Float32Array
    const capacities = edge.capacity.view; // Float32Array
    const nodeIds = node.index.view; // Uint32Array
    const sparsePos = net.getNodeAttributeBuffer('position').view; // also safe here
    // ...upload to GPU...
  });
  ```
- Skip cached/peek APIs: dense caches were removed; always `update*` then `getDense*View`.

### Color-encoded dense buffers (GPU-friendly picking ids)

- Register encoded buffers that pack `(value + 1)` into 4 components (reserving 0 as “no value”):
  - Nodes: `defineDenseColorEncodedNodeAttribute(sourceName, encodedName, { format })`
  - Edges: `defineDenseColorEncodedEdgeAttribute(sourceName, encodedName, { format })`
  - `sourceName` can be an integer attribute **or** the literal string `"$index"` to encode ids without an attribute.
- Supported formats: `Uint8x4` (default) and `Uint32x4` (choose with `DenseColorEncodingFormat.Uint32x4` or `'u32x4'`). Views are `Uint8Array` / `Uint32Array`, sized `count * 4` with `dimension: 4`.
- Dirty tracking mirrors dense attributes: source attribute changes (`markDense*AttributeDirty`), topology edits, or dense order updates will mark the encoded buffer dirty. Call `updateDenseColorEncoded*Attribute(name)` to repack; bind via `getDenseColorEncoded*AttributeView(name)` inside `withBufferAccess`.
- Ordering matches dense orders set via `setDense*Order`; buffers are stable (no reallocation unless grown) for direct WebGL/WebGPU binding.
- Use when you need GPU-ready picking/attribute ids without per-frame JS packing (e.g., render passes that sample RGBA8/32 textures or SSBOs).
### Safe pattern
1) Call `updateDense*` for everything you need (this is the only step that can allocate).  
2) Call `withBufferAccess` and read/write via `getDense*View` (or `withDenseBufferViews`).  
3) Any attempt to allocate while inside the block will throw, preventing detached views.
