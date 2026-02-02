# Helios Network Python Docstrings

Below is a summary of the main classes and methods exposed in the Python API.
This complements the inline docstrings and `python/docs/API.md`.

## Network

`Network(directed=False, node_capacity=None, edge_capacity=None)`

- Creates a network wrapper around the native core.
- Properties:
- `nodes`, `edges`: active selectors
- `network`, `graph`, `attributes`: network-scope selectors
 - `__getitem__(name)` / `__setitem__(name, value)`: network-scope shorthand

### Common methods

- `node_count() -> int`
- `edge_count() -> int`
- `add_nodes(count) -> list[int]`
- `remove_nodes(indices) -> bool`
- `add_edges([(from, to), ...]) -> list[int]`
- `remove_edges(indices) -> bool`
- `node_indices() -> list[int]`
- `edge_indices() -> list[int]`
- `edges_with_indices() -> list[(edge_id, (source, target))]`

### Attribute methods

- `define_attribute(scope, name, type, dimension=1) -> bool`
 - `nodes.define_attribute(name, type, dimension=1) -> bool`
 - `edges.define_attribute(name, type, dimension=1) -> bool`
 - `network.define_attribute(name, type, dimension=1) -> bool`
- `attribute_info(scope, name) -> dict`
- `set_attribute_value(scope, name, index, value) -> bool`
- `get_attribute_value(scope, name, index)`

### Categorical helpers

- `categorize_attribute(scope, name, sort_order=None, missing_label=None) -> bool`
- `decategorize_attribute(scope, name, missing_label=None) -> bool`
- `get_category_dictionary(scope, name) -> dict[label, id]`
- `set_category_dictionary(scope, name, mapping_or_pairs, remap_existing=True) -> bool`

### Serialization

- `save_xnet(path)`
- `save_bxnet(path)`
- `save_zxnet(path, compression=6)`

## NodeCollection / EdgeCollection

Selectors for active nodes/edges. Support iteration and vectorized assignment.

### Common behaviors

- `collection[id]` -> `NodeView` / `EdgeView`
- `collection[[ids]]` -> selector for multiple items
- `collection["attr"]` -> list of values
- `collection["attr"] = value` -> broadcast or per-item assignment

### Convenience

- `nodes.define_attribute(name, type, dimension=1)`
- `edges.define_attribute(name, type, dimension=1)`
- `network.define_attribute(name, type, dimension=1)`

## NodeView / EdgeView / NetworkView

Simple wrappers around a single item (node, edge, network).

- `node["attr"]` / `edge["attr"]` / `network["attr"]`
- `edge.endpoints -> (source, target)`
