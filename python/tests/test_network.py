import os
import tempfile
import warnings

from helios_network import (
    AttributeScope,
    AttributeType,
    ClusteringVariant,
    ConnectedComponentsMode,
    DimensionMethod,
    MeasurementExecutionMode,
    NeighborDirection,
    Network,
    StrengthMeasure,
    read_bxnet,
    read_xnet,
    read_zxnet,
)


def test_network_add_remove_counts():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    assert len(nodes) == 3
    edges = network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    assert len(edges) == 2
    assert network.node_count() == 3
    assert network.edge_count() == 2

    network.remove_nodes([nodes[2]])
    assert network.node_count() == 2


def test_attribute_set_get():
    network = Network(directed=True)
    nodes = network.add_nodes(2)
    network.define_attribute(AttributeScope.Node, "weight", AttributeType.Double, 1)
    network.set_attribute_value(AttributeScope.Node, "weight", nodes[0], 1.25)
    network.set_attribute_value(AttributeScope.Node, "weight", nodes[1], 2.5)
    assert network.get_attribute_value(AttributeScope.Node, "weight", nodes[0]) == 1.25
    assert network.get_attribute_value(AttributeScope.Node, "weight", nodes[1]) == 2.5

    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
    network.set_attribute_value(AttributeScope.Node, "label", nodes[0], "a")
    network.set_attribute_value(AttributeScope.Node, "label", nodes[1], "b")
    assert network.get_attribute_value(AttributeScope.Node, "label", nodes[0]) == "a"
    assert network.get_attribute_value(AttributeScope.Node, "label", nodes[1]) == "b"


def test_node_selectors_and_iterators():
    network = Network(directed=False)
    nodes = network.add_nodes(3)

    network.define_attribute(AttributeScope.Node, "value", AttributeType.Integer, 1)
    network.nodes["value"] = 2
    assert network.nodes["value"] == [2, 2, 2]

    network.nodes[[nodes[0], nodes[2]]]["value"] = [5, 7]
    assert network.nodes[nodes[0]]["value"] == 5
    assert network.nodes[nodes[2]]["value"] == 7

    network.nodes[[nodes[1]]]["value"] = 9
    assert network.nodes[nodes[1]]["value"] == 9

    for node in network.nodes:
        node["value"] = 1
    assert network.nodes["value"] == [1, 1, 1]

    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        network.nodes[[nodes[0], nodes[1]]]["value"] = [10]
        assert caught


def test_edge_selectors_and_pairs():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    edges = network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    network.define_attribute(AttributeScope.Edge, "weight", AttributeType.Double, 1)
    network.edges["weight"] = 3.5
    assert network.edges["weight"] == [3.5, 3.5]

    network.edges[[edges[0], edges[1]]]["weight"] = [1.0, 2.0]
    assert network.edges[edges[0]]["weight"] == 1.0
    assert network.edges[edges[1]]["weight"] == 2.0

    assert list(network.edges.pairs()) == [(nodes[0], nodes[1]), (nodes[1], nodes[2])]
    assert list(network.edges.with_indices()) == [
        (edges[0], (nodes[0], nodes[1])),
        (edges[1], (nodes[1], nodes[2])),
    ]


def test_neighbors_and_concentric_levels():
    network = Network(directed=True)
    nodes = network.add_nodes(6)
    network.add_edges([
        (nodes[0], nodes[1]),
        (nodes[0], nodes[2]),
        (nodes[1], nodes[3]),
        (nodes[2], nodes[4]),
        (nodes[3], nodes[5]),
        (nodes[4], nodes[5]),
    ])

    out_n = network.out_neighbors(nodes[0])
    assert set(out_n["nodes"]) == {nodes[1], nodes[2]}

    in_n = network.in_neighbors(nodes[5])
    assert set(in_n["nodes"]) == {nodes[3], nodes[4]}

    one_hop = network.neighbors([nodes[0], nodes[1]], direction=NeighborDirection.Out, include_source_nodes=False)
    assert set(one_hop["nodes"]) == {nodes[2], nodes[3]}
    assert len(one_hop["edges"]) == 3

    one_hop_in = network.neighbors(nodes[5], direction=NeighborDirection.In_, include_source_nodes=False)
    assert set(one_hop_in["nodes"]) == {nodes[3], nodes[4]}
    assert len(one_hop_in["edges"]) == 2

    level2 = network.neighbors_at_level(nodes[0], level=2, direction=NeighborDirection.Out)
    assert set(level2["nodes"]) == {nodes[3], nodes[4]}
    assert len(level2["edges"]) == 2

    up_to2 = network.neighbors_up_to_level(nodes[0], max_level=2, direction=NeighborDirection.Out)
    assert set(up_to2["nodes"]) == {nodes[1], nodes[2], nodes[3], nodes[4]}
    assert len(up_to2["edges"]) == 4

    selector = network.nodes[[nodes[0]]]
    selector_one_hop = selector.neighbors(direction=NeighborDirection.Out, include_source_nodes=False)
    assert set(selector_one_hop["nodes"]) == {nodes[1], nodes[2]}

    selector_level2 = selector.neighbors_at_level(2, direction=NeighborDirection.Out)
    assert set(selector_level2["nodes"]) == {nodes[3], nodes[4]}

    selector_up_to2 = selector.neighbors_up_to_level(2, direction=NeighborDirection.Out)
    assert set(selector_up_to2["nodes"]) == {nodes[1], nodes[2], nodes[3], nodes[4]}


def test_measure_connected_components():
    network = Network(directed=True)
    nodes = network.add_nodes(6)
    network.add_edges([
        (nodes[0], nodes[1]),
        (nodes[1], nodes[0]),
        (nodes[1], nodes[2]),
        (nodes[3], nodes[4]),
        (nodes[4], nodes[3]),
    ])

    result = network.measure_connected_components()
    values = result["values_by_node"]
    assert result["component_count"] == 3
    assert result["largest_component_size"] == 3
    assert values[nodes[0]] == values[nodes[1]]
    assert values[nodes[2]] == values[nodes[1]]
    assert values[nodes[3]] == values[nodes[4]]
    assert values[nodes[0]] != values[nodes[3]]
    assert values[nodes[5]] > 0

    strong = network.measure_connected_components(mode=ConnectedComponentsMode.Strong)
    strong_values = strong["values_by_node"]
    assert strong["mode"] == ConnectedComponentsMode.Strong
    assert strong["component_count"] == 4
    assert strong["largest_component_size"] == 2
    assert strong_values[nodes[0]] == strong_values[nodes[1]]
    assert strong_values[nodes[2]] != strong_values[nodes[1]]
    assert strong_values[nodes[3]] == strong_values[nodes[4]]
    assert strong_values[nodes[5]] != strong_values[nodes[3]]

    components = network.extract_connected_components(
        mode="strong",
        as_networks=True,
        out_node_component_attribute="component",
    )
    assert [component["size"] for component in components] == [2, 2, 1, 1]
    assert all("network" in component for component in components)
    assert network.get_attribute_value(AttributeScope.Node, "component", nodes[0]) > 0

    largest = network.extract_largest_connected_component(mode="strong", as_network=True)
    assert largest is not None
    assert largest["size"] == 2
    assert largest["network"].node_count() == 2
    assert largest["network"].edge_count() == 2

def test_measure_coreness():
    network = Network(directed=False)
    nodes = network.add_nodes(6)
    network.add_edges([
        (nodes[0], nodes[1]),
        (nodes[1], nodes[2]),
        (nodes[2], nodes[0]),
        (nodes[2], nodes[3]),
        (nodes[3], nodes[4]),
    ])

    single = network.measure_coreness(
        direction=NeighborDirection.Both,
        execution_mode=MeasurementExecutionMode.SingleThread,
    )
    values = single["values_by_node"]
    assert single["max_core"] == 2
    assert values[nodes[0]] == 2
    assert values[nodes[1]] == 2
    assert values[nodes[2]] == 2
    assert values[nodes[3]] == 1
    assert values[nodes[4]] == 1
    assert values[nodes[5]] == 0

    parallel = network.measure_coreness(
        direction=NeighborDirection.Both,
        execution_mode=MeasurementExecutionMode.Parallel,
    )
    assert parallel["max_core"] == single["max_core"]
    assert parallel["values_by_node"] == single["values_by_node"]


def test_query_select_nodes_and_edges():
    network = Network(directed=False)
    nodes = network.add_nodes(3)

    network.define_attribute(AttributeScope.Node, "score", AttributeType.Float, 1)
    network.nodes["score"] = [0.5, 2.0, 3.5]
    selector = network.select_nodes("score > 1.0")
    assert selector.ids == [nodes[1], nodes[2]]

    network.define_attribute(AttributeScope.Node, "flag", AttributeType.Integer, 1)
    network.nodes["flag"] = [1, 0, 1]
    edges = network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    edge_selector = network.select_edges("$src.flag == 1")
    assert edge_selector.ids == [edges[0]]

    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
    network.nodes["label"] = ["alpha", "beta", "gamma"]
    in_selector = network.select_nodes('label IN ("alpha", "gamma")')
    assert in_selector.ids == [nodes[0], nodes[2]]

    regex_selector = network.select_nodes('label =~ "^g"')
    assert regex_selector.ids == [nodes[2]]

    network.define_attribute(AttributeScope.Node, "vec2", AttributeType.Float, 2)
    network.set_attribute_value(AttributeScope.Node, "vec2", nodes[0], (0.2, 0.4))
    network.set_attribute_value(AttributeScope.Node, "vec2", nodes[1], (1.5, 0.1))
    network.set_attribute_value(AttributeScope.Node, "vec2", nodes[2], (0.3, 2.2))
    vec_selector = network.select_nodes("vec2 > 2.0")
    assert vec_selector.ids == [nodes[2]]

    vec_max = network.select_nodes("vec2.max > 2.0")
    assert vec_max.ids == [nodes[2]]

    vec_index = network.select_nodes("vec2[0] > 1.0")
    assert vec_index.ids == [nodes[1]]

    network.define_attribute(AttributeScope.Node, "vec2b", AttributeType.Float, 2)
    network.set_attribute_value(AttributeScope.Node, "vec2b", nodes[0], (0.1, 0.2))
    network.set_attribute_value(AttributeScope.Node, "vec2b", nodes[1], (1.0, 1.0))
    network.set_attribute_value(AttributeScope.Node, "vec2b", nodes[2], (0.1, 0.1))
    vec_dot = network.select_nodes("vec2.dot(vec2b) > 1.0")
    assert vec_dot.ids == [nodes[1]]

    vec_any = network.select_nodes("vec2.any > 2.0")
    assert vec_any.ids == [nodes[2]]

    vec_all = network.select_nodes("vec2.all > 0.11")
    assert vec_all.ids == [nodes[0], nodes[2]]

    vec_dot_const = network.select_nodes("vec2.dot([1, 1]) > 2.0")
    assert vec_dot_const.ids == [nodes[2]]


def test_apply_text_batch_relative_ids():
    network = Network(directed=False)
    network.define_attribute(AttributeScope.Node, "weight", AttributeType.Float, 1)
    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)

    batch = """
newIDs = ADD_NODES n=4
ADD_EDGES pairs=[(0,1),(1,2),(2,3)] ! relative newIDs
SET_ATTR_VALUES scope=node name=weight ids=[0,2] values=[0.5,2.0] ! relative newIDs
SET_ATTR_VALUES scope=node name=label ids=[1,3] values=["a","b"] ! relative newIDs
"""
    result = network.apply_text_batch(batch)
    assert all(entry.get("ok") for entry in result["results"])
    assert len(result["variables"]["newIDs"]) == 4
    assert network.edge_count() == 3


def test_network_scope_attributes():
    network = Network(directed=False)
    network.define_attribute(AttributeScope.Network, "title", AttributeType.String, 1)
    network.network["title"] = "demo"
    assert network.network["title"] == "demo"
    assert network.graph["title"] == "demo"
    network["title"] = "demo2"
    assert network["title"] == "demo2"
    network.attributes["title"] = "demo3"
    assert network.attributes["title"] == "demo3"


def test_save_load_xnet_roundtrip():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    with tempfile.TemporaryDirectory() as tmpdir:
        path = os.path.join(tmpdir, "sample.xnet")
        network.save_xnet(path)
        loaded = read_xnet(path)
        assert loaded.node_count() == network.node_count()
        assert loaded.edge_count() == network.edge_count()


def test_save_load_bxnet_roundtrip():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    with tempfile.TemporaryDirectory() as tmpdir:
        path = os.path.join(tmpdir, "sample.bxnet")
        network.save_bxnet(path)
        loaded = read_bxnet(path)
        assert loaded.node_count() == network.node_count()
        assert loaded.edge_count() == network.edge_count()


def test_save_load_zxnet_roundtrip():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    with tempfile.TemporaryDirectory() as tmpdir:
        path = os.path.join(tmpdir, "sample.zxnet")
        network.save_zxnet(path)
        loaded = read_zxnet(path)
        assert loaded.node_count() == network.node_count()
        assert loaded.edge_count() == network.edge_count()


def test_vector_attribute_assignment():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    edges = network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    network.define_attribute(AttributeScope.Node, "i3", AttributeType.Integer, 3)
    network.define_attribute(AttributeScope.Edge, "e3", AttributeType.Integer, 3)
    network.define_attribute(AttributeScope.Network, "g3", AttributeType.Integer, 3)

    network.nodes["i3"] = [1, 2, 3]
    assert network.nodes[nodes[0]]["i3"] == (1, 2, 3)

    network.nodes[[nodes[0], nodes[1], nodes[2]]]["i3"] = [[4, 5, 6], [7, 8, 9], [10, 11, 12]]
    assert network.nodes[nodes[1]]["i3"] == (7, 8, 9)

    network.edges["e3"] = [3, 2, 1]
    assert network.edges[edges[0]]["e3"] == (3, 2, 1)

    network.network["g3"] = [9, 8, 7]
    assert network["g3"] == (9, 8, 7)


def test_vector_attribute_numpy_support():
    try:
        import numpy as np
    except Exception:
        return

    network = Network(directed=False)
    nodes = network.add_nodes(2)
    network.define_attribute(AttributeScope.Node, "i3", AttributeType.Integer, 3)

    network.nodes["i3"] = np.array([1, 2, 3], dtype=np.int32)
    assert network.nodes[nodes[0]]["i3"] == (1, 2, 3)

    network.nodes[[nodes[0], nodes[1]]]["i3"] = np.array([[4, 5, 6], [7, 8, 9]], dtype=np.int32)
    assert network.nodes[nodes[1]]["i3"] == (7, 8, 9)


def test_categorical_dictionary_helpers():
    network = Network(directed=False)
    nodes = network.add_nodes(3)
    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
    network.nodes["label"] = ["a", "b", "a"]
    network.categorize_attribute(AttributeScope.Node, "label")
    mapping = network.get_category_dictionary(AttributeScope.Node, "label")
    assert mapping.get("a") is not None
    assert mapping.get("b") is not None


def test_auto_define_attribute():
    network = Network(directed=False)
    nodes = network.add_nodes(2)
    edges = network.add_edges([(nodes[0], nodes[1])])

    network.nodes["score"] = 1.5
    assert network.nodes[nodes[0]]["score"] == 1.5

    network.edges["weight"] = 2.0
    assert network.edges[edges[0]]["weight"] == 2.0

    network["title"] = "demo"
    assert network["title"] == "demo"


def test_dimension_measurements_on_toroidal_ring():
    network = Network(directed=False)
    nodes = network.add_nodes(64)
    edges = []
    for i in range(64):
        edges.append((nodes[i], nodes[(i + 1) % 64]))
    network.add_edges(edges)

    local = network.measure_node_dimension(node=0, max_level=6, method=DimensionMethod.LeastSquares, order=2)
    assert local["capacity"][0] == 1
    assert local["capacity"][1] == 3
    assert local["capacity"][2] == 5

    global_stats = network.measure_dimension(max_level=6, method=DimensionMethod.LeastSquares, order=2)
    d4 = global_stats["global_dimension"][4]
    assert 0.8 < d4 < 1.2


def _create_toroidal_network(sides):
    total_nodes = 1
    for side in sides:
        total_nodes *= side
    network = Network(directed=False)
    nodes = network.add_nodes(total_nodes)

    strides = [1] * len(sides)
    for d in range(1, len(sides)):
        strides[d] = strides[d - 1] * sides[d - 1]

    def coordinates_from_linear(index):
        coords = [0] * len(sides)
        value = index
        for d, side in enumerate(sides):
            coords[d] = value % side
            value //= side
        return coords

    def linear_from_coordinates(coords):
        return sum(coords[d] * strides[d] for d in range(len(coords)))

    edges = []
    for idx in range(total_nodes):
        coords = coordinates_from_linear(idx)
        for d in range(len(sides)):
            neighbor = coords.copy()
            neighbor[d] = (neighbor[d] + 1) % sides[d]
            edges.append((nodes[idx], nodes[linear_from_coordinates(neighbor)]))
    network.add_edges(edges)
    return network


def test_dimension_methods_support_fw_bk_ce_ls():
    network = _create_toroidal_network([20, 20])
    for method in (DimensionMethod.Forward, DimensionMethod.Backward, DimensionMethod.Central, DimensionMethod.LeastSquares):
        stats = network.measure_dimension(max_level=6, method=method, order=2, nodes=list(range(48)))
        assert stats["selected_count"] == 48
        assert stats["average_capacity"][3] > 0
        assert abs(stats["global_dimension"][3]) < 10
        assert abs(stats["average_node_dimension"][3]) < 10


def test_dimension_order_limits_follow_cv_bounds():
    network = _create_toroidal_network([16, 16])
    import pytest

    with pytest.raises(RuntimeError):
        network.measure_node_dimension(node=0, max_level=6, method=DimensionMethod.Forward, order=7)
    with pytest.raises(RuntimeError):
        network.measure_node_dimension(node=0, max_level=6, method=DimensionMethod.Backward, order=7)
    with pytest.raises(RuntimeError):
        network.measure_node_dimension(node=0, max_level=6, method=DimensionMethod.Central, order=5)


def test_measure_degree_strength_and_clustering_known_values():
    network = Network(directed=False)
    nodes = network.add_nodes(4)
    edges = network.add_edges([(nodes[0], nodes[1]), (nodes[0], nodes[2]), (nodes[0], nodes[3])])
    network.define_attribute(AttributeScope.Edge, "w", AttributeType.Float, 1)
    network.edges[[edges[0], edges[1], edges[2]]]["w"] = [2.0, 3.0, 4.0]

    degree = network.measure_degree(direction=NeighborDirection.Both)
    assert degree["values_by_node"][0] == 3
    assert degree["values_by_node"][1] == 1

    strength = network.measure_strength(edge_weight_attribute="w", direction=NeighborDirection.Both, measure=StrengthMeasure.Sum)
    assert strength["values_by_node"][0] == 9.0
    assert strength["values_by_node"][3] == 4.0

    triangle = Network(directed=False)
    tri_nodes = triangle.add_nodes(3)
    tri_edges = triangle.add_edges([(tri_nodes[0], tri_nodes[1]), (tri_nodes[1], tri_nodes[2]), (tri_nodes[0], tri_nodes[2])])
    triangle.define_attribute(AttributeScope.Edge, "w", AttributeType.Float, 1)
    triangle.edges[[tri_edges[0], tri_edges[1], tri_edges[2]]]["w"] = [1.0, 1.0, 1.0]

    clustering = triangle.measure_local_clustering_coefficient(variant=ClusteringVariant.Unweighted)
    assert abs(clustering["values_by_node"][0] - 1.0) < 1e-6
    onnela = triangle.measure_local_clustering_coefficient(edge_weight_attribute="w", variant=ClusteringVariant.Onnela)
    assert abs(onnela["values_by_node"][1] - 1.0) < 1e-6
    newman = triangle.measure_local_clustering_coefficient(edge_weight_attribute="w", variant=ClusteringVariant.Newman)
    assert abs(newman["values_by_node"][2] - 1.0) < 1e-6


def test_measure_eigenvector_and_betweenness_known_values():
    star = Network(directed=False)
    nodes = star.add_nodes(5)
    star.add_edges([(nodes[0], nodes[1]), (nodes[0], nodes[2]), (nodes[0], nodes[3]), (nodes[0], nodes[4])])
    eigen = star.measure_eigenvector_centrality(
        direction=NeighborDirection.Both,
        max_iterations=256,
        tolerance=1e-8,
        execution_mode=MeasurementExecutionMode.SingleThread,
    )
    values = eigen["values_by_node"]
    assert eigen["iterations"] > 0
    assert values[0] > values[1]
    assert abs((values[0] / values[1]) - 2.0) < 0.1

    path = Network(directed=False)
    path_nodes = path.add_nodes(4)
    path.add_edges([(path_nodes[0], path_nodes[1]), (path_nodes[1], path_nodes[2]), (path_nodes[2], path_nodes[3])])
    between = path.measure_betweenness_centrality(
        normalize=True,
        execution_mode=MeasurementExecutionMode.SingleThread,
    )
    b_values = between["values_by_node"]
    assert abs(b_values[1] - (2.0 / 3.0)) < 0.05
    assert abs(b_values[2] - (2.0 / 3.0)) < 0.05
    assert abs(b_values[0]) < 1e-6
    assert abs(b_values[3]) < 1e-6


def test_measure_betweenness_chunk_accumulation_matches_full_run():
    network = Network(directed=False)
    nodes = network.add_nodes(4)
    edges = network.add_edges([(nodes[0], nodes[1]), (nodes[1], nodes[3]), (nodes[0], nodes[2]), (nodes[2], nodes[3])])
    network.define_attribute(AttributeScope.Edge, "w", AttributeType.Float, 1)
    network.edges[[edges[0], edges[1], edges[2], edges[3]]]["w"] = [1.0, 1.0, 1.0, 10.0]

    full = network.measure_betweenness_centrality(
        edge_weight_attribute="w",
        normalize=False,
        execution_mode=MeasurementExecutionMode.SingleThread,
    )
    chunk_a = network.measure_betweenness_centrality(
        edge_weight_attribute="w",
        source_nodes=[0, 1],
        normalize=False,
        execution_mode=MeasurementExecutionMode.SingleThread,
    )
    chunk_b = network.measure_betweenness_centrality(
        edge_weight_attribute="w",
        source_nodes=[2, 3],
        normalize=False,
        accumulate=True,
        initial=chunk_a["values_by_node"],
        execution_mode=MeasurementExecutionMode.SingleThread,
    )
    for idx in range(network.node_capacity()):
        assert abs(chunk_b["values_by_node"][idx] - full["values_by_node"][idx]) < 1e-6
