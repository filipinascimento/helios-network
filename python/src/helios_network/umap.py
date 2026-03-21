from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np
from scipy import sparse
from scipy.spatial.distance import cdist
from sklearn.utils import check_random_state

from . import AttributeScope, AttributeType
from ._wrapper import Network


DEFAULT_UMAP_EDGE_WEIGHT_ATTRIBUTE = "umap_weight"
DEFAULT_UMAP_NODE_MASS_ATTRIBUTE = "umap_mass"
DEFAULT_UMAP_EMBEDDING_ATTRIBUTE = "umap_embedding"
DEFAULT_POSITION_ATTRIBUTE = "_helios_visuals_position"
DEFAULT_QUERY_MASK_ATTRIBUTE = "umap_is_query"
DEFAULT_INDEX_ATTRIBUTE = "umap_index"


_NUMERIC_DTYPES = {
    AttributeType.Boolean: np.uint8,
    AttributeType.Float: np.float32,
    AttributeType.Integer: np.int32,
    AttributeType.UnsignedInteger: np.uint32,
    AttributeType.Double: np.float64,
}


def _lazy_import_umap():
    try:
        import umap  # type: ignore
    except Exception as exc:  # pragma: no cover - optional dependency path
        raise ImportError(
            "umap-learn is required for helios_network.umap. Install it with "
            "`python -m pip install umap-learn`."
        ) from exc
    return umap


def _lazy_import_pynndescent():
    try:
        import pynndescent  # type: ignore
    except Exception as exc:  # pragma: no cover - optional dependency path
        raise ImportError(
            "pynndescent is not installed. Install it with "
            "`python -m pip install pynndescent` to enable approximate kNN export."
        ) from exc
    return pynndescent


def _metric_name(metric: Any) -> str:
    if isinstance(metric, str):
        return metric
    if callable(metric):
        return getattr(metric, "__name__", metric.__class__.__name__)
    return str(metric)


def _to_csr_matrix(graph: Any) -> sparse.csr_matrix:
    matrix = sparse.csr_matrix(graph)
    matrix.eliminate_zeros()
    return matrix


def _collapse_fit_graph(graph: Any) -> sparse.csr_matrix:
    matrix = _to_csr_matrix(graph)
    if matrix.shape[0] != matrix.shape[1]:
        raise ValueError("Expected a square fuzzy simplicial graph for fit() export")
    matrix = matrix.maximum(matrix.T).tocsr()
    matrix.setdiag(0)
    matrix.eliminate_zeros()
    return sparse.triu(matrix, k=1).tocsr()


def _extract_sparse_edges(graph: sparse.spmatrix) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    coo = graph.tocoo()
    return (
        np.asarray(coo.row, dtype=np.int64),
        np.asarray(coo.col, dtype=np.int64),
        np.asarray(coo.data, dtype=np.float32),
    )


def _as_2d_float32(array: Any) -> np.ndarray:
    values = np.asarray(array, dtype=np.float32)
    if values.ndim != 2:
        raise ValueError("Expected a 2D array")
    return np.ascontiguousarray(values)


def _positions_from_embedding(embedding: Any) -> np.ndarray:
    coords = _as_2d_float32(embedding)
    positions = np.zeros((coords.shape[0], 3), dtype=np.float32)
    copy_dims = min(coords.shape[1], 3)
    positions[:, :copy_dims] = coords[:, :copy_dims]
    return positions


def _compute_mass_from_graph(graph: sparse.spmatrix) -> np.ndarray:
    matrix = _to_csr_matrix(graph)
    return np.asarray(matrix.sum(axis=1)).reshape(-1).astype(np.float32, copy=False)


def _resolve_umap_n_epochs(model: Any, node_count: int) -> int:
    configured = getattr(model, "n_epochs", None)
    if configured is None:
        return 500 if int(node_count) <= 10000 else 200
    if isinstance(configured, (list, tuple, np.ndarray)):
        values = [int(value) for value in configured if int(value) >= 0]
        if values:
            return max(values)
        return 500 if int(node_count) <= 10000 else 200
    value = int(configured)
    return value if value > 0 else (500 if int(node_count) <= 10000 else 200)


def _write_numeric_attribute(
    network: Network,
    scope: AttributeScope,
    name: str,
    values: Any,
    attr_type: AttributeType,
) -> None:
    dtype = _NUMERIC_DTYPES[attr_type]
    array = np.asarray(values, dtype=dtype)
    if array.ndim == 1:
        dimension = 1
    elif array.ndim == 2:
        dimension = int(array.shape[1])
    else:
        raise ValueError(f'Attribute "{name}" must be a 1D or 2D numeric array')

    flat = np.ascontiguousarray(array.reshape(-1))
    network.define_attribute(scope, name, attr_type, dimension)
    buffer = network.attribute_buffer(scope, name)
    if buffer is None:
        raise RuntimeError(f'Attribute "{name}" did not expose a writable buffer')
    target = np.frombuffer(buffer, dtype=dtype)
    if target.size < flat.size:
        raise RuntimeError(f'Attribute "{name}" buffer is too small for the provided values')
    target[: flat.size] = flat


def _set_network_value(network: Network, name: str, value: Any) -> None:
    network[name] = value


def _add_sparse_edges(network: Network, sources: np.ndarray, targets: np.ndarray) -> None:
    edge_pairs = [(int(source), int(target)) for source, target in zip(sources.tolist(), targets.tolist())]
    if edge_pairs:
        network.add_edges(edge_pairs)


def _extract_knn_arrays(model: Any, data: np.ndarray, *, prefer_pynndescent: bool) -> tuple[np.ndarray, np.ndarray]:
    indices = getattr(model, "_knn_indices", None)
    distances = getattr(model, "_knn_dists", None)
    if indices is None:
        indices = getattr(model, "knn_indices", None)
    if distances is None:
        distances = getattr(model, "knn_dists", None)
    if indices is not None and distances is not None:
        return (
            np.asarray(indices, dtype=np.int32),
            np.asarray(distances, dtype=np.float32),
        )

    n_neighbors = int(getattr(model, "n_neighbors", 15))
    metric = getattr(model, "metric", "euclidean")
    if prefer_pynndescent and data.shape[0] >= 4096:
        pynndescent = _lazy_import_pynndescent()
        index = pynndescent.NNDescent(
            data,
            n_neighbors=n_neighbors,
            metric=metric,
            random_state=getattr(model, "random_state", None),
        )
        indices, distances = index.neighbor_graph
        return (
            np.asarray(indices, dtype=np.int32),
            np.asarray(distances, dtype=np.float32),
        )

    if not isinstance(metric, str):
        raise ValueError("kNN export fallback only supports string metrics when precomputed neighbors are unavailable")

    all_distances = cdist(data, data, metric=metric)
    np.fill_diagonal(all_distances, np.inf)
    order = np.argpartition(all_distances, kth=max(0, n_neighbors - 1), axis=1)[:, :n_neighbors]
    row_indices = np.arange(data.shape[0])[:, None]
    ordered_distances = all_distances[row_indices, order]
    sort_order = np.argsort(ordered_distances, axis=1)
    return (
        order[row_indices, sort_order].astype(np.int32, copy=False),
        ordered_distances[row_indices, sort_order].astype(np.float32, copy=False),
    )


def _extract_transform_graph(model: Any, data: Any) -> sparse.csr_matrix:
    previous_mode = getattr(model, "transform_mode", None)
    setattr(model, "transform_mode", "graph")
    try:
        graph = model.transform(data)
    finally:
        setattr(model, "transform_mode", previous_mode)
    return _to_csr_matrix(graph)


@dataclass(slots=True)
class NetworkExportResult:
    network: Network
    kind: str


class HeliosUMAP:
    """
    Build UMAP embeddings with umap-learn while exporting the fuzzy graph and
    optional kNN graph as Helios networks.

    The wrapper keeps `fit` / `fit_transform` / `transform` behavior aligned
    with umap-learn and adds:

    - `network_`: fit-time fuzzy simplicial set exported as a Helios graph
    - `knn_network_`: directed kNN graph exported as a Helios graph
    - `fit_network(...)`
    - `fit_transform_network(...)`
    - `transform_network(...)`
    """

    def __init__(
        self,
        *,
        umap_cls: Any | None = None,
        build_knn_network: bool = True,
        prefer_pynndescent: bool = True,
        enable_umap_layout: bool = True,
        edge_weight_attribute: str = DEFAULT_UMAP_EDGE_WEIGHT_ATTRIBUTE,
        node_mass_attribute: str = DEFAULT_UMAP_NODE_MASS_ATTRIBUTE,
        embedding_attribute: str = DEFAULT_UMAP_EMBEDDING_ATTRIBUTE,
        position_attribute: str = DEFAULT_POSITION_ATTRIBUTE,
        **umap_kwargs: Any,
    ) -> None:
        self._umap_cls = umap_cls
        self.build_knn_network = bool(build_knn_network)
        self.prefer_pynndescent = bool(prefer_pynndescent)
        self.enable_umap_layout = bool(enable_umap_layout)
        self.edge_weight_attribute = str(edge_weight_attribute)
        self.node_mass_attribute = str(node_mass_attribute)
        self.embedding_attribute = str(embedding_attribute)
        self.position_attribute = str(position_attribute)
        self.umap_kwargs = dict(umap_kwargs)

        self.umap_ = None
        self.embedding_ = None
        self.graph_ = None
        self.network_: Network | None = None
        self.knn_network_: Network | None = None
        self._fit_data: np.ndarray | None = None

    def _make_model(self):
        if self._umap_cls is not None:
            return self._umap_cls(**self.umap_kwargs)
        return _lazy_import_umap().UMAP(**self.umap_kwargs)

    def fit(self, data: Any, y: Any | None = None) -> "HeliosUMAP":
        fit_data = _as_2d_float32(data)
        self._fit_data = fit_data
        self.umap_ = self._make_model()
        self.umap_.fit(fit_data, y=y)

        self.embedding_ = _as_2d_float32(getattr(self.umap_, "embedding_"))
        self.graph_ = _to_csr_matrix(getattr(self.umap_, "graph_"))
        self.network_ = self._build_fit_network()
        self.knn_network_ = self._build_knn_network() if self.build_knn_network else None
        return self

    def fit_transform(self, data: Any, y: Any | None = None) -> np.ndarray:
        self.fit(data, y=y)
        return np.asarray(self.embedding_, dtype=np.float32)

    def fit_graph(self, data: Any, y: Any | None = None) -> "HeliosUMAP":
        if y is not None:
            raise NotImplementedError("Graph-only UMAP export does not currently support supervised targets")

        fit_data = _as_2d_float32(data)
        self._fit_data = fit_data
        self.umap_ = self._make_model()

        umap_module = _lazy_import_umap()
        umap_impl = umap_module.umap_
        model = self.umap_
        model._raw_data = fit_data
        model._initial_alpha = getattr(model, "learning_rate", 1.0)
        model._validate_parameters()
        if getattr(model, "a", None) is None or getattr(model, "b", None) is None:
            model._a, model._b = umap_impl.find_ab_params(model.spread, model.min_dist)
        else:
            model._a = model.a
            model._b = model.b

        random_state = check_random_state(getattr(model, "random_state", None))
        metric_kwds = dict(getattr(model, "_metric_kwds", None) or getattr(model, "metric_kwds", None) or {})
        knn_indices, knn_dists, knn_search_index = umap_impl.nearest_neighbors(
            fit_data,
            int(getattr(model, "n_neighbors", 15)),
            getattr(model, "metric", "euclidean"),
            metric_kwds,
            bool(getattr(model, "angular_rp_forest", False)),
            random_state,
            low_memory=bool(getattr(model, "low_memory", True)),
            use_pynndescent=self.prefer_pynndescent,
            n_jobs=int(getattr(model, "n_jobs", -1)),
            verbose=bool(getattr(model, "verbose", False)),
        )

        knn_indices = np.asarray(knn_indices, dtype=np.int32)
        knn_dists = np.asarray(knn_dists, dtype=np.float32)
        disconnection_distance = float(getattr(model, "_disconnection_distance", np.inf))
        disconnected = knn_dists >= disconnection_distance
        knn_indices[disconnected] = -1
        knn_dists[disconnected] = np.inf

        model._knn_indices = knn_indices
        model._knn_dists = knn_dists
        model._knn_search_index = knn_search_index

        graph, sigmas, rhos = umap_impl.fuzzy_simplicial_set(
            fit_data,
            int(getattr(model, "n_neighbors", 15)),
            random_state,
            getattr(model, "metric", "euclidean"),
            metric_kwds,
            knn_indices,
            knn_dists,
            bool(getattr(model, "angular_rp_forest", False)),
            float(getattr(model, "set_op_mix_ratio", 1.0)),
            float(getattr(model, "local_connectivity", 1.0)),
            True,
            bool(getattr(model, "verbose", False)),
            None,
        )

        self.embedding_ = None
        self.graph_ = _to_csr_matrix(graph)
        model.graph_ = self.graph_
        model._sigmas = np.asarray(sigmas, dtype=np.float32)
        model._rhos = np.asarray(rhos, dtype=np.float32)
        self.network_ = self._build_fit_network(include_embedding=False)
        self.knn_network_ = self._build_knn_network(include_embedding=False) if self.build_knn_network else None
        return self

    def fit_network(self, data: Any, y: Any | None = None) -> Network:
        self.fit(data, y=y)
        if self.network_ is None:
            raise RuntimeError("UMAP fit completed without producing a network export")
        return self.network_

    def fit_graph_network(self, data: Any, y: Any | None = None) -> Network:
        self.fit_graph(data, y=y)
        if self.network_ is None:
            raise RuntimeError("UMAP graph fit completed without producing a network export")
        return self.network_

    def fit_transform_network(self, data: Any, y: Any | None = None) -> Network:
        return self.fit_network(data, y=y)

    def transform(self, data: Any) -> np.ndarray:
        if self.umap_ is None:
            raise RuntimeError("Call fit(...) before transform(...)")
        return _as_2d_float32(self.umap_.transform(_as_2d_float32(data)))

    def transform_network(self, data: Any, *, enable_umap_layout: bool = False) -> Network:
        if self.umap_ is None or self.embedding_ is None:
            raise RuntimeError("Call fit(...) before transform_network(...)")

        query_data = _as_2d_float32(data)
        query_embedding = self.transform(query_data)
        transform_graph = _extract_transform_graph(self.umap_, query_data)

        train_count = int(self.embedding_.shape[0])
        query_count = int(query_embedding.shape[0])
        combined_embedding = np.vstack([self.embedding_, query_embedding])
        rows, cols, weights = _extract_sparse_edges(transform_graph)

        network = Network(directed=False, node_capacity=train_count + query_count, edge_capacity=len(weights))
        network.add_nodes(train_count + query_count)
        if len(weights) > 0:
            _add_sparse_edges(network, rows + train_count, cols)

        self._write_node_exports(
            network,
            combined_embedding,
            mass=np.concatenate(
                [
                    np.zeros(train_count, dtype=np.float32),
                    np.asarray(transform_graph.sum(axis=1)).reshape(-1).astype(np.float32, copy=False),
                ]
            ),
            is_query=np.concatenate(
                [
                    np.zeros(train_count, dtype=np.uint32),
                    np.ones(query_count, dtype=np.uint32),
                ]
            ),
            source_index=np.concatenate(
                [
                    np.arange(train_count, dtype=np.uint32),
                    np.arange(query_count, dtype=np.uint32),
                ]
            ),
        )
        if len(weights) > 0:
            _write_numeric_attribute(network, AttributeScope.Edge, self.edge_weight_attribute, weights, AttributeType.Float)

        self._write_umap_metadata(
            network,
            graph_kind="transform",
            enable_umap_layout=enable_umap_layout,
            include_embedding_exports=True,
        )
        return network

    def _build_fit_network(self, *, include_embedding: bool = True) -> Network:
        if self.umap_ is None or self.graph_ is None:
            raise RuntimeError("UMAP fit state is incomplete")

        collapsed_graph = _collapse_fit_graph(self.graph_)
        rows, cols, weights = _extract_sparse_edges(collapsed_graph)
        node_count = int(self.embedding_.shape[0]) if self.embedding_ is not None else int(self.graph_.shape[0])
        mass = _compute_mass_from_graph(self.graph_)

        network = Network(directed=False, node_capacity=node_count, edge_capacity=len(weights))
        network.add_nodes(node_count)
        if len(weights) > 0:
            _add_sparse_edges(network, rows, cols)
            _write_numeric_attribute(network, AttributeScope.Edge, self.edge_weight_attribute, weights, AttributeType.Float)

        self._write_node_exports(
            network,
            self.embedding_ if include_embedding else None,
            mass=mass,
            is_query=np.zeros(node_count, dtype=np.uint32),
            source_index=np.arange(node_count, dtype=np.uint32),
        )

        sigmas = getattr(self.umap_, "_sigmas", None)
        rhos = getattr(self.umap_, "_rhos", None)
        if sigmas is not None:
            _write_numeric_attribute(network, AttributeScope.Node, "umap_sigma", np.asarray(sigmas, dtype=np.float32), AttributeType.Float)
        if rhos is not None:
            _write_numeric_attribute(network, AttributeScope.Node, "umap_rho", np.asarray(rhos, dtype=np.float32), AttributeType.Float)

        self._write_umap_metadata(
            network,
            graph_kind="fuzzy_simplicial_set",
            enable_umap_layout=self.enable_umap_layout,
            include_embedding_exports=include_embedding,
        )
        return network

    def _build_knn_network(self, *, include_embedding: bool = True) -> Network | None:
        if self.umap_ is None or self._fit_data is None:
            return None

        indices, distances = _extract_knn_arrays(
            self.umap_,
            self._fit_data,
            prefer_pynndescent=self.prefer_pynndescent,
        )
        node_count = int(indices.shape[0])
        neighbor_count = int(indices.shape[1]) if indices.ndim == 2 else 0
        if node_count <= 0 or neighbor_count <= 0:
            return None

        rows = np.repeat(np.arange(node_count, dtype=np.int64), neighbor_count)
        cols = np.asarray(indices.reshape(-1), dtype=np.int64)
        edge_mask = cols >= 0
        rows = rows[edge_mask]
        cols = cols[edge_mask]
        dist_values = np.asarray(distances.reshape(-1), dtype=np.float32)[edge_mask]
        ranks = np.tile(np.arange(neighbor_count, dtype=np.uint32), node_count)[edge_mask]

        network = Network(directed=True, node_capacity=node_count, edge_capacity=len(rows))
        network.add_nodes(node_count)
        if len(rows) > 0:
            _add_sparse_edges(network, rows, cols)
            _write_numeric_attribute(network, AttributeScope.Edge, "umap_knn_distance", dist_values, AttributeType.Float)
            _write_numeric_attribute(network, AttributeScope.Edge, "umap_knn_rank", ranks, AttributeType.UnsignedInteger)

        self._write_node_exports(
            network,
            self.embedding_ if include_embedding else None,
            mass=np.zeros(node_count, dtype=np.float32),
            is_query=np.zeros(node_count, dtype=np.uint32),
            source_index=np.arange(node_count, dtype=np.uint32),
        )
        self._write_umap_metadata(
            network,
            graph_kind="knn",
            enable_umap_layout=False,
            include_embedding_exports=include_embedding,
        )
        return network

    def _write_node_exports(
        self,
        network: Network,
        embedding: np.ndarray | None,
        *,
        mass: np.ndarray,
        is_query: np.ndarray,
        source_index: np.ndarray,
    ) -> None:
        if embedding is not None:
            _write_numeric_attribute(network, AttributeScope.Node, self.embedding_attribute, embedding, AttributeType.Float)
            _write_numeric_attribute(network, AttributeScope.Node, self.position_attribute, _positions_from_embedding(embedding), AttributeType.Float)
        _write_numeric_attribute(network, AttributeScope.Node, self.node_mass_attribute, mass, AttributeType.Float)
        _write_numeric_attribute(network, AttributeScope.Node, DEFAULT_QUERY_MASK_ATTRIBUTE, is_query, AttributeType.UnsignedInteger)
        _write_numeric_attribute(network, AttributeScope.Node, DEFAULT_INDEX_ATTRIBUTE, source_index, AttributeType.UnsignedInteger)

    def _write_umap_metadata(
        self,
        network: Network,
        *,
        graph_kind: str,
        enable_umap_layout: bool,
        include_embedding_exports: bool,
    ) -> None:
        model = self.umap_
        if model is None:
            raise RuntimeError("UMAP model is not initialized")

        _set_network_value(network, "umap", "true" if enable_umap_layout else "false")
        _set_network_value(network, "umap_graph_kind", graph_kind)
        _set_network_value(network, "umap_edge_weight_attr", self.edge_weight_attribute)
        _set_network_value(network, "umap_node_mass_attr", self.node_mass_attribute)
        if include_embedding_exports:
            _set_network_value(network, "umap_embedding_attr", self.embedding_attribute)
            _set_network_value(network, "umap_position_attr", self.position_attribute)
        _set_network_value(network, "umap_n_neighbors", int(getattr(model, "n_neighbors", 15)))
        _set_network_value(network, "umap_n_components", int(getattr(model, "n_components", 2)))
        _set_network_value(network, "umap_n_epochs", _resolve_umap_n_epochs(model, int(network.node_count())))
        _set_network_value(network, "umap_min_dist", float(getattr(model, "min_dist", 0.1)))
        _set_network_value(network, "umap_spread", float(getattr(model, "spread", 1.0)))
        _set_network_value(network, "umap_negative_sample_rate", str(float(getattr(model, "negative_sample_rate", 5))))
        _set_network_value(network, "umap_gamma", str(float(getattr(model, "repulsion_strength", getattr(model, "gamma", 1.0)))))
        _set_network_value(network, "umap_metric", _metric_name(getattr(model, "metric", "euclidean")))
        _set_network_value(network, "umap_a", str(float(getattr(model, "_a", getattr(model, "a", 1.5769434601962196)))))
        _set_network_value(network, "umap_b", str(float(getattr(model, "_b", getattr(model, "b", 0.8950608779914887)))))


__all__ = ["HeliosUMAP", "NetworkExportResult"]
