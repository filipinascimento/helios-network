# Helios Network Python API

## Overview

The Python bindings expose a high-level `Network` class with convenient node/edge/network selectors. Attributes are backed by the native Helios core and can be created explicitly or implicitly.

Key concepts:
- `network.nodes`, `network.edges`: active-only selectors that allow iteration and batch attribute assignment.
- `network.network` / `network.graph`: network-scope attribute access (single record).
- `network["attr"]`: shorthand for network-scope attribute access.

## Creating Networks

```python
from helios_network import Network

network = Network(directed=False)
```

### Constructor

`Network(directed: bool = False, node_capacity: int | None = None, edge_capacity: int | None = None)`

- `directed`: If `True`, edges are directed.
- `node_capacity`, `edge_capacity`: Optional initial capacities.

## Nodes and Edges

### Iteration

```python
for node in network.nodes:
    print(node.id)

for edge in network.edges:
    print(edge.id, edge.endpoints)
```

### Edge pairs

```python
for source, target in network.edges.pairs():
    print(source, target)
```

### Edge indices + endpoints

```python
for edge_id, (source, target) in network.edges.with_indices():
    print(edge_id, source, target)
```

## Attributes

### Auto-definition

Assigning to an unknown attribute name will create it automatically based on the assigned value.

```python
network.nodes["weight"] = 1.0          # creates Double attribute, dimension 1
network.nodes["i3"] = [1, 2, 3]         # creates Integer attribute, dimension 3
network.edges["weight"] = 0.5          # creates Double attribute on edges
network["title"] = "demo"              # creates String attribute on network scope
```

### Explicit definition

```python
from helios_network import AttributeScope, AttributeType

network.define_attribute(AttributeScope.Node, "score", AttributeType.Double, 1)
```

### Collection helpers

```python
network.nodes.define_attribute("score", float)
network.edges.define_attribute("weight", AttributeType.Double)
network.network.define_attribute("title", str)
```

### Attribute access

```python
# Single node
network.nodes[node_id]["score"] = 1.5
value = network.nodes[node_id]["score"]

# Selector
network.nodes[[id1, id2]]["score"] = [1.0, 2.0]
values = network.nodes[[id1, id2]]["score"]

# All nodes
network.nodes["score"] = 2.0
```

### Vector attributes (dimension > 1)

```python
network.nodes["i3"] = [1, 2, 3]  # broadcast
network.nodes[[id1, id2]]["i3"] = [[1, 2, 3], [4, 5, 6]]
```

NumPy arrays with shape `(dim,)` or `(count, dim)` are supported when NumPy is installed.

## Categorical attributes

```python
network.nodes["label"] = ["a", "b", "a"]
network.categorize_attribute(AttributeScope.Node, "label")

mapping = network.get_category_dictionary(AttributeScope.Node, "label")
# mapping: {"a": 0, "b": 1, ...}

network.set_category_dictionary(AttributeScope.Node, "label", {"x": 10, "y": 11}, remap_existing=True)
network.decategorize_attribute(AttributeScope.Node, "label")
```

## Serialization

```python
network.save_xnet("graph.xnet")
network.save_bxnet("graph.bxnet")
network.save_zxnet("graph.zxnet")

loaded = read_xnet("graph.xnet")
```

## Conversions

```python
from helios_network import to_networkx, from_networkx

nx_graph = to_networkx(network)
network2 = from_networkx(nx_graph)
```

`to_igraph` / `from_igraph` are available when `igraph` is installed.
