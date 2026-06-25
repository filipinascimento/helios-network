from __future__ import annotations

from contextlib import contextmanager
import os
import struct
import tempfile
import warnings
from typing import Iterable, List, Sequence

from . import _core


NETWORK_EVENTS = {
    "topology_changed": "topology:changed",
    "nodes_added": "nodes:added",
    "nodes_removed": "nodes:removed",
    "edges_added": "edges:added",
    "edges_removed": "edges:removed",
    "attribute_defined": "attribute:defined",
    "attribute_removed": "attribute:removed",
    "attribute_changed": "attribute:changed",
    "category_changed": "category:changed",
    "batch_applied": "batch:applied",
}

_SCOPE_NAMES = {
    _core.SCOPE_NODE: "node",
    _core.SCOPE_EDGE: "edge",
    _core.SCOPE_NETWORK: "network",
}

_SCOPE_IDS = {value: key for key, value in _SCOPE_NAMES.items()}

_ATTRIBUTE_TYPE_NAMES = {
    _core.ATTR_STRING: "String",
    _core.ATTR_BOOLEAN: "Boolean",
    _core.ATTR_FLOAT: "Float",
    _core.ATTR_INTEGER: "Integer",
    _core.ATTR_UNSIGNED_INTEGER: "UnsignedInteger",
    _core.ATTR_DOUBLE: "Double",
    _core.ATTR_CATEGORY: "Category",
    _core.ATTR_DATA: "Data",
    _core.ATTR_JAVASCRIPT: "Javascript",
    _core.ATTR_BIG_INTEGER: "BigInteger",
    _core.ATTR_UNSIGNED_BIG_INTEGER: "UnsignedBigInteger",
    _core.ATTR_MULTI_CATEGORY: "MultiCategory",
    _core.ATTR_UNKNOWN: "Unknown",
}


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
        self._event_handlers = {}
        self._any_event_handlers = []
        self._mutation_journal = []
        self._batch_depth = 0
        self._pending_mutations = []

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
        self._emit_attribute_changed(_core.SCOPE_NETWORK, name, [0], [value])

    def on(self, event_type: str, handler):
        """Register a network event handler and return an unsubscribe callback."""
        if not isinstance(event_type, str) or not event_type:
            raise TypeError("Event type must be a non-empty string")
        if not callable(handler):
            raise TypeError("Event handler must be callable")
        handlers = self._event_handlers.setdefault(event_type, [])
        handlers.append(handler)

        def unsubscribe():
            self.off(event_type, handler)

        return unsubscribe

    def off(self, event_type: str, handler):
        """Remove an event handler previously registered with `on`."""
        handlers = self._event_handlers.get(event_type)
        if not handlers:
            return
        try:
            handlers.remove(handler)
        except ValueError:
            return
        if not handlers:
            self._event_handlers.pop(event_type, None)

    def on_any(self, handler):
        """Register a handler that receives every emitted event."""
        if not callable(handler):
            raise TypeError("Event handler must be callable")
        self._any_event_handlers.append(handler)

        def unsubscribe():
            try:
                self._any_event_handlers.remove(handler)
            except ValueError:
                pass

        return unsubscribe

    def emit(self, event_type: str, detail=None):
        """Emit an event immediately to matching and catch-all handlers."""
        event = {"type": event_type, "detail": detail, "target": self}
        for handler in list(self._event_handlers.get(event_type, [])):
            handler(event)
        for handler in list(self._any_event_handlers):
            handler(event)
        return event

    def begin_batch(self):
        """Begin coalescing mutation events into one `batch:applied` event."""
        self._batch_depth += 1
        return self

    def end_batch(self):
        """End a mutation batch and emit the coalesced event at outermost depth."""
        if self._batch_depth <= 0:
            raise RuntimeError("No active mutation batch")
        self._batch_depth -= 1
        if self._batch_depth == 0 and self._pending_mutations:
            events = self._pending_mutations
            self._pending_mutations = []
            detail = {
                "events": [_public_event_payload(event) for event in events],
                "records": [event["record"] for event in events if event.get("record")],
            }
            detail["text"] = mutation_events_to_text_batch(detail["events"])
            self._record_and_emit(NETWORK_EVENTS["batch_applied"], detail)
        return self

    @contextmanager
    def batch(self):
        """Context manager that coalesces mutations until the block exits."""
        self.begin_batch()
        try:
            yield self
        finally:
            self.end_batch()

    def mutation_journal(self):
        """Return a copy of mutation events emitted since the last drain."""
        return list(self._mutation_journal)

    def drain_mutation_journal(self):
        """Return and clear the mutation journal."""
        journal = list(self._mutation_journal)
        self._mutation_journal.clear()
        return journal

    def add_nodes(self, count: int):
        old_node_count = self._core.node_count()
        old_edge_count = self._core.edge_count()
        indices = self._core.add_nodes(int(count))
        detail = {
            "indices": list(indices),
            "count": len(indices),
            "old_node_count": old_node_count,
            "node_count": self._core.node_count(),
            "old_edge_count": old_edge_count,
            "edge_count": self._core.edge_count(),
        }
        self._emit_mutation(NETWORK_EVENTS["nodes_added"], detail, {
            "op": "ADD_NODES",
            "count": len(indices),
            "indices": list(indices),
        })
        self._emit_mutation(NETWORK_EVENTS["topology_changed"], {
            "kind": "nodes",
            "op": "added",
            "node_count": detail["node_count"],
            "edge_count": detail["edge_count"],
        })
        return indices

    def remove_nodes(self, indices):
        ids = [int(index) for index in indices]
        old_node_count = self._core.node_count()
        old_edge_count = self._core.edge_count()
        result = self._core.remove_nodes(ids)
        detail = {
            "indices": ids,
            "count": len(ids),
            "old_node_count": old_node_count,
            "node_count": self._core.node_count(),
            "old_edge_count": old_edge_count,
            "edge_count": self._core.edge_count(),
        }
        self._emit_mutation(NETWORK_EVENTS["nodes_removed"], detail, {
            "op": "REMOVE_NODES",
            "ids": ids,
        })
        self._emit_mutation(NETWORK_EVENTS["topology_changed"], {
            "kind": "nodes",
            "op": "removed",
            "node_count": detail["node_count"],
            "edge_count": detail["edge_count"],
        })
        return result

    def add_edges(self, edge_list):
        pairs = [(int(source), int(target)) for source, target in edge_list]
        old_node_count = self._core.node_count()
        old_edge_count = self._core.edge_count()
        indices = self._core.add_edges(pairs)
        detail = {
            "indices": list(indices),
            "pairs": pairs,
            "count": len(indices),
            "old_node_count": old_node_count,
            "node_count": self._core.node_count(),
            "old_edge_count": old_edge_count,
            "edge_count": self._core.edge_count(),
        }
        self._emit_mutation(NETWORK_EVENTS["edges_added"], detail, {
            "op": "ADD_EDGES",
            "pairs": pairs,
            "indices": list(indices),
        })
        self._emit_mutation(NETWORK_EVENTS["topology_changed"], {
            "kind": "edges",
            "op": "added",
            "node_count": detail["node_count"],
            "edge_count": detail["edge_count"],
        })
        return indices

    def add_edges_from_arrays(self, sources, targets, return_indices: bool = False):
        """
        Add edges from contiguous integer source and target buffers.

        This avoids materializing a Python list of `(source, target)` tuples and
        is intended for large generated graphs. `sources` and `targets` must be
        one-dimensional buffer objects with 32-bit or 64-bit integer storage.
        """
        old_node_count = self._core.node_count()
        old_edge_count = self._core.edge_count()
        result = self._core.add_edges_from_arrays(
            sources,
            targets,
            return_indices=bool(return_indices),
        )
        count = len(result) if return_indices else int(result)
        detail = {
            "indices": list(result) if return_indices else None,
            "pairs": None,
            "count": count,
            "old_node_count": old_node_count,
            "node_count": self._core.node_count(),
            "old_edge_count": old_edge_count,
            "edge_count": self._core.edge_count(),
        }
        self._emit_mutation(NETWORK_EVENTS["edges_added"], detail, {
            "op": "ADD_EDGES_FROM_ARRAYS",
            "count": count,
            "indices": list(result) if return_indices else None,
        })
        self._emit_mutation(NETWORK_EVENTS["topology_changed"], {
            "kind": "edges",
            "op": "added",
            "node_count": detail["node_count"],
            "edge_count": detail["edge_count"],
        })
        return result

    def remove_edges(self, indices):
        ids = [int(index) for index in indices]
        old_node_count = self._core.node_count()
        old_edge_count = self._core.edge_count()
        result = self._core.remove_edges(ids)
        detail = {
            "indices": ids,
            "count": len(ids),
            "old_node_count": old_node_count,
            "node_count": self._core.node_count(),
            "old_edge_count": old_edge_count,
            "edge_count": self._core.edge_count(),
        }
        self._emit_mutation(NETWORK_EVENTS["edges_removed"], detail, {
            "op": "REMOVE_EDGES",
            "ids": ids,
        })
        self._emit_mutation(NETWORK_EVENTS["topology_changed"], {
            "kind": "edges",
            "op": "removed",
            "node_count": detail["node_count"],
            "edge_count": detail["edge_count"],
        })
        return result

    def define_attribute(self, scope, name: str, attr_type, dimension=1):
        return _define_attribute_helper(self, _coerce_scope(scope), name, attr_type, dimension)

    def set_attribute_value(self, scope, name: str, index: int, value):
        scope_id = _coerce_scope(scope)
        _ensure_attribute(self, scope_id, name, value)
        self._core.set_attribute_value(scope_id, name, int(index), value)
        self._emit_attribute_changed(scope_id, name, [int(index)], [value])

    def get_attribute_value(self, scope, name: str, index: int):
        return self._core.get_attribute_value(_coerce_scope(scope), name, int(index))

    def categorize_attribute(self, scope, name: str, **kwargs):
        result = self._core.categorize_attribute(_coerce_scope(scope), name, **kwargs)
        self._emit_category_changed(_coerce_scope(scope), name, "categorize")
        return result

    def decategorize_attribute(self, scope, name: str, **kwargs):
        result = self._core.decategorize_attribute(_coerce_scope(scope), name, **kwargs)
        self._emit_category_changed(_coerce_scope(scope), name, "decategorize")
        return result

    def set_category_dictionary(self, scope, name: str, entries, **kwargs):
        result = self._core.set_category_dictionary(_coerce_scope(scope), name, entries, **kwargs)
        self._emit_category_changed(_coerce_scope(scope), name, "set_dictionary", entries=entries)
        return result

    def to_bxnet_bytes(self):
        """Serialize the network to an in-memory `.bxnet` byte payload."""

        return self._serialized_bytes("bxnet")

    def to_zxnet_bytes(self, compression: int = 6):
        """Serialize the network to an in-memory `.zxnet` byte payload."""

        return self._serialized_bytes("zxnet", compression=compression)

    def to_gt_bytes(self):
        """Serialize the network to an in-memory graph-tool `.gt` byte payload."""

        return self._serialized_bytes("gt")

    def _serialized_bytes(self, kind: str, compression: int = 6):
        suffix = f".{kind}"
        fd, path = tempfile.mkstemp(suffix=suffix)
        os.close(fd)
        try:
            if kind == "bxnet":
                self._core.save_bxnet(path)
            elif kind == "zxnet":
                self._core.save_zxnet(path, int(compression))
            elif kind == "xnet":
                self._core.save_xnet(path)
            elif kind == "gt":
                self._core.save_gt(path)
            else:
                raise ValueError(f"Unsupported serialization kind: {kind}")
            with open(path, "rb") as handle:
                return handle.read()
        finally:
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass

    def _emit_attribute_defined(self, scope, name: str, attr_type, dimension: int):
        detail = {
            "scope": _scope_name(scope),
            "name": name,
            "type": int(attr_type),
            "type_name": _ATTRIBUTE_TYPE_NAMES.get(int(attr_type), str(attr_type)),
            "dimension": int(dimension or 1),
        }
        self._emit_mutation(NETWORK_EVENTS["attribute_defined"], detail, {
            "op": "DEFINE_ATTRIBUTE",
            **detail,
        })

    def _emit_attribute_changed(self, scope, name: str, ids, values):
        detail = {
            "scope": _scope_name(scope),
            "name": name,
            "ids": [int(item) for item in ids],
            "values": _jsonable_values(values),
        }
        self._emit_mutation(NETWORK_EVENTS["attribute_changed"], detail, {
            "op": "SET_ATTR_VALUES",
            **detail,
        })

    def _emit_category_changed(self, scope, name: str, op: str, **extra):
        detail = {
            "scope": _scope_name(scope),
            "name": name,
            "op": op,
            **extra,
        }
        self._emit_mutation(NETWORK_EVENTS["category_changed"], detail, {
            "op": "CATEGORY_CHANGE",
            "category_op": op,
            "scope": detail["scope"],
            "name": name,
            **extra,
        })

    def _emit_mutation(self, event_type: str, detail, record=None):
        event = {"type": event_type, "detail": detail}
        if record is not None:
            event["record"] = record
        if self._batch_depth > 0:
            self._pending_mutations.append(event)
            return event
        self._record_and_emit(event_type, detail, record=record)
        return event

    def _record_and_emit(self, event_type: str, detail, record=None):
        journal_entry = {"type": event_type, "detail": detail}
        if record is not None:
            journal_entry["record"] = record
        self._mutation_journal.append(journal_entry)
        return self.emit(event_type, detail)

    def edge_pairs(self):
        """
        Iterate over active edge endpoint pairs.

        Returns:
        --------
        Iterator[tuple[int, int]]
            Yields (source, target) for each active edge.
        """
        return self.edges.pairs()

    def _extract_component_network(self, node_indices: Sequence[int], edge_indices: Sequence[int]) -> "Network":
        subgraph = Network(
            directed=bool(self.is_directed),
            node_capacity=max(1, len(node_indices)),
            edge_capacity=max(1, len(edge_indices)),
        )
        if not node_indices:
            return subgraph

        new_nodes = subgraph.add_nodes(len(node_indices))
        node_map = {int(source): int(target) for source, target in zip(node_indices, new_nodes)}
        edge_payload = []
        for edge_id in edge_indices:
            source, target = self._core.edge_endpoints(int(edge_id))
            mapped_source = node_map.get(int(source))
            mapped_target = node_map.get(int(target))
            if mapped_source is None or mapped_target is None:
                continue
            edge_payload.append((mapped_source, mapped_target))
        if edge_payload:
            subgraph.add_edges(edge_payload)
        return subgraph

    def extract_connected_components(
        self,
        mode="weak",
        min_size: int = 1,
        as_networks: bool = False,
        out_node_component_attribute: str | None = "component",
    ):
        """
        Extract connected components as explicit partitions.

        Parameters:
        -----------
        mode: str | int
            Connected-components mode ("weak" or "strong").
        min_size: int
            Minimum number of nodes per returned component.
        as_networks: bool
            If True, include an induced `Network` object per component.
        out_node_component_attribute: str | None
            Optional node attribute name to store component ids.

        Returns:
        --------
        list[dict]
            Each record has: component_id, size, node_indices, edge_indices,
            and optionally network.
        """
        minimum = max(1, int(min_size))
        measured = self._core.measure_connected_components(mode=mode)
        values = measured["values_by_node"]
        active_nodes = self._core.node_indices()

        if out_node_component_attribute:
            node_attributes = self._core.list_attributes(_core.SCOPE_NODE)
            if out_node_component_attribute not in node_attributes:
                self.nodes.define_attribute(out_node_component_attribute, _core.ATTR_UNSIGNED_INTEGER, 1)
            else:
                info = self._core.attribute_info(_core.SCOPE_NODE, out_node_component_attribute)
                if int(info["type"]) != int(_core.ATTR_UNSIGNED_INTEGER):
                    raise TypeError(f'Node attribute "{out_node_component_attribute}" must be UnsignedInteger')
            for node_id in active_nodes:
                self._core.set_attribute_value(
                    _core.SCOPE_NODE,
                    out_node_component_attribute,
                    int(node_id),
                    int(values[int(node_id)]),
                )

        grouped_nodes = {}
        for node_id in active_nodes:
            component_id = int(values[int(node_id)])
            if component_id <= 0:
                continue
            grouped_nodes.setdefault(component_id, []).append(int(node_id))

        grouped_edges = {component_id: [] for component_id in grouped_nodes}
        for edge_id, (source, target) in self._core.edges_with_indices():
            component_id = int(values[int(source)])
            if component_id <= 0 or component_id != int(values[int(target)]):
                continue
            bucket = grouped_edges.get(component_id)
            if bucket is not None:
                bucket.append(int(edge_id))

        components = []
        for component_id, node_ids in grouped_nodes.items():
            if len(node_ids) < minimum:
                continue
            edge_ids = grouped_edges.get(component_id, [])
            record = {
                "component_id": int(component_id),
                "size": len(node_ids),
                "node_indices": list(node_ids),
                "edge_indices": list(edge_ids),
            }
            if as_networks:
                record["network"] = self._extract_component_network(node_ids, edge_ids)
            components.append(record)

        components.sort(key=lambda item: (-int(item["size"]), int(item["component_id"])))
        return components

    def extract_largest_connected_component(
        self,
        mode="weak",
        as_network: bool = True,
        out_node_component_attribute: str | None = "component",
    ):
        """
        Extract the largest connected component.
        """
        components = self.extract_connected_components(
            mode=mode,
            min_size=1,
            as_networks=bool(as_network),
            out_node_component_attribute=out_node_component_attribute,
        )
        if not components:
            return None
        return components[0]

    def measure_leiden_modularity(
        self,
        edge_weight_attribute: str | None = None,
        resolution: float = 1.0,
        seed: int = 0,
        max_levels: int = 32,
        max_passes: int = 8,
        out_node_community_attribute: str = "community",
    ):
        """
        Run Leiden community detection optimizing modularity.

        Parameters:
        -----------
        edge_weight_attribute: str | None
            Optional edge attribute name with scalar weights.
        resolution: float
            Modularity resolution parameter (gamma).
        seed: int
            Random seed. Zero uses the native default seed.
        max_levels: int
            Maximum aggregation levels.
        max_passes: int
            Maximum local-moving passes per phase.
        out_node_community_attribute: str
            Node attribute name to store detected community ids.

        Returns:
        --------
        dict
            Contains community_count, modularity, values_by_node, and options.
        """
        existed = out_node_community_attribute in self._core.list_attributes(_core.SCOPE_NODE)
        result = self._core.measure_leiden_modularity(
            edge_weight_attribute=edge_weight_attribute,
            resolution=float(resolution),
            seed=int(seed),
            max_levels=int(max_levels),
            max_passes=int(max_passes),
            out_node_community_attribute=out_node_community_attribute,
        )
        if not existed:
            self._emit_attribute_defined(
                _core.SCOPE_NODE,
                out_node_community_attribute,
                _core.ATTR_UNSIGNED_INTEGER,
                1,
            )
        active_nodes = self._core.node_indices()
        self._emit_attribute_changed(
            _core.SCOPE_NODE,
            out_node_community_attribute,
            active_nodes,
            [result["values_by_node"][int(node_id)] for node_id in active_nodes],
        )
        return result

    def leiden_modularity(self, **kwargs):
        """Alias for :meth:`measure_leiden_modularity`."""
        return self.measure_leiden_modularity(**kwargs)

    def label_major_components(
        self,
        mode="weak",
        max_components: int | None = None,
        min_size: int = 1,
        out_node_component_attribute: str = "major_component",
        other_label: int = 0,
    ):
        """
        Label the largest connected components with compact rank ids.

        The largest selected component receives label 1, the second largest
        receives label 2, and so on. Nodes outside the selected components get
        `other_label`.

        Parameters:
        -----------
        mode: str | int
            Connected-components mode ("weak" or "strong").
        max_components: int | None
            Maximum number of largest components to label. None labels all
            components that satisfy `min_size`.
        min_size: int
            Minimum component size to receive a positive rank label.
        out_node_component_attribute: str
            Node attribute name to store rank labels.
        other_label: int
            Label assigned to nodes outside selected major components.

        Returns:
        --------
        dict
            Summary with selected component records and values_by_node.
        """
        if max_components is not None and int(max_components) < 1:
            raise ValueError("max_components must be at least 1 or None")
        if int(min_size) < 1:
            raise ValueError("min_size must be at least 1")
        if not out_node_component_attribute:
            raise ValueError("out_node_component_attribute is required")

        measured = self._core.measure_connected_components(mode=mode)
        component_values = measured["values_by_node"]
        active_nodes = [int(node_id) for node_id in self._core.node_indices()]

        component_sizes = {}
        for node_id in active_nodes:
            component_id = int(component_values[node_id])
            if component_id <= 0:
                continue
            component_sizes[component_id] = component_sizes.get(component_id, 0) + 1

        selected = [
            {"component_id": component_id, "size": size}
            for component_id, size in component_sizes.items()
            if size >= int(min_size)
        ]
        selected.sort(key=lambda item: (-int(item["size"]), int(item["component_id"])))
        if max_components is not None:
            selected = selected[:int(max_components)]

        rank_by_component = {
            int(component["component_id"]): rank
            for rank, component in enumerate(selected, start=1)
        }
        values = [int(other_label)] * len(component_values)
        for node_id in active_nodes:
            component_id = int(component_values[node_id])
            values[node_id] = int(rank_by_component.get(component_id, other_label))

        node_attributes = self._core.list_attributes(_core.SCOPE_NODE)
        if out_node_component_attribute not in node_attributes:
            self.nodes.define_attribute(out_node_component_attribute, _core.ATTR_UNSIGNED_INTEGER, 1)
        else:
            info = self._core.attribute_info(_core.SCOPE_NODE, out_node_component_attribute)
            if int(info["type"]) != int(_core.ATTR_UNSIGNED_INTEGER):
                raise TypeError(f'Node attribute "{out_node_component_attribute}" must be UnsignedInteger')

        for node_id in active_nodes:
            self._core.set_attribute_value(
                _core.SCOPE_NODE,
                out_node_component_attribute,
                node_id,
                int(values[node_id]),
            )
        self._emit_attribute_changed(
            _core.SCOPE_NODE,
            out_node_component_attribute,
            active_nodes,
            [values[node_id] for node_id in active_nodes],
        )

        return {
            "values_by_node": values,
            "selected_components": selected,
            "component_count": measured["component_count"],
            "largest_component_size": measured["largest_component_size"],
            "mode": measured["mode"],
            "out_node_component_attribute": out_node_component_attribute,
            "other_label": int(other_label),
        }

    def out_neighbors(self, node: int):
        """
        Return outgoing neighbors for a node.

        Returns:
        --------
        dict
            {"nodes": [...], "edges": [...]}.
        """
        return self._core.out_neighbors(int(node))

    def in_neighbors(self, node: int):
        """
        Return incoming neighbors for a node.

        Returns:
        --------
        dict
            {"nodes": [...], "edges": [...]}.
        """
        return self._core.in_neighbors(int(node))

    def neighbors(self, source_nodes, direction="both", include_source_nodes: bool = True):
        """
        Return unique one-hop neighbors for source node(s).

        Parameters:
        -----------
        source_nodes: int | Sequence[int]
            One node id or a sequence of node ids.
        direction: str | int
            Neighbor direction ('out', 'in', 'both') or enum value.
        include_source_nodes: bool
            If True, source nodes may appear in the returned node list.
        """
        return self._core.neighbors(
            source_nodes,
            direction=direction,
            include_source_nodes=bool(include_source_nodes),
        )

    def neighbors_at_level(self, source_nodes, level: int, direction="both", include_source_nodes: bool = False):
        """
        Return neighbors at exactly `level` hops from source node(s).
        """
        return self._core.neighbors_at_level(
            source_nodes,
            level=int(level),
            direction=direction,
            include_source_nodes=bool(include_source_nodes),
        )

    def neighbors_up_to_level(self, source_nodes, max_level: int, direction="both", include_source_nodes: bool = False):
        """
        Return neighbors up to (and including) `max_level` hops from source node(s).
        """
        return self._core.neighbors_up_to_level(
            source_nodes,
            max_level=int(max_level),
            direction=direction,
            include_source_nodes=bool(include_source_nodes),
        )

    def select_nodes(self, where_expr: str) -> "NodeSelector":
        """
        Select nodes matching a query expression.

        Parameters:
        -----------
        where_expr: str
            Query expression string.

        Returns:
        --------
        NodeSelector
            Selector with matching node ids.
        """
        if not isinstance(where_expr, str):
            raise TypeError("Query expression must be a string")
        ids = self._core.select_nodes(where_expr)
        return NodeSelector(self, ids)

    def select_edges(self, where_expr: str) -> "EdgeSelector":
        """
        Select edges matching a query expression.

        Parameters:
        -----------
        where_expr: str
            Query expression string.

        Returns:
        --------
        EdgeSelector
            Selector with matching edge ids.
        """
        if not isinstance(where_expr, str):
            raise TypeError("Query expression must be a string")
        ids = self._core.select_edges(where_expr)
        return EdgeSelector(self, ids)

    def apply_text_batch(self, text: str, stop_on_error: bool = True):
        """
        Apply a text batch of stream commands to this network.

        Supported commands (MVP):
        - ADD_NODES n=#
        - ADD_EDGES pairs=[(a,b),...]
        - SET_ATTR_VALUES scope=node|edge name=attr ids=[...] values=[...]

        Relative ids:
        - suffix `! relative [varName]` remaps ids against a prior result set.
        - result sets are captured as `varName = ADD_NODES ...` or `varName = ADD_EDGES ...`.
        """
        if not isinstance(text, str):
            raise TypeError("Batch text must be a string")
        results = []
        variables = {}
        last_added_nodes = None
        last_added_edges = None

        for line_index, raw_line in enumerate(text.splitlines(), start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            result = {"line": line_index, "ok": False, "op": None}
            try:
                relative = False
                relative_name = None
                if "! relative" in line:
                    prefix, suffix = line.split("! relative", 1)
                    relative = True
                    relative_name = suffix.strip().split()[0] if suffix.strip() else None
                    line = prefix.strip()

                var_name = None
                if "=" in line:
                    left, right = line.split("=", 1)
                    if left.strip().replace("_", "").isalnum():
                        var_name = left.strip()
                        line = right.strip()

                tokens = _split_command_tokens(line)
                if not tokens:
                    raise ValueError("Empty command")
                op = tokens[0]
                result["op"] = op
                args = _parse_key_value_args(" ".join(tokens[1:]))

                if op == "DEFINE_ATTRIBUTE":
                    scope = _coerce_scope(args.get("scope"))
                    name = args.get("name")
                    attr_type = _coerce_attribute_type(args.get("type"))
                    dimension = int(args.get("dimension") or 1)
                    if not name:
                        raise ValueError("DEFINE_ATTRIBUTE requires name")
                    self.define_attribute(scope, name, attr_type, dimension)
                    result["ok"] = True
                elif op == "ADD_NODES":
                    count = int(args.get("n") or args.get("count") or 0)
                    if count < 0:
                        raise ValueError("ADD_NODES requires n=<count>")
                    ids = self.add_nodes(count)
                    last_added_nodes = ids
                    variables["added_node_ids"] = ids
                    if var_name:
                        variables[var_name] = ids
                    result["ok"] = True
                    result["result"] = ids
                elif op == "ADD_EDGES":
                    pairs = _parse_pairs(args.get("pairs", ""))
                    if not pairs:
                        raise ValueError("ADD_EDGES requires pairs=[(a,b),...]")
                    if relative:
                        relative_set = _resolve_relative_set(relative_name, last_added_nodes, variables, "node")
                        pairs = _resolve_pairs_relative(pairs, relative_set)
                    edges = self.add_edges(pairs)
                    last_added_edges = edges
                    variables["added_edge_ids"] = edges
                    if var_name:
                        variables[var_name] = edges
                    result["ok"] = True
                    result["result"] = edges
                elif op == "REMOVE_NODES":
                    ids = [int(value) for value in _parse_number_list(args.get("ids", ""))]
                    self.remove_nodes(ids)
                    result["ok"] = True
                elif op == "REMOVE_EDGES":
                    ids = [int(value) for value in _parse_number_list(args.get("ids", ""))]
                    self.remove_edges(ids)
                    result["ok"] = True
                elif op == "SET_CATEGORY_DICTIONARY":
                    scope = _coerce_scope(args.get("scope"))
                    name = args.get("name")
                    entries, value_kind = _parse_value_list(args.get("entries") or args.get("labels") or "")
                    if not name or value_kind != "string":
                        raise ValueError("SET_CATEGORY_DICTIONARY requires string entries")
                    self.set_category_dictionary(scope, name, entries)
                    result["ok"] = True
                elif op == "SET_ATTR_VALUES":
                    scope = args.get("scope")
                    name = args.get("name")
                    ids = [0] if scope in ("network", "graph") and not args.get("ids") else _parse_number_list(args.get("ids", ""))
                    values, value_kind = _parse_value_list(args.get("values", ""))
                    if not scope or not name or not ids or not values:
                        raise ValueError("SET_ATTR_VALUES requires scope, name, ids, and values")
                    if relative:
                        relative_set = _resolve_relative_set(
                            relative_name,
                            last_added_edges if scope == "edge" else last_added_nodes,
                            variables,
                            scope,
                        )
                        ids = _resolve_ids_relative(ids, relative_set)
                    scope_id = _coerce_scope(scope)
                    info = self._core.attribute_info(scope_id, name)
                    dimension = int(info.get("dimension") or 1)
                    if len(values) == 1:
                        values = [values[0] for _ in ids]
                    elif dimension > 1 and len(values) == len(ids) * dimension:
                        values = [values[i * dimension:(i + 1) * dimension] for i in range(len(ids))]
                    elif len(values) != len(ids):
                        raise ValueError("SET_ATTR_VALUES values length must be 1, match ids, or match ids * dimension")
                    elif dimension > 1:
                        raise ValueError("Vector text batches require flattened values matching ids * dimension")
                    attr_type = info.get("type")
                    if value_kind == "string" and attr_type == _core.ATTR_STRING:
                        for idx, value in zip(ids, values):
                            self.set_attribute_value(scope_id, name, int(idx), value)
                    elif value_kind == "string" and attr_type == _core.ATTR_CATEGORY:
                        mapping = self._core.get_category_dictionary(scope_id, name)
                        for idx, label in zip(ids, values):
                            if label not in mapping:
                                raise ValueError(f"Category label '{label}' not found")
                            self.set_attribute_value(scope_id, name, int(idx), int(mapping[label]))
                    elif value_kind == "number":
                        for idx, value in zip(ids, values):
                            self.set_attribute_value(scope_id, name, int(idx), value)
                    else:
                        raise ValueError("Values are incompatible with attribute type")
                    result["ok"] = True
                else:
                    raise ValueError(f"Unsupported command: {op}")
            except Exception as exc:
                result["error"] = str(exc)
                results.append(result)
                if stop_on_error:
                    return {"results": results, "variables": variables}
                continue
            results.append(result)
        return {"results": results, "variables": variables}

    def apply_binary_batch(self, data, stop_on_error: bool = True):
        """Apply the Helios binary mutation batch protocol to this network."""
        payload = bytes(data)
        if payload[:4] != b"HNPB":
            raise ValueError("Invalid batch magic")
        version, _flags, _reserved, record_count = struct.unpack_from("<BBHI", payload, 4)
        if version != 1:
            raise ValueError(f"Unsupported batch version {version}")
        offset = 12
        results = []
        slots = {}
        for record_index in range(record_count):
            result = {"record": record_index, "ok": False}
            try:
                op, flags, _reserved, result_slot, payload_len = struct.unpack_from("<BBHII", payload, offset)
                offset += 12
                payload_start = offset
                relative = bool(flags & 0x1)
                if op == 1:
                    (count,) = struct.unpack_from("<I", payload, offset)
                    ids = self.add_nodes(count)
                    if result_slot:
                        slots[result_slot] = ids
                    result["ok"] = True
                    result["result"] = ids
                elif op == 2:
                    pair_count, base_slot = struct.unpack_from("<II", payload, offset)
                    offset += 8
                    base = slots.get(base_slot) if relative else None
                    if relative and base is None:
                        raise ValueError("Missing base slot for relative edge pairs")
                    pairs = []
                    for _ in range(pair_count):
                        a, b = struct.unpack_from("<II", payload, offset)
                        offset += 8
                        pairs.append((base[a], base[b]) if relative else (a, b))
                    edges = self.add_edges(pairs)
                    if result_slot:
                        slots[result_slot] = edges
                    result["ok"] = True
                    result["result"] = edges
                elif op == 3:
                    scope_id, value_type, _reserved = struct.unpack_from("<BBH", payload, offset)
                    offset += 4
                    id_count, value_count, base_slot, name_len = struct.unpack_from("<IIII", payload, offset)
                    offset += 16
                    name = payload[offset:offset + name_len].decode("utf-8")
                    offset += name_len
                    ids = list(struct.unpack_from(f"<{id_count}I", payload, offset)) if id_count else []
                    offset += id_count * 4
                    values = []
                    if value_type == 0:
                        values = list(struct.unpack_from(f"<{value_count}d", payload, offset)) if value_count else []
                        offset += value_count * 8
                    elif value_type == 1:
                        for _ in range(value_count):
                            (length,) = struct.unpack_from("<I", payload, offset)
                            offset += 4
                            values.append(payload[offset:offset + length].decode("utf-8"))
                            offset += length
                    else:
                        raise ValueError("Unsupported value type")
                    base = slots.get(base_slot) if relative else None
                    if relative and base is None:
                        raise ValueError("Missing base slot for relative ids")
                    resolved_ids = [base[index] for index in ids] if relative else ids
                    scope = _scope_name(scope_id)
                    text = (
                        "SET_ATTR_VALUES "
                        f"scope={scope} name={name} ids={_format_list(resolved_ids)} values={_format_list(values)}"
                    )
                    applied = self.apply_text_batch(text, stop_on_error=True)
                    if not all(entry["ok"] for entry in applied["results"]):
                        raise ValueError(applied["results"][0].get("error", "Failed to apply attribute values"))
                    result["ok"] = True
                elif op in (4, 5):
                    (id_count,) = struct.unpack_from("<I", payload, offset)
                    offset += 4
                    ids = list(struct.unpack_from(f"<{id_count}I", payload, offset)) if id_count else []
                    offset += id_count * 4
                    self.remove_nodes(ids) if op == 4 else self.remove_edges(ids)
                    result["ok"] = True
                elif op == 6:
                    scope_id, attr_type, _reserved = struct.unpack_from("<BBH", payload, offset)
                    offset += 4
                    dimension, name_len = struct.unpack_from("<II", payload, offset)
                    offset += 8
                    name = payload[offset:offset + name_len].decode("utf-8")
                    offset += name_len
                    self.define_attribute(scope_id, name, attr_type, dimension)
                    result["ok"] = True
                elif op == 7:
                    scope_id, _reserved8, _reserved16 = struct.unpack_from("<BBH", payload, offset)
                    offset += 4
                    entry_count, name_len = struct.unpack_from("<II", payload, offset)
                    offset += 8
                    name = payload[offset:offset + name_len].decode("utf-8")
                    offset += name_len
                    labels = []
                    for _ in range(entry_count):
                        (length,) = struct.unpack_from("<I", payload, offset)
                        offset += 4
                        labels.append(payload[offset:offset + length].decode("utf-8"))
                        offset += length
                    self.set_category_dictionary(scope_id, name, labels)
                    result["ok"] = True
                else:
                    raise ValueError(f"Unsupported op {op}")
                offset = payload_start + payload_len
            except Exception as exc:
                result["error"] = str(exc)
                results.append(result)
                if stop_on_error:
                    return {"results": results, "slots": slots}
                offset = payload_start + payload_len
                continue
            results.append(result)
        return {"results": results, "slots": slots}


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
        self._network._emit_attribute_changed(_core.SCOPE_NODE, name, [self.id], [value])

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
        self._network._emit_attribute_changed(_core.SCOPE_EDGE, name, [self.id], [value])

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
        self._network._emit_attribute_changed(_core.SCOPE_NETWORK, name, [0], [value])

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
        self._network._emit_attribute_changed(_core.SCOPE_NODE, name, self._ids, values)

    def neighbors(self, direction="both", include_source_nodes: bool = True):
        """
        Return one-hop neighbors for the selected nodes.

        Parameters:
        -----------
        direction: str | int
            Neighbor direction ('out', 'in', 'both') or enum value.
        include_source_nodes: bool
            If True, selected source nodes may appear in the result.
        """
        return self._network.neighbors(self._ids, direction=direction, include_source_nodes=include_source_nodes)

    def neighbors_at_level(self, level: int, direction="both", include_source_nodes: bool = False):
        """
        Return neighbors at an exact hop distance from the selected nodes.
        """
        return self._network.neighbors_at_level(
            self._ids,
            level=level,
            direction=direction,
            include_source_nodes=include_source_nodes,
        )

    def neighbors_up_to_level(self, max_level: int, direction="both", include_source_nodes: bool = False):
        """
        Return neighbors up to (and including) a hop distance from the selected nodes.
        """
        return self._network.neighbors_up_to_level(
            self._ids,
            max_level=max_level,
            direction=direction,
            include_source_nodes=include_source_nodes,
        )

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
        self._network._emit_attribute_changed(_core.SCOPE_EDGE, name, self._ids, values)

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
        self._network._emit_attribute_changed(_core.SCOPE_NODE, key, ids, values)

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
        self._network._emit_attribute_changed(_core.SCOPE_EDGE, key, ids, values)

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
        self._network._emit_attribute_changed(_core.SCOPE_NETWORK, key, [0], values)

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
    result = network._core.define_attribute(scope, attr_name, attr_type_id, dimension)
    if hasattr(network, "_emit_attribute_defined"):
        network._emit_attribute_defined(scope, attr_name, attr_type_id, dimension)
    return result


def _coerce_attribute_type(value):
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        text = value.strip()
        if text.isdigit():
            return int(text)
        for type_id, type_name in _ATTRIBUTE_TYPE_NAMES.items():
            if text.lower() == type_name.lower():
                return type_id
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


def _split_command_tokens(line: str) -> List[str]:
    tokens = []
    current = []
    depth = 0
    in_quote = False
    for ch in line:
        if ch == '"':
            in_quote = not in_quote
            current.append(ch)
            continue
        if not in_quote:
            if ch in "[(":
                depth += 1
            elif ch in "])":
                depth = max(0, depth - 1)
            elif ch.isspace() and depth == 0:
                if current:
                    tokens.append("".join(current))
                    current = []
                continue
        current.append(ch)
    if current:
        tokens.append("".join(current))
    return tokens


def _parse_key_value_args(text: str) -> dict:
    args = {}
    if not text:
        return args
    for token in _split_command_tokens(text.strip()):
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        value = value.strip()
        if value.startswith('"') and value.endswith('"'):
            value = value[1:-1]
        args[key.strip()] = value
    return args


def _parse_number_list(text: str) -> List[float]:
    if not text:
        return []
    value = text.strip()
    if value.startswith("[") and value.endswith("]"):
        value = value[1:-1]
    parts = []
    number = ""
    for ch in value:
        if ch.isdigit() or ch in ".-+eE":
            number += ch
        else:
            if number:
                try:
                    parts.append(float(number))
                except ValueError:
                    pass
                number = ""
    if number:
        try:
            parts.append(float(number))
        except ValueError:
            pass
    return parts


def _parse_value_list(text: str):
    if not text:
        return [], "none"
    value = text.strip()
    if value.startswith("[") and value.endswith("]"):
        value = value[1:-1]
    parts = []
    value_type = None
    buffer = ""
    in_quote = False
    for ch in value:
        if ch == '"':
            in_quote = not in_quote
            if not in_quote and buffer:
                parts.append(buffer)
                value_type = "string" if value_type in (None, "string") else _raise_value_mix()
                buffer = ""
            continue
        if in_quote:
            buffer += ch
            continue
        if ch in ", ":
            if buffer:
                parts.append(_coerce_number(buffer))
                value_type = "number" if value_type in (None, "number") else _raise_value_mix()
                buffer = ""
            continue
        buffer += ch
    if buffer:
        parts.append(_coerce_number(buffer))
        value_type = "number" if value_type in (None, "number") else _raise_value_mix()
    return parts, value_type or "none"


def _coerce_number(text: str):
    try:
        return float(text)
    except ValueError:
        raise ValueError("Value list cannot mix strings and numbers") from None


def _raise_value_mix():
    raise ValueError("Value list cannot mix strings and numbers")


def _coerce_scope(scope):
    if isinstance(scope, str):
        normalized = scope.strip().lower()
        if normalized == "graph":
            normalized = "network"
        if normalized not in _SCOPE_IDS:
            raise ValueError(f"Unknown attribute scope: {scope}")
        return _SCOPE_IDS[normalized]
    return int(scope)


def _scope_name(scope) -> str:
    return _SCOPE_NAMES.get(_coerce_scope(scope), str(scope))


def _jsonable_values(values):
    result = []
    for value in values:
        if hasattr(value, "tolist"):
            value = value.tolist()
        if isinstance(value, tuple):
            value = list(value)
        result.append(value)
    return result


def _public_event_payload(event):
    payload = {
        "type": event["type"],
        "detail": event.get("detail"),
    }
    if event.get("record") is not None:
        payload["record"] = event["record"]
    return payload


def _quote_text_value(value):
    text = str(value)
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t") + '"'


def _format_list(values):
    parts = []
    for value in values:
        if isinstance(value, (list, tuple)):
            parts.extend(value)
        else:
            parts.append(value)
    return "[" + ",".join(_quote_text_value(value) if isinstance(value, str) else str(value) for value in parts) + "]"


def mutation_events_to_text_batch(events) -> str:
    """Convert mutation events to the text batch format understood by JS."""
    lines = []
    for event in events:
        record = event.get("record") or {}
        op = record.get("op")
        if op == "DEFINE_ATTRIBUTE":
            lines.append(
                "DEFINE_ATTRIBUTE "
                f"scope={record['scope']} name={record['name']} "
                f"type={record['type']} dimension={record.get('dimension', 1)}"
            )
        elif op == "ADD_NODES":
            lines.append(f"ADD_NODES n={record.get('count', 0)}")
        elif op == "REMOVE_NODES":
            lines.append(f"REMOVE_NODES ids={_format_list(record.get('ids', []))}")
        elif op == "ADD_EDGES":
            pairs = ",".join(f"({int(source)},{int(target)})" for source, target in record.get("pairs", []))
            lines.append(f"ADD_EDGES pairs=[{pairs}]")
        elif op == "REMOVE_EDGES":
            lines.append(f"REMOVE_EDGES ids={_format_list(record.get('ids', []))}")
        elif op == "SET_ATTR_VALUES":
            ids = record.get("ids", [0])
            values = record.get("values", [])
            lines.append(
                "SET_ATTR_VALUES "
                f"scope={record['scope']} name={record['name']} "
                f"ids={_format_list(ids)} values={_format_list(values)}"
            )
        elif op == "CATEGORY_CHANGE" and record.get("category_op") == "set_dictionary":
            entries = record.get("entries") or []
            if isinstance(entries, dict):
                labels = list(entries.keys())
            else:
                labels = [
                    entry.get("label", entry.get("name", entry.get("value", ""))) if isinstance(entry, dict) else entry
                    for entry in entries
                ]
            lines.append(
                "SET_CATEGORY_DICTIONARY "
                f"scope={record['scope']} name={record['name']} entries={_format_list(labels)}"
            )
    return "\n".join(lines)


def encode_binary_batch(events) -> bytes:
    """Encode mutation events into the Helios network binary batch protocol."""
    records = []
    for event in events:
        record = event.get("record", event)
        payload = _encode_binary_record_payload(record)
        if payload is None:
            continue
        records.append(payload)
    header = bytearray(b"HNPB")
    header += struct.pack("<BBHI", 1, 0, 0, len(records))
    body = bytearray()
    for op, flags, slot, payload in records:
        body += struct.pack("<BBHII", op, flags, 0, slot, len(payload))
        body += payload
    return bytes(header + body)


def _encode_binary_record_payload(record):
    op = record.get("op")
    if op == "ADD_NODES":
        return 1, 0, 0, struct.pack("<I", int(record.get("count", 0)))
    if op == "ADD_EDGES":
        pairs = record.get("pairs", [])
        payload = bytearray(struct.pack("<II", len(pairs), 0))
        for source, target in pairs:
            payload += struct.pack("<II", int(source), int(target))
        return 2, 0, 0, bytes(payload)
    if op == "SET_ATTR_VALUES":
        scope = 1 if record.get("scope") == "edge" else 2 if record.get("scope") == "network" else 0
        values = record.get("values", [])
        flat_values = []
        for value in values:
            if isinstance(value, (list, tuple)):
                flat_values.extend(value)
            else:
                flat_values.append(value)
        value_type = 1 if any(isinstance(value, str) for value in flat_values) else 0
        name = str(record.get("name", "")).encode("utf-8")
        ids = [int(item) for item in record.get("ids", [0])]
        payload = bytearray(struct.pack("<BBHIIII", scope, value_type, 0, len(ids), len(flat_values), 0, len(name)))
        payload += name
        for index in ids:
            payload += struct.pack("<I", index)
        if value_type == 1:
            for value in flat_values:
                chunk = str(value).encode("utf-8")
                payload += struct.pack("<I", len(chunk))
                payload += chunk
        else:
            for value in flat_values:
                payload += struct.pack("<d", float(value))
        return 3, 0, 0, bytes(payload)
    if op == "REMOVE_NODES":
        ids = [int(item) for item in record.get("ids", [])]
        payload = struct.pack("<I", len(ids)) + b"".join(struct.pack("<I", item) for item in ids)
        return 4, 0, 0, payload
    if op == "REMOVE_EDGES":
        ids = [int(item) for item in record.get("ids", [])]
        payload = struct.pack("<I", len(ids)) + b"".join(struct.pack("<I", item) for item in ids)
        return 5, 0, 0, payload
    if op == "DEFINE_ATTRIBUTE":
        scope = 1 if record.get("scope") == "edge" else 2 if record.get("scope") == "network" else 0
        name = str(record.get("name", "")).encode("utf-8")
        payload = struct.pack(
            "<BBHII",
            scope,
            int(record.get("type", _core.ATTR_DOUBLE)),
            0,
            int(record.get("dimension", 1)),
            len(name),
        ) + name
        return 6, 0, 0, payload
    if op == "CATEGORY_CHANGE" and record.get("category_op") == "set_dictionary":
        scope = 1 if record.get("scope") == "edge" else 2 if record.get("scope") == "network" else 0
        name = str(record.get("name", "")).encode("utf-8")
        entries = record.get("entries") or []
        labels = []
        if isinstance(entries, dict):
            labels = [str(label) for label in entries.keys()]
        else:
            for entry in entries:
                if isinstance(entry, dict):
                    labels.append(str(entry.get("label") or entry.get("name") or entry.get("value") or ""))
                else:
                    labels.append(str(entry))
        payload = bytearray(struct.pack("<BBHII", scope, 0, 0, len(labels), len(name)))
        payload += name
        for label in labels:
            chunk = label.encode("utf-8")
            payload += struct.pack("<I", len(chunk))
            payload += chunk
        return 7, 0, 0, bytes(payload)
    return None


def _parse_pairs(text: str) -> List[tuple]:
    if not text:
        return []
    value = text.strip()
    if value.startswith("[") and value.endswith("]"):
        value = value[1:-1]
    pairs = []
    for chunk in value.split(")"):
        if "(" not in chunk:
            continue
        inside = chunk.split("(", 1)[1]
        parts = [p.strip() for p in inside.split(",") if p.strip()]
        if len(parts) != 2:
            continue
        try:
            pairs.append((int(float(parts[0])), int(float(parts[1]))))
        except ValueError:
            continue
    return pairs


def _resolve_relative_set(name: str | None, fallback, variables: dict, scope: str):
    key = "added_edge_ids" if scope == "edge" else "added_node_ids"
    if name:
        result = variables.get(name)
    else:
        result = fallback or variables.get(key)
    if not result:
        raise ValueError("Relative ids require a captured result set")
    return result


def _resolve_ids_relative(ids: List[float], ref_ids: Sequence[int]) -> List[int]:
    resolved = []
    for idx in ids:
        pos = int(idx)
        if pos < 0 or pos >= len(ref_ids):
            raise ValueError("Relative id is out of range")
        resolved.append(int(ref_ids[pos]))
    return resolved


def _resolve_pairs_relative(pairs: List[tuple], ref_ids: Sequence[int]) -> List[tuple]:
    resolved = []
    for a, b in pairs:
        if a < 0 or a >= len(ref_ids) or b < 0 or b >= len(ref_ids):
            raise ValueError("Relative edge endpoint is out of range")
        resolved.append((int(ref_ids[a]), int(ref_ids[b])))
    return resolved


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
