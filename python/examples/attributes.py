from helios_network import AttributeScope, AttributeType, Network

network = Network(directed=True)
node_ids = network.add_nodes(3)

network.define_attribute(AttributeScope.Node, "weight", AttributeType.Double, 1)
network.define_attribute(AttributeScope.Node, "label", AttributeType.String, 1)

network.nodes["weight"] = 1.0
network.nodes["label"] = [f"node-{idx}" for idx in range(len(node_ids))]

network.nodes[[node_ids[0], node_ids[2]]]["weight"] = [2.5, 3.5]

for node in network.nodes:
    print(node)
    print(node.id, node["weight"], node["label"])
