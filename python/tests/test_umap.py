import os
import tempfile

import pytest

np = pytest.importorskip("numpy")
sparse = pytest.importorskip("scipy.sparse")

from helios_network import HeliosUMAP, read_bxnet


class FakeUMAP:
    def __init__(self, **kwargs):
        self.n_neighbors = int(kwargs.get("n_neighbors", 2))
        self.n_components = int(kwargs.get("n_components", 2))
        self.min_dist = float(kwargs.get("min_dist", 0.2))
        self.spread = float(kwargs.get("spread", 1.5))
        self.negative_sample_rate = float(kwargs.get("negative_sample_rate", 7))
        self.metric = kwargs.get("metric", "euclidean")
        self.random_state = kwargs.get("random_state", 13)
        self.repulsion_strength = float(kwargs.get("repulsion_strength", 1.25))
        self._a = float(kwargs.get("a", 1.7))
        self._b = float(kwargs.get("b", 0.83))
        self.transform_mode = "embedding"

    def fit(self, data, y=None):
        _ = y
        count = data.shape[0]
        self.embedding_ = np.asarray(
            [[0.0, 0.0], [1.0, 0.5], [2.0, -0.25]][:count],
            dtype=np.float32,
        )
        self.graph_ = sparse.csr_matrix(
            (
                np.asarray([0.4, 0.6, 0.5, 0.3], dtype=np.float32),
                (
                    np.asarray([0, 1, 1, 2], dtype=np.int32),
                    np.asarray([1, 0, 2, 1], dtype=np.int32),
                ),
            ),
            shape=(count, count),
        )
        self._sigmas = np.asarray([0.11, 0.22, 0.33][:count], dtype=np.float32)
        self._rhos = np.asarray([0.01, 0.02, 0.03][:count], dtype=np.float32)
        self._knn_indices = np.asarray(
            [[1, 2], [0, 2], [1, 0]][:count],
            dtype=np.int32,
        )
        self._knn_dists = np.asarray(
            [[0.1, 0.4], [0.1, 0.2], [0.2, 0.4]][:count],
            dtype=np.float32,
        )
        return self

    def transform(self, data):
        query_count = data.shape[0]
        if self.transform_mode == "graph":
            return sparse.csr_matrix(
                (
                    np.asarray([0.9, 0.2, 0.7], dtype=np.float32),
                    (
                        np.asarray([0, 0, 1], dtype=np.int32),
                        np.asarray([0, 2, 1], dtype=np.int32),
                    ),
                ),
                shape=(query_count, self.embedding_.shape[0]),
            )
        return np.asarray(
            [[3.0, 1.5], [4.0, -1.0]][:query_count],
            dtype=np.float32,
        )


def test_helios_umap_fit_exports_fuzzy_graph_and_knn_graph():
    data = np.asarray(
        [
            [0.0, 0.0],
            [1.0, 0.0],
            [0.0, 1.0],
        ],
        dtype=np.float32,
    )
    model = HeliosUMAP(
        umap_cls=FakeUMAP,
        n_neighbors=2,
        n_components=2,
        min_dist=0.2,
        spread=1.5,
        negative_sample_rate=7,
        repulsion_strength=1.25,
        metric="euclidean",
    )

    embedding = model.fit_transform(data)

    assert embedding.shape == (3, 2)
    assert model.network_ is not None
    assert model.knn_network_ is not None

    network = model.network_
    assert network.node_count() == 3
    assert network.edge_count() == 2
    assert network["umap"] == "true"
    assert network["umap_graph_kind"] == "fuzzy_simplicial_set"
    assert network["umap_edge_weight_attr"] == "umap_weight"
    assert network["umap_node_mass_attr"] == "umap_mass"
    assert network["umap_embedding_attr"] == "umap_embedding"
    assert network["umap_position_attr"] == "_helios_visuals_position"
    assert abs(float(network["umap_a"]) - 1.7) < 1e-6
    assert abs(float(network["umap_b"]) - 0.83) < 1e-6
    assert abs(float(network["umap_gamma"]) - 1.25) < 1e-6
    assert float(network["umap_negative_sample_rate"]) == 7.0
    assert int(network["umap_n_epochs"]) == 500

    assert [round(float(value), 6) for value in sorted(network.edges["umap_weight"])] == [0.5, 0.6]
    assert [round(float(value), 6) for value in network.nodes["umap_mass"]] == [0.4, 1.1, 0.3]
    assert network.nodes["umap_is_query"] == [0, 0, 0]
    assert network.nodes["umap_index"] == [0, 1, 2]
    assert network.nodes["umap_embedding"] == [
        (0.0, 0.0),
        (1.0, 0.5),
        (2.0, -0.25),
    ]
    assert network.nodes["_helios_visuals_position"] == [
        (0.0, 0.0, 0.0),
        (1.0, 0.5, 0.0),
        (2.0, -0.25, 0.0),
    ]

    knn_network = model.knn_network_
    assert bool(knn_network.is_directed) is True
    assert knn_network["umap"] == "false"
    assert knn_network["umap_graph_kind"] == "knn"
    assert knn_network.edge_count() == 6
    assert sorted(int(value) for value in knn_network.edges["umap_knn_rank"]) == [0, 0, 0, 1, 1, 1]


def test_helios_umap_transform_network_marks_queries_and_roundtrips_to_bxnet():
    data = np.asarray(
        [
            [0.0, 0.0],
            [1.0, 0.0],
            [0.0, 1.0],
        ],
        dtype=np.float32,
    )
    query = np.asarray(
        [
            [2.0, 2.0],
            [3.0, 3.0],
        ],
        dtype=np.float32,
    )
    model = HeliosUMAP(umap_cls=FakeUMAP)
    model.fit(data)

    network = model.transform_network(query)

    assert network.node_count() == 5
    assert network.edge_count() == 3
    assert network["umap"] == "false"
    assert network["umap_graph_kind"] == "transform"
    assert network.nodes["umap_is_query"] == [0, 0, 0, 1, 1]
    assert network.nodes["umap_index"] == [0, 1, 2, 0, 1]
    assert [round(float(value), 6) for value in network.nodes["umap_mass"]] == [0.0, 0.0, 0.0, 1.1, 0.7]
    assert [round(float(value), 6) for value in sorted(network.edges["umap_weight"])] == [0.2, 0.7, 0.9]

    with tempfile.TemporaryDirectory() as tmpdir:
        path = os.path.join(tmpdir, "umap-export.bxnet")
        network.save_bxnet(path)
        loaded = read_bxnet(path)
        assert loaded.node_count() == network.node_count()
        assert loaded.edge_count() == network.edge_count()


def test_helios_umap_fit_graph_network_exports_graph_without_embedding():
    pytest.importorskip("umap")

    data = np.asarray(
        [
            [-3.0, -3.0, 0.0, 0.0],
            [-2.8, -3.1, 0.1, -0.1],
            [3.0, -3.0, 0.0, 0.0],
            [3.2, -2.7, -0.1, 0.1],
            [0.0, 3.0, 0.0, 0.0],
            [0.2, 3.1, 0.1, -0.1],
        ],
        dtype=np.float32,
    )

    model = HeliosUMAP(
        n_neighbors=2,
        min_dist=0.1,
        random_state=7,
        build_knn_network=False,
    )

    network = model.fit_graph_network(data)

    assert network.node_count() == data.shape[0]
    assert network.edge_count() >= data.shape[0] // 2
    assert network["umap"] == "true"
    assert network["umap_graph_kind"] == "fuzzy_simplicial_set"
    assert network["umap_edge_weight_attr"] == "umap_weight"
    assert network["umap_node_mass_attr"] == "umap_mass"
    with pytest.raises(KeyError):
        _ = network["umap_embedding_attr"]
    with pytest.raises(KeyError):
        _ = network["umap_position_attr"]
    with pytest.raises(KeyError):
        _ = network.nodes["umap_embedding"]
    with pytest.raises(KeyError):
        _ = network.nodes["_helios_visuals_position"]
    assert network.nodes["umap_mass"] is not None
    assert network.nodes["umap_index"] is not None
    assert network.nodes["umap_is_query"] is not None
