# Helios Network (Python)

Python bindings for the Helios Network C core.

## Quick start

```python
from helios_network import AttributeScope, AttributeType, Network

network = Network(directed=False)
node_ids = network.add_nodes(3)
network.add_edges([(node_ids[0], node_ids[1])])

network.define_attribute(AttributeScope.Node, "weight", AttributeType.Double, 1)
network.nodes["weight"] = 1.0

# Auto-define attributes on assignment
network.nodes["score"] = 2.5

network.define_attribute(AttributeScope.Network, "title", AttributeType.String, 1)
network.network["title"] = "demo"
print(network.graph["title"])
network["title"] = "demo2"

# Node access
print(network.nodes[node_ids[0]]["weight"])
network.nodes[[node_ids[0], node_ids[1]]]["weight"] = [2.0, 3.0]

for node in network.nodes:
    node["weight"] = 4.0
```

## Selectors

- `network.nodes[id]` returns a node view
- `network.nodes[[id1, id2]]` returns a selector for active nodes
- `network.nodes["attr"]` returns a list of attribute values for active nodes
- `network.nodes["attr"] = value` assigns a constant to all active nodes
- `network.nodes[[id1, id2]]["attr"] = [v1, v2]` assigns per-node values

Edge access mirrors node access via `network.edges`, with `network.edges.pairs()` yielding `(source, target)` tuples.

For large generated graphs, avoid materializing Python `(source, target)` tuples:

```python
from array import array

network.add_nodes(1_000_000)
network.add_edges_from_arrays(
    array("I", sources),
    array("I", targets),
)
```

## Network Generators

The Python package exposes native generators as module-level constructors:

```python
from helios_network import generate_watts_strogatz

network = generate_watts_strogatz(
    5_000,
    neighbor_level=2,
    rewiring_probability=0.01,
    seed=1,
)
```

Available generators include `generate_stochastic_block_model`,
`generate_barabasi_albert`, `generate_watts_strogatz`,
`generate_random_geometric`, `generate_waxman`,
`generate_configuration_model`, and `generate_lattice_2d`.

### Edge indices

```python
for edge_id, (source, target) in network.edges.with_indices():
    print(edge_id, source, target)
```

## Measurements and Community Detection

The Python wrapper exposes the native measurement APIs:

```python
degree = network.measure_degree(direction="both")
components = network.measure_connected_components(mode="weak")
leiden = network.leiden_modularity(
    resolution=1.0,
    seed=42,
    out_node_community_attribute="community",
)
```

`leiden_modularity(...)` is an alias for `measure_leiden_modularity(...)`. It
writes an unsigned-integer node attribute and returns `community_count`,
`modularity`, and `values_by_node`.

To label only major components for filtering or visualization:

```python
major = network.label_major_components(
    mode="weak",
    max_components=3,
    min_size=100,
    out_node_component_attribute="major_component",
)
```

Selected components receive rank labels `1..N` by decreasing size. Other nodes
receive `0` by default.

The Leiden comparison harness in `research/benchmarks/leiden_compare/` uses a
conda environment file:

```bash
conda env create -f research/benchmarks/leiden_compare/environment.yml
conda run -n helios-leiden-compare python -m pip install -e python --no-build-isolation
conda run -n helios-leiden-compare python research/benchmarks/leiden_compare/compare_leiden.py
```

## Installation

Standard install:

```bash
python -m pip install ./python --upgrade
```

Editable install (recommended during development):

```bash
python -m pip install -e python --no-build-isolation
```

`mesonpy` editable builds require `--no-build-isolation`.

If editable install fails, install build tools in the active environment:

```bash
python -m pip install meson ninja meson-python
python -m pip install -e python --no-build-isolation
```

## Optional conversions

NetworkX and igraph conversions are optional. Install when needed:

```bash
python -m pip install networkx
python -m pip install igraph
```

## UMAP export

Install the optional UMAP stack when you want a Helios-ready fuzzy graph or
kNN graph export:

```bash
python -m pip install ./python[umap]
```

```python
from helios_network import HeliosUMAP

umap_export = HeliosUMAP(
    n_neighbors=15,
    min_dist=0.1,
    build_knn_network=True,
)

network = umap_export.fit_network(X)
network.save_bxnet("embedding.bxnet")

# Optional directed kNN export
umap_export.knn_network_.save_zxnet("embedding-knn.zxnet")
```

If you want only the UMAP graph construction output for `helios-web-next`
to lay out from scratch, use the graph-only export path instead:

```python
graph_network = umap_export.fit_graph_network(X)
graph_network.save_zxnet("umap-graph.zxnet")
```

The fit graph export writes:

- node attributes: `umap_embedding`, `_helios_visuals_position`, `umap_mass`
- edge attribute: `umap_weight`
- network attributes that let `helios-web-next` auto-enable UMAP force mode on import

The graph-only export writes the same UMAP edge weights and graph metadata, but
omits `umap_embedding` and `_helios_visuals_position` so `helios-web-next` can
start its own realtime UMAP-like layout from scratch.

## Examples

From `python/`:

```bash
python examples/basic_usage.py
python examples/toroidal_dimensions_table.py
python examples/toroidal_dimensions_plot.py --skip-plot
python examples/toroidal_dimensions_plot.py
python examples/toroidal_dimensions_plot.py --large --skip-plot
python examples/generate_umap_example_networks.py --sizes 200 2000 20000
```

`toroidal_dimensions_plot.py` uses `matplotlib` for plotting local dimension curves. Install it with:

```bash
python -m pip install matplotlib
```

## Vector attributes

Attributes with dimension > 1 accept vector assignments:

```python
network.define_attribute(AttributeScope.Node, "i3", AttributeType.Integer, 3)
network.nodes["i3"] = [1, 2, 3]  # broadcast to all nodes
network.nodes[[0, 1]]["i3"] = [[4, 5, 6], [7, 8, 9]]
```
