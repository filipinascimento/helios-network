# Layout Delegation Support Plan (Helios Network v2)

**Date:** 2026-01-28

## Status update (implemented)
- Native interpolation helper: `CXAttributeInterpolateFloatBuffer(...)`
- WASM export and JS wrapper: `HeliosNetwork.interpolateNodeAttribute(...)`
- Network-backed interpolation flow in helios-web-next (layout target captured into WASM, positions interpolated in-place, dense buffers dirtied without emitting events by default)

## Status update (still needed)
- Explicit position ownership flags for external vs network control
- Event coverage audit for delegation workflows + payload documentation
- Tests covering event emissions and ownership transitions
- Documentation updates for ownership + interpolation backends
- Clarify best-practice for passing WASM-backed interpolation targets to avoid per-step copies

## Goals
- Provide the minimal network-side hooks needed for position delegation.
- Ensure network emits change events so delegates can keep consistent state.
- Avoid duplicating position buffers unless explicitly requested.

---

## 1) Event Coverage Audit
**Objective:** Confirm the event system covers what delegates need.

### Must-have events
- Node added / removed
- Edge added / removed
- Attribute created / removed / changed (esp. positions)

### Tasks
- Audit current event emission points in native + JS glue.
- Add missing events where required.
- Document payload formats for use by external delegates.
- Add a safe query helper (or document best practice) so renderers can skip dense updates when an attribute is not defined.

---

## 2) Position Ownership / Sync Control
**Objective:** Allow external owners to manage positions without forcing network to keep them updated in real time.

### Proposed API (conceptual)
- `positionsOwner: "network" | "external"` (option on network init or setter)
- `syncPositionsFromExternal()`
- `syncPositionsToExternal()` (optional helper)

### Behavior
- When `external`, network stores positions attribute but does not update it unless explicitly synced.
- Layout or delegate can call sync on demand.

---

## 3) WASM View Safety
**Objective:** Follow non‑negotiables about WASM memory growth.

### Guidance
- Allocate first, view second.
- Prefer `withBufferAccess(...)` to avoid stale views.
- Avoid JS-side duplication of large buffers.
- When interpolating, prefer WASM-backed targets so `interpolateNodeAttribute(...)` can reuse the heap view without copying.

---

## 4) Tests
- Event emission tests for add/remove nodes/edges.
- Attribute-change event tests.
- Position ownership toggle tests.
- Interpolation tests for large graphs (performance + convergence bounds).

---

## 5) Docs
- Update docs to describe external position ownership and sync workflow.
- Document interpolation backends (CPU overrides vs network C core).
- Note safety rules for WASM buffers.

---

## Phased Delivery
**Phase 1:** Event audit + missing events.

**Phase 2:** Position ownership option + sync APIs.

**Phase 3:** Documentation and regression tests.
