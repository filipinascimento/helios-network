from __future__ import annotations

import warnings
from typing import Iterable, List, Sequence

from . import _core


class Network:
    """
    Python convenience wrapper around the Helios C core network.

    Parameters:
    -----------
    directed: bool
        If True, the graph is directed.
    node_capacity: int | None
        Optional initial node capacity.
    edge_capacity: int | None
        Optional initial edge capacity.
    """

    def __init__(self, directed: bool = False, node_capacity: int | None = None, edge_capacity: int | None = None, _core_network=None):
        """
        Create a network wrapper.

        Parameters:
        -----------
        directed: bool
            If True, the graph is directed.
        node_capacity: int | None
            Optional initial node capacity.
        edge_capacity: int | None
            Optional initial edge capacity.
        """
        if _core_network is not None:
            self._core = _core_network
        else:
            if node_capacity is None and edge_capacity is None:
                self._core = _core.Network(directed=directed)
            else:
                if node_capacity is None:
                    node_capacity = 128
                if edge_capacity is None:
                    edge_capacity = 256
                self._core = _core.Network(directed=directed, node_capacity=node_capacity, edge_capacity=edge_capacity)

        self.nodes = NodeCollection(self)
        self.edges = EdgeCollection(self)
        self.network = NetworkAttributeCollection(self)
        self.graph = self.network
        self.attributes = self.network

    def __getattr__(self, name):
        """
        Forward unknown attributes to the C core wrapper.

        Parameters:
        -----------
        name: str
            Attribute name to forward.

        Returns:
        --------
        Any
            The forwarded attribute from the C core wrapper.
        """
        return getattr(self._core, name)

    def __getitem__(self, name: str):
        """
        Get a network-scope attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.

        Returns:
        --------
        Any
            The attribute value for index 0 in network scope.
        """
        if not isinstance(name, str):
            raise TypeError("Attribute name must be a string")
        return self._core.get_attribute_value(_core.SCOPE_NETWORK, name, 0)

    def __setitem__(self, name: str, value):
        """
        Set a network-scope attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.
        value: Any
            Value to set (auto-defines the attribute if missing).
        """
        _ensure_attribute(self, _core.SCOPE_NETWORK, name, value)
        if not isinstance(name, str):
            raise TypeError("Attribute name must be a string")
        self._core.set_attribute_value(_core.SCOPE_NETWORK, name, 0, value)

    def edge_pairs(self):
        """
        Iterate over active edge endpoint pairs.

        Returns:
        --------
        Iterator[tuple[int, int]]
            Yields (source, target) for each active edge.
        """
        return self.edges.pairs()


class NodeView:
    """
    Single-node attribute access wrapper.

    Parameters:
    -----------
    network: Network
        Parent network.
    node_id: int
        Node index.
    """

    def __init__(self, network: Network, node_id: int):
        self._network = network
        self.id = int(node_id)

    def __getitem__(self, name: str):
        """
        Get a node attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.

        Returns:
        --------
        Any
            Attribute value for the node.
        """
        return self._network._core.get_attribute_value(_core.SCOPE_NODE, name, self.id)

    def __setitem__(self, name: str, value):
        """
        Set a node attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.
        value: Any
            Value to set (auto-defines the attribute if missing).
        """
        _ensure_attribute(self._network, _core.SCOPE_NODE, name, value)
        self._network._core.set_attribute_value(_core.SCOPE_NODE, name, self.id, value)

    def __repr__(self) -> str:
        attrs = _format_preview_attributes(self._network, _core.SCOPE_NODE, self.id)
        return f"<NodeView id={self.id}{attrs}>"


class EdgeView:
    """
    Single-edge attribute access wrapper.

    Parameters:
    -----------
    network: Network
        Parent network.
    edge_id: int
        Edge index.
    """

    def __init__(self, network: Network, edge_id: int):
        self._network = network
        self.id = int(edge_id)

    @property
    def endpoints(self):
        """
        Return (source, target) for this edge.

        Returns:
        --------
        tuple[int, int]
            (source, target)
        """
        return self._network._core.edge_endpoints(self.id)

    def __getitem__(self, name: str):
        """
        Get an edge attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.

        Returns:
        --------
        Any
            Attribute value for the edge.
        """
        return self._network._core.get_attribute_value(_core.SCOPE_EDGE, name, self.id)

    def __setitem__(self, name: str, value):
        """
        Set an edge attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.
        value: Any
            Value to set (auto-defines the attribute if missing).
        """
        _ensure_attribute(self._network, _core.SCOPE_EDGE, name, value)
        self._network._core.set_attribute_value(_core.SCOPE_EDGE, name, self.id, value)

    def __repr__(self) -> str:
        source, target = self.endpoints
        attrs = _format_preview_attributes(self._network, _core.SCOPE_EDGE, self.id)
        return f"<EdgeView id={self.id} ({source}->{target}){attrs}>"


class NetworkView:
    """
    Network-level attribute access wrapper.

    Parameters:
    -----------
    network: Network
        Parent network.
    """

    def __init__(self, network: Network):
        self._network = network

    def __getitem__(self, name: str):
        """
        Get a network attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.

        Returns:
        --------
        Any
            Attribute value for the network scope (index 0).
        """
        return self._network._core.get_attribute_value(_core.SCOPE_NETWORK, name, 0)

    def __setitem__(self, name: str, value):
        """
        Set a network attribute value by name.

        Parameters:
        -----------
        name: str
            Attribute name.
        value: Any
            Value to set (auto-defines the attribute if missing).
        """
        _ensure_attribute(self._network, _core.SCOPE_NETWORK, name, value)
        self._network._core.set_attribute_value(_core.SCOPE_NETWORK, name, 0, value)

    def __repr__(self) -> str:
        return "<NetworkView>"


class NodeSelector:
    """
    Selector for multiple nodes (active-only).

    Parameters:
    -----------
    network: Network
        Parent network.
    node_ids: Sequence[int]
        Node indices to select.
    """

    def __init__(self, network: Network, node_ids: Sequence[int]):
        self._network = network
        self._ids = _filter_active_nodes(network, node_ids)

    def __iter__(self):
        """
        Iterate NodeView for active ids.

        Returns:
        --------
        Iterator[NodeView]
            Node views for active ids.
        """
        for node_id in self._ids:
            yield NodeView(self._network, node_id)

    def __len__(self) -> int:
        return len(self._ids)

    def __getitem__(self, name: str):
        """
        Return a list of attribute values for the selector.

        Parameters:
        -----------
        name: str
            Attribute name.

        Returns:
        --------
        list
            Values for each selected node.
        """
        if not isinstance(name, str):
            raise TypeError("Attribute name must be a string")
        return [
            self._network._core.get_attribute_value(_core.SCOPE_NODE, name, node_id)
            for node_id in self._ids
        ]

    def __setitem__(self, name: str, value):
        """
        Set attribute values for the selector (broadcast or per-item).

        Parameters:
        -----------
        name: str
            Attribute name.
        value: Any
            Single value or per-node list/array.
        """
        if not isinstance(name, str):
            raise TypeError("Attribute name must be a string")
        _ensure_attribute(self._network, _core.SCOPE_NODE, name, value)
        values = _prepare_values(self._network, _core.SCOPE_NODE, name, value, len(self._ids), "node selector")
        for node_id, item in zip(self._ids, values):
            self._network._core.set_attribute_value(_core.SCOPE_NODE, name, node_id, item)

    @property
    def ids(self) -> List[int]:
        return list(self._ids)


class EdgeSelector:
    """
    Selector for multiple edges (active-only).

    Parameters:
    -----------
    network: Network
        Parent network.
    edge_ids: Sequence[int]
        Edge indices to select.
    """

    def __init__(self, network: Network, edge_ids: Sequence[int]):
        self._network = network
        self._ids = _filter_active_edges(network, edge_ids)

    def __iter__(self):
        """
        Iterate EdgeView for active ids.

        Returns:
        --------
        Iterator[EdgeView]
            Edge views for active ids.
        """
        for edge_id in self._ids:
            yield EdgeView(self._network, edge_id)

    def __len__(self) -> int:
        return len(self._ids)

    def __getitem__(self, name: str):
        """
        Return a list of attribute values for the selector.

        Parameters:
        -----------
        name: str
            Attribute name.

        Returns:
        --------
        list
            Values for each selected edge.
        """
        if not isinstance(name, str):
            raise TypeError("Attribute name must be a string")
        return [
            self._network._core.get_attribute_value(_core.SCOPE_EDGE, name, edge_id)
            for edge_id in self._ids
        ]

    def __setitem__(self, name: str, value):
        """
        Set attribute values for the selector (broadcast or per-item).

        Parameters:
        -----------
        name: str
            Attribute name.
        value: Any
            Single value or per-edge list/array.
        """
        if not isinstance(name, str):
            raise TypeError("Attribute name must be a string")
        _ensure_attribute(self._network, _core.SCOPE_EDGE, name, value)
        values = _prepare_values(self._network, _core.SCOPE_EDGE, name, value, len(self._ids), "edge selector")
        for edge_id, item in zip(self._ids, values):
            self._network._core.set_attribute_value(_core.SCOPE_EDGE, name, edge_id, item)

    @property
    def ids(self) -> List[int]:
        return list(self._ids)


class NodeCollection:
    """
    Active node collection with selector helpers.

    Parameters:
    -----------
    network: Network
        Parent network.
    """

    def __init__(self, network: Network):
        self._network = network

    def __iter__(self):
        """
        Iterate NodeView for all active nodes.

        Returns:
        --------
        Iterator[NodeView]
            Node views for active nodes.
        """
        for node_id in self._network._core.node_indices():
            yield NodeView(self._network, node_id)

    def __len__(self) -> int:
        return len(self._network._core.node_indices())

    def __getitem__(self, key):
        """
        Select nodes or get attribute values for all active nodes.

        Parameters:
        -----------
        key: str | int | Sequence[int]
            Attribute name, single node id, or list of node ids.

        Returns:
        --------
        list | NodeView | NodeSelector
            Attribute values or selector/view.
        """
        if isinstance(key, str):
            ids = self._network._core.node_indices()
            return [
                self._network._core.get_attribute_value(_core.SCOPE_NODE, key, node_id)
                for node_id in ids
            ]
        if isinstance(key, int):
            if not self._network._core.is_node_active(key):
                raise IndexError("Node is not active")
            return NodeView(self._network, key)
        ids = _coerce_ids(key)
        return NodeSelector(self._network, ids)

    def __setitem__(self, key, value):
        """
        Set attribute values for all active nodes (broadcast or per-item).

        Parameters:
        -----------
        key: str
            Attribute name.
        value: Any
            Single value or per-node list/array.
        """
        if not isinstance(key, str):
            raise TypeError("Attribute name must be a string")
        _ensure_attribute(self._network, _core.SCOPE_NODE, key, value)
        ids = self._network._core.node_indices()
        values = _prepare_values(self._network, _core.SCOPE_NODE, key, value, len(ids), "node collection")
        for node_id, item in zip(ids, values):
            self._network._core.set_attribute_value(_core.SCOPE_NODE, key, node_id, item)

    def define_attribute(self, name, attr_type, dimension=1):
        """
        Define a node attribute.

        Parameters:
        -----------
        name: str
            Attribute name.
        attr_type: Any
            AttributeType enum, Python type, or numpy dtype.
        dimension: int
            Vector dimension (default: 1).
        """
        return _define_attribute_helper(self._network, _core.SCOPE_NODE, name, attr_type, dimension)


class EdgeCollection:
    """
    Active edge collection with selector helpers.

    Parameters:
    -----------
    network: Network
        Parent network.
    """

    def __init__(self, network: Network):
        self._network = network

    def __iter__(self):
        """
        Iterate EdgeView for all active edges.

        Returns:
        --------
        Iterator[EdgeView]
            Edge views for active edges.
        """
        for edge_id in self._network._core.edge_indices():
            yield EdgeView(self._network, edge_id)

    def __len__(self) -> int:
        return len(self._network._core.edge_indices())

    def __getitem__(self, key):
        """
        Select edges or get attribute values for all active edges.

        Parameters:
        -----------
        key: str | int | Sequence[int]
            Attribute name, single edge id, or list of edge ids.

        Returns:
        --------
        list | EdgeView | EdgeSelector
            Attribute values or selector/view.
        """
        if isinstance(key, str):
            ids = self._network._core.edge_indices()
            return [
                self._network._core.get_attribute_value(_core.SCOPE_EDGE, key, edge_id)
                for edge_id in ids
            ]
        if isinstance(key, int):
            if not self._network._core.is_edge_active(key):
                raise IndexError("Edge is not active")
            return EdgeView(self._network, key)
        ids = _coerce_ids(key)
        return EdgeSelector(self._network, ids)

    def __setitem__(self, key, value):
        """
        Set attribute values for all active edges (broadcast or per-item).

        Parameters:
        -----------
        key: str
            Attribute name.
        value: Any
            Single value or per-edge list/array.
        """
        if not isinstance(key, str):
            raise TypeError("Attribute name must be a string")
        _ensure_attribute(self._network, _core.SCOPE_EDGE, key, value)
        ids = self._network._core.edge_indices()
        values = _prepare_values(self._network, _core.SCOPE_EDGE, key, value, len(ids), "edge collection")
        for edge_id, item in zip(ids, values):
            self._network._core.set_attribute_value(_core.SCOPE_EDGE, key, edge_id, item)

    def pairs(self):
        """
        Yield (source, target) for each active edge.

        Returns:
        --------
        Iterator[tuple[int, int]]
            (source, target) for each edge.
        """
        for _, (source, target) in self._network._core.edges_with_indices():
            yield (source, target)

    def with_indices(self):
        """
        Yield (edge_id, (source, target)) for each active edge.

        Returns:
        --------
        Iterator[tuple[int, tuple[int, int]]]
            (edge_id, (source, target)) for each edge.
        """
        for edge_id, (source, target) in self._network._core.edges_with_indices():
            yield (edge_id, (source, target))

    def define_attribute(self, name, attr_type, dimension=1):
        """
        Define an edge attribute.

        Parameters:
        -----------
        name: str
            Attribute name.
        attr_type: Any
            AttributeType enum, Python type, or numpy dtype.
        dimension: int
            Vector dimension (default: 1).
        """
        return _define_attribute_helper(self._network, _core.SCOPE_EDGE, name, attr_type, dimension)


class NetworkAttributeCollection:
    """
    Network-level attribute collection.

    Parameters:
    -----------
    network: Network
        Parent network.
    """

    def __init__(self, network: Network):
        self._network = network

    def __iter__(self):
        """
        Iterate a single NetworkView (index 0).

        Returns:
        --------
        Iterator[NetworkView]
            One network view.
        """
        yield NetworkView(self._network)

    def __len__(self) -> int:
        return 1

    def __getitem__(self, key):
        """
        Get a network attribute value by name or index 0.

        Parameters:
        -----------
        key: str | int | Sequence[int]
            Attribute name or 0.

        Returns:
        --------
        Any | NetworkView
            Attribute value or network view.
        """
        if isinstance(key, str):
            return self._network._core.get_attribute_value(_core.SCOPE_NETWORK, key, 0)
        if isinstance(key, int):
            if key != 0:
                raise IndexError("Network scope only supports index 0")
            return NetworkView(self._network)
        ids = _coerce_ids(key)
        if len(ids) != 1 or ids[0] != 0:
            raise IndexError("Network scope only supports index 0")
        return NetworkView(self._network)

    def __setitem__(self, key, value):
        """
        Set a network attribute value by name.

        Parameters:
        -----------
        key: str
            Attribute name.
        value: Any
            Value to set (auto-defines the attribute if missing).
        """
        if not isinstance(key, str):
            raise TypeError("Attribute name must be a string")
        _ensure_attribute(self._network, _core.SCOPE_NETWORK, key, value)
        values = _prepare_values(self._network, _core.SCOPE_NETWORK, key, value, 1, "network collection")
        self._network._core.set_attribute_value(_core.SCOPE_NETWORK, key, 0, values[0])

    def define_attribute(self, name, attr_type, dimension=1):
        """
        Define a network attribute.

        Parameters:
        -----------
        name: str
            Attribute name.
        attr_type: Any
            AttributeType enum, Python type, or numpy dtype.
        dimension: int
            Vector dimension (default: 1).
        """
        return _define_attribute_helper(self._network, _core.SCOPE_NETWORK, name, attr_type, dimension)

def _coerce_ids(obj: Iterable[int] | Sequence[int]) -> List[int]:
    if isinstance(obj, (str, bytes)):
        raise TypeError("Expected an iterable of indices, not a string")
    if isinstance(obj, (list, tuple, set)):
        return [int(item) for item in obj]
    if isinstance(obj, Iterable):
        return [int(item) for item in obj]
    raise TypeError("Expected an iterable of indices")


def _prepare_values(network: Network, scope, name: str, value, count: int, context: str):
    info = network._core.attribute_info(scope, name)
    dimension = int(info.get("dimension") or 1)
    if dimension <= 1:
        return _normalize_scalar_values(value, count, context)
    return _normalize_vector_values(value, count, dimension, context)


def _normalize_scalar_values(value, count: int, context: str):
    if _is_iterable_value(value):
        values = list(value)
        if len(values) == count:
            return values
        warnings.warn(
            f"Value length mismatch for {context}; using the first value for all items.",
            RuntimeWarning,
            stacklevel=2,
        )
        if len(values) == 1:
            return [values[0]] * count
        if values:
            return [values[0]] * count
        return [None] * count
    return [value] * count


def _normalize_vector_values(value, count: int, dimension: int, context: str):
    np_array = _as_numpy_array(value)
    if np_array is not None:
        if np_array.ndim == 1 and np_array.shape[0] == dimension:
            vector = np_array.tolist()
            return [vector] * count
        if np_array.ndim == 2 and np_array.shape[0] == count and np_array.shape[1] == dimension:
            return [row.tolist() for row in np_array]
        warnings.warn(
            f"Value shape mismatch for {context}; using the first value for all items.",
            RuntimeWarning,
            stacklevel=2,
        )
        if np_array.ndim >= 1 and np_array.size > 0:
            vector = np_array.reshape(-1).tolist()[:dimension]
            if len(vector) < dimension:
                vector += [0] * (dimension - len(vector))
            return [vector] * count
        return [[0] * dimension for _ in range(count)]

    if _is_iterable_value(value):
        values = list(value)
        if len(values) == count and all(_is_vector_item(item, dimension) for item in values):
            return [list(item) for item in values]
        if _is_vector_item(values, dimension):
            return [list(values)] * count
        warnings.warn(
            f"Value length mismatch for {context}; using the first value for all items.",
            RuntimeWarning,
            stacklevel=2,
        )
        if values and _is_vector_item(values, dimension):
            return [list(values)] * count
        if values and _is_vector_item(values[0], dimension):
            return [list(values[0])] * count
        if values and len(values) >= dimension:
            return [list(values[:dimension])] * count
        return [[0] * dimension for _ in range(count)]
    raise TypeError("Vector attribute assignment requires an iterable value")


def _is_iterable_value(value) -> bool:
    if isinstance(value, (str, bytes)):
        return False
    return isinstance(value, Iterable)


def _is_vector_item(value, dimension: int) -> bool:
    if not _is_iterable_value(value):
        return False
    try:
        values = list(value)
    except TypeError:
        return False
    if len(values) != dimension:
        return False
    return all(_is_scalar_value(item) for item in values)


def _is_scalar_value(value) -> bool:
    if isinstance(value, (str, bytes)):
        return True
    return not _is_iterable_value(value)


def _as_numpy_array(value):
    if hasattr(value, "shape") and hasattr(value, "ndim") and hasattr(value, "tolist"):
        return value
    return None


def _format_preview_attributes(network: Network, scope, index: int, limit: int = 5) -> str:
    try:
        names = network._core.list_attributes(scope)
    except Exception:
        return ""
    if not names:
        return ""
    preview = []
    for name in names[:limit]:
        try:
            value = network._core.get_attribute_value(scope, name, index)
        except Exception:
            value = None
        preview.append(f"{name}={value}")
    suffix = " ..." if len(names) > limit else ""
    return f" attrs={{" + ", ".join(preview) + "}}" + suffix


def _ensure_attribute(network: Network, scope, name: str, value):
    try:
        network._core.attribute_info(scope, name)
        return
    except Exception:
        pass
    attr_type, dimension = _infer_attribute_from_value(value)
    if attr_type is None:
        raise ValueError("Unable to infer attribute type from value")
    _define_attribute_helper(network, scope, name, attr_type, dimension)


def _define_attribute_helper(network: Network, scope, attr_name: str, attr_type, dimension=1):
    attr_type_id = _coerce_attribute_type(attr_type)
    if dimension is None or dimension <= 0:
        dimension = 1
    return network._core.define_attribute(scope, attr_name, attr_type_id, dimension)


def _coerce_attribute_type(value):
    if isinstance(value, int):
        return value
    if value is bool:
        return _core.ATTR_BOOLEAN
    if value is int:
        return _core.ATTR_INTEGER
    if value is float:
        return _core.ATTR_DOUBLE
    if value is str:
        return _core.ATTR_STRING
    dtype_name = getattr(value, "dtype", None)
    if dtype_name is not None:
        name = str(dtype_name)
        return _dtype_to_attr_type(name)
    return value


def _infer_attribute_from_value(value):
    np_array = _as_numpy_array(value)
    if np_array is not None:
        dtype_name = str(getattr(np_array, "dtype", ""))
        attr_type = _dtype_to_attr_type(dtype_name)
        if np_array.ndim == 1:
            return attr_type, int(np_array.shape[0])
        if np_array.ndim == 2:
            return attr_type, int(np_array.shape[1])
        return attr_type, 1

    if _is_iterable_value(value):
        values = list(value)
        if not values:
            return _core.ATTR_DOUBLE, 1
        if _is_vector_item(values, len(values)):
            return _infer_scalar_type(values[0]), len(values)
        first = values[0]
        if _is_iterable_value(first):
            vector = list(first)
            if vector:
                return _infer_scalar_type(vector[0]), len(vector)
        return _infer_scalar_type(values[0]), 1
    return _infer_scalar_type(value), 1


def _infer_scalar_type(value):
    if isinstance(value, bool):
        return _core.ATTR_BOOLEAN
    if isinstance(value, int):
        return _core.ATTR_INTEGER
    if isinstance(value, float):
        return _core.ATTR_DOUBLE
    if isinstance(value, str):
        return _core.ATTR_STRING
    return _core.ATTR_DOUBLE


def _dtype_to_attr_type(name: str):
    name = name.lower()
    if "bool" in name:
        return _core.ATTR_BOOLEAN
    if "int64" in name or "uint64" in name:
        return _core.ATTR_BIG_INTEGER if "int64" in name else _core.ATTR_UNSIGNED_BIG_INTEGER
    if "uint" in name:
        return _core.ATTR_UNSIGNED_INTEGER
    if "int" in name:
        return _core.ATTR_INTEGER
    if "float" in name:
        return _core.ATTR_DOUBLE
    return _core.ATTR_DOUBLE


def _filter_active_nodes(network: Network, ids: Sequence[int]) -> List[int]:
    active = []
    dropped = 0
    for node_id in ids:
        if network._core.is_node_active(node_id):
            active.append(int(node_id))
        else:
            dropped += 1
    if dropped:
        warnings.warn("Some nodes are inactive and were skipped.", RuntimeWarning, stacklevel=2)
    return active


def _filter_active_edges(network: Network, ids: Sequence[int]) -> List[int]:
    active = []
    dropped = 0
    for edge_id in ids:
        if network._core.is_edge_active(edge_id):
            active.append(int(edge_id))
        else:
            dropped += 1
    if dropped:
        warnings.warn("Some edges are inactive and were skipped.", RuntimeWarning, stacklevel=2)
    return active
