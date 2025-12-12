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

### Safe pattern
1) Call `updateDense*` for everything you need (this is the only step that can allocate).  
2) Call `withBufferAccess` and read/write via `getDense*View` (or `withDenseBufferViews`).  
3) Any attempt to allocate while inside the block will throw, preventing detached views.
