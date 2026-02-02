from helios_network import AttributeScope, AttributeType, Network, from_igraph, from_networkx, to_igraph, to_networkx

network = Network(directed=False)
node_ids = network.add_nodes(3)
network.add_edges([(node_ids[0], node_ids[1]), (node_ids[1], node_ids[2])])

network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)
network.define_attribute(AttributeScope.Edge, "weight", AttributeType.Double, 1)
network.define_attribute(AttributeScope.Network, "title", AttributeType.String, 1)

network.nodes["label"] = ["a", "b", "c"]
network.edges["weight"] = [1.0, 2.0]
network["title"] = "demo"

try:
    nx_graph = to_networkx(network)
    network2 = from_networkx(nx_graph)
    print("networkx roundtrip node count:", network2.node_count())
except ImportError:
    print("networkx not installed")

try:
    ig_graph = to_igraph(network)
    network3 = from_igraph(ig_graph)
    print("igraph roundtrip node count:", network3.node_count())
except ImportError:
    print("igraph not installed")
