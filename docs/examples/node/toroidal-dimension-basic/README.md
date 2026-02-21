# Node: Toroidal Dimensions (One-Shot)

Builds 1D/2D/3D/4D toroidal regular networks and measures:

- local multiscale dimension `d_i(r)` at node `0`
- global multiscale dimension `D(r)` on a sampled node set
- a summary table with maximum local/global dimensions per topology

## Run

```bash
node docs/examples/node/toroidal-dimension-basic/main.mjs
```

Quick mode (smaller networks):

```bash
node docs/examples/node/toroidal-dimension-basic/main.mjs --quick
```

Large mode (includes 20k-50k node tori):

```bash
node docs/examples/node/toroidal-dimension-basic/main.mjs --large
```
