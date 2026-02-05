import os
import tempfile
import warnings

from helios_network import AttributeScope, AttributeType, Network, read_bxnet, read_xnet, read_zxnet


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
