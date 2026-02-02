import pytest

from helios_network import AttributeScope, AttributeType, Network, from_igraph, from_networkx, to_igraph, to_networkx

nx = pytest.importorskip("networkx", reason="networkx not installed")


def test_networkx_roundtrip():
    network = Network(directed=False)
    node_ids = network.add_nodes(3)
    network.add_edges([(node_ids[0], node_ids[1]), (node_ids[1], node_ids[2])])

    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
    network.define_attribute(AttributeScope.Edge, "weight", AttributeType.Double, 1)
    network.define_attribute(AttributeScope.Network, "title", AttributeType.String, 1)

    network.nodes["label"] = ["a", "b", "c"]
    network.edges["weight"] = [1.0, 2.0]
    network["title"] = "demo"

    nx_graph = to_networkx(network)
    assert nx_graph.graph["title"] == "demo"
    assert nx_graph.nodes[node_ids[0]]["label"] == "a"
    assert nx_graph.edges[node_ids[0], node_ids[1]]["weight"] == 1.0

    network2 = from_networkx(nx_graph)
    assert network2.node_count() == 3
    assert network2.edge_count() == 2
    assert network2["title"] == "demo"
    assert network2.nodes[0]["label"] in {"a", "b", "c"}

ig = pytest.importorskip("igraph", reason="igraph not installed")


def test_igraph_roundtrip():
    network = Network(directed=False)
    node_ids = network.add_nodes(3)
    network.add_edges([(node_ids[0], node_ids[1]), (node_ids[1], node_ids[2])])

    network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
    network.define_attribute(AttributeScope.Edge, "weight", AttributeType.Double, 1)
    network.define_attribute(AttributeScope.Network, "title", AttributeType.String, 1)

    network.nodes["label"] = ["a", "b", "c"]
    network.edges["weight"] = [1.0, 2.0]
    network["title"] = "demo"

    ig_graph = to_igraph(network)
    assert ig_graph["title"] == "demo"
    assert ig_graph.vs["label"][0] == "a"
    assert ig_graph.es["weight"][0] == 1.0

    network2 = from_igraph(ig_graph)
    assert network2.node_count() == 3
    assert network2.edge_count() == 2
    assert network2["title"] == "demo"
