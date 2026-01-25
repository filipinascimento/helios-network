# Stable Sort Options (Future)

## Context
The current array sorting uses an introsort variant (quicksort + heapsort fallback + insertion cutoff). It is fast and in-place but not stable. If we need stability, we must either accept extra memory or additional passes.

## Goal
Introduce a stable sorting option for CX basic arrays that preserves relative order of equal keys.

## Constraints
- C core (C17/C23) with WASM build via Emscripten.
- Avoid large JS-side copies; keep operations in C/WASM.
- Prefer predictable memory usage; avoid unbounded realloc during sorting.

## Options

### Option A: Stable mergesort (recommended, close to current performance)
- Implement a bottom-up mergesort for each CX<Type>Array.
- Requires an auxiliary buffer of the same element count (O(n)).
- Stable by construction.
- Performance: O(n log n) worst-case with tight inner loops; for large arrays it is typically within a small constant factor of introsort while being stable.

### Option B: Index-stable sort + permute (memory-light for values)
- Build an index array [0..n-1].
- Stable sort indices based on value (and tie on index).
- Apply permutation to values (and any paired arrays).
- Requires O(n) indices plus permute scratch; still stable.
- Good when paired arrays already exist (e.g., sorting values with indices).

### Option C: Stable timsort-like (advanced, best-case faster)
- Detect runs, merge with galloping and adaptive behavior.
- Stable, O(n log n) worst-case, O(n) for nearly sorted data.
- More complex to implement and test in C.

## Comparison Helper
- Use a total order for floats/doubles (NaNs last for ascending, first for descending) to keep stable semantics consistent.
- For stable sorts, define a tie-breaker on original position when values compare equal.

## Proposed API
- Add stable entry points:
  - `CXIntegerArrayStableSort(...)`, `CXFloatArrayStableSort(...)`, etc.
  - Optional order parameter (`CXOrderedAscending` / `CXOrderedDescending`).
- Keep existing `CX*ArraySort` as the fast unstable default.

## Plan (high level)
1. Implement a shared stable mergesort template for CX arrays.
2. Add stable wrappers per type, mirroring the existing sort API shape.
3. Extend tests to validate stability (equal keys preserve original order) and NaN ordering for floats/doubles.
4. Benchmark vs current introsort to document tradeoffs.

## Testing Notes
- Add arrays with repeated values and verify that relative order is unchanged.
- For floats/doubles, include NaN values and confirm ordering is consistent with the current total-order rules.
- Exercise both ascending and descending order.
