from __future__ import annotations

import enum

from . import _core
from ._conversions import from_igraph, from_networkx, to_igraph, to_networkx
from ._wrapper import Network


class AttributeType(enum.IntEnum):
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
    Node = _core.SCOPE_NODE
    Edge = _core.SCOPE_EDGE
    Network = _core.SCOPE_NETWORK


class DenseColorEncodingFormat(enum.IntEnum):
    Uint8x4 = _core.DENSE_COLOR_U8X4
    Uint32x4 = _core.DENSE_COLOR_U32X4


class CategorySortOrder(enum.IntEnum):
    None_ = _core.CATEGORY_SORT_NONE
    Frequency = _core.CATEGORY_SORT_FREQUENCY
    Alphabetical = _core.CATEGORY_SORT_ALPHABETICAL
    Natural = _core.CATEGORY_SORT_NATURAL


class DimensionMethod(enum.IntEnum):
    Forward = _core.DIMENSION_METHOD_FORWARD
    Backward = _core.DIMENSION_METHOD_BACKWARD
    Central = _core.DIMENSION_METHOD_CENTRAL
    LeastSquares = _core.DIMENSION_METHOD_LEAST_SQUARES


def read_bxnet(path: str) -> Network:
    return Network(_core_network=_core.read_bxnet(path))


def read_zxnet(path: str) -> Network:
    return Network(_core_network=_core.read_zxnet(path))


def read_xnet(path: str) -> Network:
    return Network(_core_network=_core.read_xnet(path))


__all__ = [
    "Network",
    "AttributeType",
    "AttributeScope",
    "DenseColorEncodingFormat",
    "CategorySortOrder",
    "DimensionMethod",
    "read_bxnet",
    "read_zxnet",
    "read_xnet",
    "to_networkx",
    "from_networkx",
    "to_igraph",
    "from_igraph",
]
