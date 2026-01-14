# Steppable Sessions (Long-Running Computations)

Helios runs heavy algorithms (e.g. community detection, future centrality measures) inside WASM on the main JS thread by default. To avoid blocking the event loop for long stretches, the library supports **steppable sessions**: you advance an algorithm in small chunks and optionally yield between chunks.

This document explains:

- How to *use* steppable sessions (progress + cooperative yielding).
- How to *implement* new steppable algorithms (native + JS glue).
- How version-based cancellation keeps sessions correct when the graph changes mid-run.

## Key ideas

### 1) Step-by-step execution

A steppable session exposes:

- `step(...)` - run a bounded amount of work, return `{ phase, progress... }`
- `getProgress()` - read current progress without advancing
- `run(...)` - repeatedly `step(...)` and yield between chunks (Promise-based)
- `runWorker(...)` - run in a dedicated worker (when supported)
- `finalize(...)` - write results into attributes / buffers (when done)
- `dispose()` - free native session state

For completion checks, use `phase` (or an algorithm-specific helper like `session.isComplete()` when provided). A session being complete is separate from being finalized (writing results); `finalize(...)` performs that write, and some sessions may expose `isFinalized()` to reflect that the write has happened.

### 2) Version-based cancellation (safety)

The network is mutable. If the topology (nodes/edges) or relevant attributes change while an algorithm is mid-run, the session can become invalid.

To prevent producing out-of-sync results, `WasmSteppableSession` supports **automatic cancellation**:

- A session captures a baseline of selected versions at creation time.
- Before `step()` / `getProgress()`, it compares current versions against the baseline.
- If anything changed, it disposes the session and throws `Session canceled: ...`.

Each algorithm defines its own cancellation policy internally:

- **Leiden**: always cancels on topology changes; additionally cancels on the weight edge attribute version when running weighted modularity.
- **Betweenness (future)**: would likely cancel on topology changes.
- **Assortativity (future)**: would cancel on topology + the attribute used in the calculation.

## Using a steppable session (example: Leiden)

```js
const session = network.createLeidenSession({ edgeWeightAttribute: 'w', seed: 1 });

// Option A: manual stepping
for (;;) {
  const { phase, progressCurrent, progressTotal } = session.step({ timeoutMs: 4, chunkBudget: 20000 });
  const progress01 = progressTotal ? progressCurrent / progressTotal : 0;
  if (phase === 5) break; // done
  if (phase === 6) throw new Error('failed');
  // UI can update here before next step
}
const result = session.finalize({ outNodeCommunityAttribute: 'community' });
session.dispose();

// Option B: run loop with cooperative yielding
await session.run({
  stepOptions: { timeoutMs: 4, chunkBudget: 20000 },
  onProgress: ({ progressCurrent, progressTotal }) => {
    const progress01 = progressTotal ? progressCurrent / progressTotal : 0;
    /* update UI */
  },
  yieldMs: 0, // yield to event loop between chunks
});
```

Notes:

- `timeoutMs` and `chunkBudget` control how much work happens per call; smaller values yield control more frequently.
- If the graph topology/attribute versions change while the session is running, `step()`/`run()` will throw a cancellation error.
- Always call `dispose()` when you’re done (or use `try/finally`).

## Running in a worker

Supported sessions can run in a dedicated worker via `runWorker(...)`. This:

- creates a fresh WASM module instance in the worker,
- snapshots only the required topology/attributes from the main-thread network,
- runs the native session in the worker (step-by-step, posting progress events),
- applies the result back onto the original network (main thread).

Example:

```js
const session = network.createLeidenSession({ edgeWeightAttribute: 'w', seed: 1 });
await session.runWorker({
  stepOptions: { timeoutMs: 4, chunkBudget: 20000 },
  onProgress: ({ progressCurrent, progressTotal }) => {
    const progress01 = progressTotal ? progressCurrent / progressTotal : 0;
    /* update UI */
  },
});
```

Notes:

- `runWorker` still respects the session cancellation policy: if the tracked topology/attribute versions change on the main thread, the worker run is canceled.
- The session object is disposed after `runWorker` completes (use `create*Session(...)` again if you want to run another pass).
- Worker execution uses data snapshots (no `SharedArrayBuffer`); only the required inputs for the session are copied.
- In the browser, `runWorker` requires serving the ES module build from a real URL so the worker module can be loaded; it will not work if the library is loaded from a `data:`/`blob:` URL.

## Implementing a new steppable algorithm

There are two parts: a native session API (C) and a JS wrapper that plugs into `WasmSteppableSession`.

### A) Native side (C / WASM)

The pattern is:

1. Define an opaque `Session` struct holding all working memory/state.
2. Export a small C API:
   - `SessionCreate(...) -> SessionRef`
   - `SessionStep(session, budget) -> phase`
   - `SessionGetProgress(session, ...)` (write a small POD progress struct into out-pointers)
   - `SessionFinalize(session, ...) -> ok`
   - `SessionDestroy(session)`
3. Add the `_Session*` functions to `scripts/exported-functions.txt`, then rebuild WASM.

Progress should be **small and fixed-size** (numbers written into out-pointers), so JS can read it via `HEAP*` without creating large copies.

### B) JS side (plumbing + cancellation policy)

`WasmSteppableSession` (in `src/js/sessions/WasmSteppableSession.js`) is the reusable engine:

- owns the native pointer
- allocates a small scratch buffer once (optional)
- provides `step({ budget, timeoutMs, chunkBudget })`
- provides `run({ stepOptions, onProgress, yieldMs | yield, signal, maxIterations })`
- enforces version-based cancellation via `handlers.cancelOn`

To add a new session type, create a wrapper class that *composes* a `WasmSteppableSession` and supplies the handlers:

- `destroy` - native destroy function
- `step` - native step function
- `getProgress` - reads native progress into JS values
- `isDonePhase` / `isFailedPhase` - phase decoding
- `cancelOn` - the algorithm’s private cancellation policy (topology + relevant attributes)
- `workerSpec` - how to snapshot required inputs + apply results back when running in a worker

The important rule is: **cancellation policy is defined by the session type**, not by the user at creation time.

## Choosing what to track for cancellation

Use versioning to track the things your algorithm depends on:

- Topology-dependent algorithms: track `topology: 'both'` unless truly node-only or edge-only.
- Attribute-dependent algorithms: track the attribute versions you read during the computation.
  - If an algorithm reads an attribute buffer directly (dense/sparse), it must ensure a version bump happens when that buffer changes (manual bump APIs exist for direct buffer writes).

See `docs/versioning.md` for details on how versions behave and how to bump them when needed.

## WASM buffer view safety

WASM memory can grow; `TypedArray` views can become invalid after allocation. As a result:

- Don’t cache `TypedArray` views across session steps if any step might allocate.
- Treat `step()` / `run()` as allocation-prone.
- Allocate first, view second; use `withBufferAccess(...)` for safe view lifetimes when needed.
