from __future__ import annotations

from typing import Any, Iterable, Tuple

from . import _core
from ._wrapper import Network


_UNSUPPORTED_EXPORT_TYPES = {
    _core.ATTR_DATA,
    _core.ATTR_JAVASCRIPT,
    _core.ATTR_MULTI_CATEGORY,
}


def _lazy_import_networkx():
    try:
        import networkx as nx  # type: ignore
    except Exception as exc:  # pragma: no cover - import error path
        raise ImportError("networkx is required for this conversion") from exc
    return nx


def _lazy_import_igraph():
    try:
        import igraph as ig  # type: ignore
    except Exception as exc:  # pragma: no cover - import error path
        raise ImportError("igraph is required for this conversion") from exc
    return ig


def _infer_attribute_type(value: Any):
    if value is None:
        return None
    if isinstance(value, bool):
        return _core.ATTR_BOOLEAN
    if isinstance(value, int):
        return _core.ATTR_INTEGER
    if isinstance(value, float):
        return _core.ATTR_DOUBLE
    if isinstance(value, str):
        return _core.ATTR_STRING
    return None


def _should_export_attr(info: dict) -> bool:
    return info.get("type") not in _UNSUPPORTED_EXPORT_TYPES


def _iter_network_attributes(network: Network, scope) -> Iterable[str]:
    for name in network.list_attributes(scope):
        info = network.attribute_info(scope, name)
        if _should_export_attr(info):
            yield name


def to_networkx(network: Network):
    nx = _lazy_import_networkx()
    graph = nx.DiGraph() if network.is_directed else nx.Graph()

    node_ids = network.node_indices()
    edge_entries = network.edges_with_indices()

    for node_id in node_ids:
        graph.add_node(node_id)

    for edge_id, (source, target) in edge_entries:
        graph.add_edge(source, target, _helios_edge_index=edge_id)

    for attr_name in _iter_network_attributes(network, _core.SCOPE_NETWORK):
        graph.graph[attr_name] = network.get_attribute_value(_core.SCOPE_NETWORK, attr_name, 0)

    for attr_name in _iter_network_attributes(network, _core.SCOPE_NODE):
        for node_id in node_ids:
            value = network.get_attribute_value(_core.SCOPE_NODE, attr_name, node_id)
            graph.nodes[node_id][attr_name] = value

    for attr_name in _iter_network_attributes(network, _core.SCOPE_EDGE):
        for edge_id, (source, target) in edge_entries:
            value = network.get_attribute_value(_core.SCOPE_EDGE, attr_name, edge_id)
            graph.edges[source, target][attr_name] = value

    return graph


def from_networkx(graph):
    nx = _lazy_import_networkx()
    if not isinstance(graph, (nx.Graph, nx.DiGraph)):
        raise TypeError("Expected a networkx Graph or DiGraph")

    network = graph.graph.get("_helios_network")
    if network is None:
        network = Network(directed=graph.is_directed())

    node_mapping = {}
    for node in graph.nodes:
        new_id = network.add_nodes(1)[0]
        node_mapping[node] = new_id

    edges = []
    for source, target in graph.edges:
        edges.append((node_mapping[source], node_mapping[target]))
    edge_ids = network.add_edges(edges) if edges else []
    edge_triplets = list(zip(graph.edges, edge_ids))

    _apply_networkx_attributes(network, graph, node_mapping, edge_triplets)
    return network


def to_igraph(network: Network):
    ig = _lazy_import_igraph()
    graph = ig.Graph(directed=bool(network.is_directed))

    node_ids = network.node_indices()
    node_id_to_index = {node_id: idx for idx, node_id in enumerate(node_ids)}
    edge_entries = network.edges_with_indices()
    graph.add_vertices(len(node_ids))
    graph.vs["_helios_id"] = node_ids

    edges = []
    for edge_id, (source, target) in edge_entries:
        edges.append((node_id_to_index[source], node_id_to_index[target]))
    if edges:
        graph.add_edges(edges)
        graph.es["_helios_edge_index"] = [edge_id for edge_id, _ in edge_entries]

    for attr_name in _iter_network_attributes(network, _core.SCOPE_NETWORK):
        graph[attr_name] = network.get_attribute_value(_core.SCOPE_NETWORK, attr_name, 0)

    for attr_name in _iter_network_attributes(network, _core.SCOPE_NODE):
        graph.vs[attr_name] = [
            network.get_attribute_value(_core.SCOPE_NODE, attr_name, node_id)
            for node_id in node_ids
        ]

    for attr_name in _iter_network_attributes(network, _core.SCOPE_EDGE):
        graph.es[attr_name] = [
            network.get_attribute_value(_core.SCOPE_EDGE, attr_name, edge_id)
            for edge_id, _ in edge_entries
        ]

    return graph


def from_igraph(graph):
    ig = _lazy_import_igraph()
    if not isinstance(graph, ig.Graph):
        raise TypeError("Expected an igraph Graph")

    network = Network(directed=graph.is_directed())

    node_ids = network.add_nodes(graph.vcount())
    edges = [(node_ids[source], node_ids[target]) for source, target in graph.get_edgelist()]
    edge_ids = network.add_edges(edges) if edges else []

    _apply_igraph_attributes(network, graph, node_ids, edge_ids)
    return network


def _apply_networkx_attributes(network: Network, graph, node_mapping, edge_triplets):
    node_attr_names = set()
    edge_attr_names = set()
    for _, attrs in graph.nodes(data=True):
        node_attr_names.update(attrs.keys())
    for _, _, attrs in graph.edges(data=True):
        edge_attr_names.update(attrs.keys())

    for attr_name, value in graph.graph.items():
        if attr_name.startswith("_"):
            continue
        attr_type = _infer_attribute_type(value)
        if attr_type is None:
            continue
        network.define_attribute(_core.SCOPE_NETWORK, attr_name, attr_type, 1)
        network.set_attribute_value(_core.SCOPE_NETWORK, attr_name, 0, value)

    for attr_name in sorted(node_attr_names):
        if attr_name.startswith("_"):
            continue
        sample_value = None
        for _, attrs in graph.nodes(data=True):
            sample_value = attrs.get(attr_name)
            if sample_value is not None:
                break
        attr_type = _infer_attribute_type(sample_value)
        if attr_type is None:
            continue
        dimension = _infer_vector_dimension(sample_value)
        network.define_attribute(_core.SCOPE_NODE, attr_name, attr_type, dimension)
        for node, attrs in graph.nodes(data=True):
            value = attrs.get(attr_name)
            if value is None:
                continue
            network.set_attribute_value(_core.SCOPE_NODE, attr_name, node_mapping[node], value)

    for attr_name in sorted(edge_attr_names):
        if attr_name.startswith("_"):
            continue
        sample_value = None
        for _, _, attrs in graph.edges(data=True):
            sample_value = attrs.get(attr_name)
            if sample_value is not None:
                break
        attr_type = _infer_attribute_type(sample_value)
        if attr_type is None:
            continue
        dimension = _infer_vector_dimension(sample_value)
        network.define_attribute(_core.SCOPE_EDGE, attr_name, attr_type, dimension)
        for (source, target), edge_id in edge_triplets:
            value = graph.edges[source, target].get(attr_name)
            if value is None:
                continue
            network.set_attribute_value(_core.SCOPE_EDGE, attr_name, edge_id, value)


def _apply_igraph_attributes(network: Network, graph, node_ids, edge_ids):
    for attr_name in graph.attributes():
        if attr_name.startswith("_"):
            continue
        value = graph[attr_name]
        attr_type = _infer_attribute_type(value)
        if attr_type is None:
            continue
        network.define_attribute(_core.SCOPE_NETWORK, attr_name, attr_type, 1)
        network.set_attribute_value(_core.SCOPE_NETWORK, attr_name, 0, value)

    for attr_name in graph.vs.attributes():
        if attr_name.startswith("_"):
            continue
        values = graph.vs[attr_name]
        sample_value = next((value for value in values if value is not None), None)
        attr_type = _infer_attribute_type(sample_value)
        if attr_type is None:
            continue
        dimension = _infer_vector_dimension(sample_value)
        network.define_attribute(_core.SCOPE_NODE, attr_name, attr_type, dimension)
        for node_id, value in zip(node_ids, values):
            if value is None:
                continue
            network.set_attribute_value(_core.SCOPE_NODE, attr_name, node_id, value)

    for attr_name in graph.es.attributes():
        if attr_name.startswith("_"):
            continue
        values = graph.es[attr_name]
        sample_value = next((value for value in values if value is not None), None)
        attr_type = _infer_attribute_type(sample_value)
        if attr_type is None:
            continue
        dimension = _infer_vector_dimension(sample_value)
        network.define_attribute(_core.SCOPE_EDGE, attr_name, attr_type, dimension)
        for edge_id, value in zip(edge_ids, values):
            if value is None:
                continue
            network.set_attribute_value(_core.SCOPE_EDGE, attr_name, edge_id, value)


def _infer_vector_dimension(value) -> int:
    if hasattr(value, "shape") and hasattr(value, "ndim"):
        if value.ndim == 1:
            return int(value.shape[0])
    if isinstance(value, (list, tuple)):
        return len(value)
    return 1
