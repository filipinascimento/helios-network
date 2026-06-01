from __future__ import annotations

import enum

from . import _core
from ._conversions import from_igraph, from_networkx, to_igraph, to_networkx
from ._node_link_json import read_node_link_json as _read_node_link_json
from ._wrapper import NETWORK_EVENTS, Network, encode_binary_batch, mutation_events_to_text_batch


class AttributeType(enum.IntEnum):
    """Attribute storage types supported by the Helios native core."""

    String = _core.ATTR_STRING
    Boolean = _core.ATTR_BOOLEAN
    Float = _core.ATTR_FLOAT
    Integer = _core.ATTR_INTEGER
    UnsignedInteger = _core.ATTR_UNSIGNED_INTEGER
    Double = _core.ATTR_DOUBLE
    Category = _core.ATTR_CATEGORY
    Data = _core.ATTR_DATA
    Javascript = _core.ATTR_JAVASCRIPT
    BigInteger = _core.ATTR_BIG_INTEGER
    UnsignedBigInteger = _core.ATTR_UNSIGNED_BIG_INTEGER
    MultiCategory = _core.ATTR_MULTI_CATEGORY
    Unknown = _core.ATTR_UNKNOWN


class AttributeScope(enum.IntEnum):
    """Scopes where attributes can be attached: nodes, edges, or graph-level metadata."""

    Node = _core.SCOPE_NODE
    Edge = _core.SCOPE_EDGE
    Network = _core.SCOPE_NETWORK


class CategorySortOrder(enum.IntEnum):
    """Ordering policies for category dictionaries derived from string attributes."""

    None_ = _core.CATEGORY_SORT_NONE
    Frequency = _core.CATEGORY_SORT_FREQUENCY
    Alphabetical = _core.CATEGORY_SORT_ALPHABETICAL
    Natural = _core.CATEGORY_SORT_NATURAL


class DimensionMethod(enum.IntEnum):
    """Finite-difference method used by multiscale dimension measurements."""

    Forward = _core.DIMENSION_METHOD_FORWARD
    Backward = _core.DIMENSION_METHOD_BACKWARD
    Central = _core.DIMENSION_METHOD_CENTRAL
    LeastSquares = _core.DIMENSION_METHOD_LEAST_SQUARES


class NeighborDirection(enum.IntEnum):
    """Traversal direction for neighbor queries and directed graph measurements."""

    Out = _core.NEIGHBOR_DIRECTION_OUT
    In_ = _core.NEIGHBOR_DIRECTION_IN
    Both = _core.NEIGHBOR_DIRECTION_BOTH


class StrengthMeasure(enum.IntEnum):
    """Edge-weight aggregation used by strength measurements."""

    Sum = _core.STRENGTH_MEASURE_SUM
    Average = _core.STRENGTH_MEASURE_AVERAGE
    Maximum = _core.STRENGTH_MEASURE_MAXIMUM
    Minimum = _core.STRENGTH_MEASURE_MINIMUM


class ClusteringVariant(enum.IntEnum):
    """Formula selector for local clustering coefficient measurements."""

    Unweighted = _core.CLUSTERING_VARIANT_UNWEIGHTED
    Onnela = _core.CLUSTERING_VARIANT_ONNELA
    Newman = _core.CLUSTERING_VARIANT_NEWMAN


class MeasurementExecutionMode(enum.IntEnum):
    """Execution policy for native measurements that support serial or parallel kernels."""

    Auto = _core.MEASUREMENT_EXECUTION_AUTO
    SingleThread = _core.MEASUREMENT_EXECUTION_SINGLE_THREAD
    Parallel = _core.MEASUREMENT_EXECUTION_PARALLEL

class ConnectedComponentsMode(enum.IntEnum):
    """Weak or strong connected-component mode."""

    Weak = _core.CONNECTED_COMPONENTS_WEAK
    Strong = _core.CONNECTED_COMPONENTS_STRONG


def read_bxnet(path: str) -> Network:
    """Read a binary `.bxnet` file into a `Network`."""

    return Network(_core_network=_core.read_bxnet(path))


def read_zxnet(path: str) -> Network:
    """Read a compressed `.zxnet` file into a `Network`."""

    return Network(_core_network=_core.read_zxnet(path))


def read_xnet(path: str) -> Network:
    """Read a text `.xnet` file into a `Network`."""

    return Network(_core_network=_core.read_xnet(path))

def read_gml(path: str) -> Network:
    """Read a GML graph file into a `Network`."""

    return Network(_core_network=_core.read_gml(path))


def read_node_link_json(path: str) -> Network:
    """Read a D3/NetworkX-style node-link JSON file into a `Network`."""

    return _read_node_link_json(path)


def __getattr__(name: str):
    if name in {"HeliosUMAP", "NetworkExportResult"}:
        from .umap import HeliosUMAP, NetworkExportResult

        exports = {
            "HeliosUMAP": HeliosUMAP,
            "NetworkExportResult": NetworkExportResult,
        }
        return exports[name]
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")


__all__ = [
    "Network",
    "NETWORK_EVENTS",
    "AttributeType",
    "AttributeScope",
    "CategorySortOrder",
    "DimensionMethod",
    "NeighborDirection",
    "StrengthMeasure",
    "ClusteringVariant",
    "MeasurementExecutionMode",
    "ConnectedComponentsMode",
    "read_bxnet",
    "read_zxnet",
    "read_xnet",
    "read_gml",
    "read_node_link_json",
    "HeliosUMAP",
    "NetworkExportResult",
    "to_networkx",
    "from_networkx",
    "to_igraph",
    "from_igraph",
    "encode_binary_batch",
    "mutation_events_to_text_batch",
]
