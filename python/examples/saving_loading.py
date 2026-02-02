import os
import tempfile

from helios_network import Network, read_xnet

network = Network(directed=False)
node_ids = network.add_nodes(3)
network.add_edges([(node_ids[0], node_ids[1]), (node_ids[1], node_ids[2])])

with tempfile.TemporaryDirectory() as tmpdir:
    path = os.path.join(tmpdir, "example.xnet")
    network.save_xnet(path)
    loaded = read_xnet(path)
    print("loaded nodes:", loaded.node_count())
    print("loaded edges:", loaded.edge_count())
