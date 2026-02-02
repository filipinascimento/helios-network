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

### Edge indices

```python
for edge_id, (source, target) in network.edges.with_indices():
    print(edge_id, source, target)
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

## Vector attributes

Attributes with dimension > 1 accept vector assignments:

```python
network.define_attribute(AttributeScope.Node, "i3", AttributeType.Integer, 3)
network.nodes["i3"] = [1, 2, 3]  # broadcast to all nodes
network.nodes[[0, 1]]["i3"] = [[4, 5, 6], [7, 8, 9]]
```
