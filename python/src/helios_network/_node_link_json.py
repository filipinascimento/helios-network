from __future__ import annotations

import json
import math
import warnings

from . import _core
from ._wrapper import Network


def _is_plain_object(value) -> bool:
    return isinstance(value, dict)


def _node_link_id_key(value) -> str:
    return f"{type(value).__name__}:{json.dumps(value, ensure_ascii=False, sort_keys=True)}"


def _node_link_id_string(value) -> str:
    if isinstance(value, str):
        return value
    return json.dumps(value, ensure_ascii=False, sort_keys=True)


def _merge_node_link_record_attributes(record, reserved_keys: set[str]) -> dict:
    merged = {}
    if not _is_plain_object(record):
        return merged
    for key, value in record.items():
        if key in reserved_keys or key == "attributes":
            continue
        merged[key] = value
    nested = record.get("attributes")
    if _is_plain_object(nested):
        merged.update(nested)
    return merged


def _classify_node_link_value(value):
    if value is None:
        return None
    if isinstance(value, bool):
        return {"mode": "scalar", "attr_type": _core.ATTR_BOOLEAN, "dimension": 1}
    if isinstance(value, int) and not isinstance(value, bool):
        return {"mode": "scalar", "attr_type": _core.ATTR_INTEGER, "dimension": 1}
    if isinstance(value, float):
        if math.isfinite(value):
            return {"mode": "scalar", "attr_type": _core.ATTR_DOUBLE, "dimension": 1}
        return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}
    if isinstance(value, str):
        return {"mode": "string", "attr_type": _core.ATTR_STRING, "dimension": 1}
    if isinstance(value, list):
        non_null = [item for item in value if item is not None]
        if not non_null:
            return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}
        if all(isinstance(item, bool) for item in non_null):
            return {"mode": "vector", "attr_type": _core.ATTR_BOOLEAN, "dimension": len(value)}
        if all(
            ((isinstance(item, int) and not isinstance(item, bool)) or isinstance(item, float))
            and math.isfinite(float(item))
            for item in non_null
        ):
            wants_double = any(
                isinstance(item, float) and not float(item).is_integer()
                for item in non_null
            )
            attr_type = _core.ATTR_DOUBLE if wants_double else _core.ATTR_INTEGER
            return {"mode": "vector", "attr_type": attr_type, "dimension": len(value)}
        return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}
    return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}


def _merge_node_link_schema(existing, candidate):
    if candidate is None:
        return existing
    if existing is None:
        return dict(candidate)
    if existing["mode"] == "json-string" or candidate["mode"] == "json-string":
        return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}
    if existing["mode"] == candidate["mode"] and existing["dimension"] == candidate["dimension"]:
        if existing["attr_type"] != candidate["attr_type"]:
            if existing["mode"] in {"scalar", "vector"}:
                if existing["attr_type"] == _core.ATTR_BOOLEAN and candidate["attr_type"] == _core.ATTR_BOOLEAN:
                    return dict(existing)
                return {
                    "mode": existing["mode"],
                    "attr_type": _core.ATTR_DOUBLE,
                    "dimension": existing["dimension"],
                }
            return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}
        return dict(existing)
    return {"mode": "json-string", "attr_type": _core.ATTR_STRING, "dimension": 1}


def _infer_node_link_schemas(records, reserved_keys: set[str]):
    schemas = {}
    warnings_out = []
    for record in records:
        attrs = _merge_node_link_record_attributes(record, reserved_keys)
        for name, value in attrs.items():
            candidate = _classify_node_link_value(value)
            merged = _merge_node_link_schema(schemas.get(name), candidate)
            if (
                merged["mode"] == "json-string"
                and schemas.get(name, {}).get("mode") != "json-string"
            ):
                warnings_out.append(
                    f'Node-link JSON coerced attribute "{name}" to a string payload during load'
                )
            schemas[name] = merged
    return schemas, warnings_out


def _normalize_node_record(record, index: int, warnings_out: list[str]):
    if not _is_plain_object(record):
        warnings_out.append(f"Node-link JSON skipped non-object node at position {index}")
        return None
    normalized = dict(record)
    if normalized.get("id") is None:
        normalized["id"] = f"__helios_node_{index}"
        warnings_out.append(f"Node-link JSON synthesized an id for node at position {index}")
    return normalized


def _normalize_link_record(record, index: int, warnings_out: list[str]):
    if not _is_plain_object(record):
        warnings_out.append(f"Node-link JSON skipped non-object link at position {index}")
        return None
    if record.get("source") is None or record.get("target") is None:
        warnings_out.append(
            f"Node-link JSON skipped link at position {index} because source/target is missing"
        )
        return None
    return dict(record)


def _ensure_node_link_endpoints(nodes: list[dict], links: list[dict], warnings_out: list[str]) -> list[dict]:
    records = [dict(node) for node in nodes]
    seen = {_node_link_id_key(node.get("id")) for node in records}
    for link in links:
        for key in ("source", "target"):
            endpoint = link.get(key)
            endpoint_key = _node_link_id_key(endpoint)
            if endpoint_key not in seen:
                seen.add(endpoint_key)
                records.append({"id": endpoint})
                warnings_out.append(
                    f"Node-link JSON synthesized a node for missing endpoint {_node_link_id_string(endpoint)}"
                )
    return records


def _coerce_json_string(value) -> str:
    if isinstance(value, str):
        return value
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _coerce_attribute_value(value, schema):
    mode = schema["mode"]
    attr_type = schema["attr_type"]
    if mode == "string":
        return str(value)
    if mode == "json-string":
        return _coerce_json_string(value)
    if mode == "vector":
        values = list(value)
        result = []
        for component in range(schema["dimension"]):
            item = values[component] if component < len(values) else 0
            if attr_type == _core.ATTR_BOOLEAN:
                result.append(bool(item))
            elif attr_type == _core.ATTR_DOUBLE:
                result.append(float(item) if item is not None else 0.0)
            else:
                result.append(int(item) if item is not None else 0)
        return tuple(result)
    if attr_type == _core.ATTR_BOOLEAN:
        return bool(value)
    if attr_type == _core.ATTR_DOUBLE:
        return float(value)
    return int(value)


def _apply_node_link_attributes(
    network: Network,
    scope: int,
    records: list[dict],
    indices: list[int],
    schemas: dict,
    reserved_keys: set[str],
):
    for name, schema in schemas.items():
        network.define_attribute(scope, name, schema["attr_type"], schema["dimension"])
    for record, index in zip(records, indices):
        attrs = _merge_node_link_record_attributes(record, reserved_keys)
        for name, value in attrs.items():
            if value is None:
                continue
            schema = schemas.get(name)
            if schema is None:
                continue
            network.set_attribute_value(scope, name, int(index), _coerce_attribute_value(value, schema))


def read_node_link_json(path: str) -> Network:
    """
    Read a node-link JSON file into a Network.

    The loader accepts the common `{directed, graph, nodes, links}` layout and
    also accepts `edges` as an alias for `links`. Unsupported nested values are
    stringified with a warning instead of aborting the import.
    """
    with open(path, "r", encoding="utf-8") as handle:
        document = json.load(handle)
    if not _is_plain_object(document):
        raise ValueError("Node-link JSON root must be an object")

    warnings_out: list[str] = []
    node_input = document.get("nodes", [])
    if not isinstance(node_input, list):
        raise ValueError('Node-link JSON "nodes" must be an array when present')
    link_input = document.get("links", document.get("edges", []))
    if not isinstance(link_input, list):
        raise ValueError('Node-link JSON "links" or "edges" must be an array when present')

    normalized_nodes = []
    for index, record in enumerate(node_input):
        normalized = _normalize_node_record(record, index, warnings_out)
        if normalized is not None:
            normalized_nodes.append(normalized)
    normalized_links = []
    for index, record in enumerate(link_input):
        normalized = _normalize_link_record(record, index, warnings_out)
        if normalized is not None:
            normalized_links.append(normalized)

    seen_node_ids = set()
    for record in normalized_nodes:
        key = _node_link_id_key(record["id"])
        if key in seen_node_ids:
            original = _node_link_id_string(record["id"])
            suffix = 1
            replacement = f"{original}#{suffix}"
            while _node_link_id_key(replacement) in seen_node_ids:
                suffix += 1
                replacement = f"{original}#{suffix}"
            record["id"] = replacement
            warnings_out.append(f"Node-link JSON renamed duplicate node id {original} to {replacement}")
        seen_node_ids.add(_node_link_id_key(record["id"]))

    node_records = _ensure_node_link_endpoints(normalized_nodes, normalized_links, warnings_out)
    graph_record = (
        document.get("graph")
        if _is_plain_object(document.get("graph"))
        else document.get("network")
        if _is_plain_object(document.get("network"))
        else {}
    )
    if "graph" in document and not _is_plain_object(document.get("graph")):
        warnings_out.append('Node-link JSON ignored top-level "graph" because it is not an object')
    if "network" in document and not _is_plain_object(document.get("network")):
        warnings_out.append('Node-link JSON ignored top-level "network" because it is not an object')

    network = Network(directed=bool(document.get("directed")))
    node_ids = network.add_nodes(len(node_records))
    id_to_node = {
        _node_link_id_key(record["id"]): int(node_id)
        for record, node_id in zip(node_records, node_ids)
    }
    edge_pairs = []
    for record in normalized_links:
        source = id_to_node.get(_node_link_id_key(record.get("source")))
        target = id_to_node.get(_node_link_id_key(record.get("target")))
        if source is None or target is None:
            warnings_out.append(
                "Node-link JSON skipped a link because endpoint lookup failed "
                f"({_node_link_id_string(record.get('source'))} -> {_node_link_id_string(record.get('target'))})"
            )
            continue
        edge_pairs.append((source, target))
    edge_ids = network.add_edges(edge_pairs) if edge_pairs else []

    node_schemas, node_warnings = _infer_node_link_schemas(node_records, {"id"})
    edge_schemas, edge_warnings = _infer_node_link_schemas(normalized_links, {"source", "target"})
    graph_schemas, graph_warnings = _infer_node_link_schemas([graph_record], set())
    warnings_out.extend(node_warnings)
    warnings_out.extend(edge_warnings)
    warnings_out.extend(graph_warnings)

    if "_original_ids_" in node_schemas:
        del node_schemas["_original_ids_"]
        warnings_out.append(
            'Node-link JSON reserved "_original_ids_" during load and replaced the input payload with imported node ids'
        )

    _apply_node_link_attributes(network, _core.SCOPE_NODE, node_records, node_ids, node_schemas, {"id"})
    _apply_node_link_attributes(network, _core.SCOPE_EDGE, normalized_links, edge_ids, edge_schemas, {"source", "target"})
    _apply_node_link_attributes(network, _core.SCOPE_NETWORK, [graph_record], [0], graph_schemas, set())

    if node_records:
        network.define_attribute(_core.SCOPE_NODE, "_original_ids_", _core.ATTR_STRING, 1)
        for node_id, record in zip(node_ids, node_records):
            network.set_attribute_value(
                _core.SCOPE_NODE,
                "_original_ids_",
                int(node_id),
                _node_link_id_string(record["id"]),
            )

    for message in warnings_out:
        warnings.warn(message, UserWarning, stacklevel=2)
    return network
