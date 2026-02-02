from helios_network import Network

network = Network(directed=False)
node_ids = network.add_nodes(4)
edge_ids = network.add_edges([
    (node_ids[0], node_ids[1]),
    (node_ids[1], node_ids[2]),
    (node_ids[2], node_ids[3]),
])

print("nodes:", node_ids)
print("edges:", edge_ids)
print("node_count:", network.node_count())
print("edge_count:", network.edge_count())
