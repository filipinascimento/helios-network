# Node: Toroidal Dimensions (Steppable Session)

Builds 1D/2D/3D/4D toroidal regular networks and computes dimensions with
`createDimensionSession()` in incremental steps.

This example shows:

- progress-aware stepping (`step` + `getProgress`)
- `finalize()` output curves
- writing node attributes with highest local dimension and per-level dimensions

## Run

```bash
node docs/examples/node/toroidal-dimension-session/main.mjs
```

Quick mode:

```bash
node docs/examples/node/toroidal-dimension-session/main.mjs --quick
```

Large mode (includes 20k-50k node tori):

```bash
node docs/examples/node/toroidal-dimension-session/main.mjs --large
```
