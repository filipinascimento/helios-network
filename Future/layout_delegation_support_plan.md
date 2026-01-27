# Layout Delegation Support Plan (Helios Network v2)

**Date:** 2026-01-26

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

---

## 4) Tests
- Event emission tests for add/remove nodes/edges.
- Attribute-change event tests.
- Position ownership toggle tests.

---

## 5) Docs
- Update docs to describe external position ownership and sync workflow.
- Note safety rules for WASM buffers.

---

## Phased Delivery
**Phase 1:** Event audit + missing events.

**Phase 2:** Position ownership option + sync APIs.

**Phase 3:** Documentation and regression tests.
