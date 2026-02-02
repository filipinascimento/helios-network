from helios_network import AttributeScope, AttributeType, Network

network = Network(directed=False)
node_ids = network.add_nodes(4)
edge_ids = network.add_edges([
    (node_ids[0], node_ids[1]),
    (node_ids[1], node_ids[2]),
    (node_ids[2], node_ids[3]),
])

network.define_attribute(AttributeScope.Node, "score", AttributeType.Double, 1)
network.define_attribute(AttributeScope.Edge, "weight", AttributeType.Double, 1)

network.nodes["score"] = 0.0
network.nodes[[node_ids[1], node_ids[3]]]["score"] = [1.5, 2.5]

network.edges["weight"] = 1.0
network.edges[[edge_ids[0], edge_ids[2]]]["weight"] = [2.0, 3.0]

print("node scores:", network.nodes["score"])
print("edge weights:", network.edges["weight"])
print("edge pairs:", list(network.edges.pairs()))
