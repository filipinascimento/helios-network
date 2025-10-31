import { loadHelios } from '../utils/load-helios.js';

// Typical app import: `import HeliosNetwork from 'helios-network';`

const output = document.getElementById('output');

function log(message) {
	console.log(message);
	if (output) {
		output.textContent += `${message}\n`;
	}
}

async function run() {
	const { default: HeliosNetwork } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0 });

	try {
		const nodes = network.addNodes(5);
		const edges = network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
			{ from: nodes[2], to: nodes[3] },
			{ from: nodes[3], to: nodes[4] },
		]);

		log(`Initial nodeCount=${network.nodeCount}, edgeCount=${network.edgeCount}`);

		network.removeEdges([edges[1], edges[3]]);
		log(`After removing edges → edgeCount=${network.edgeCount}`);

		network.removeNodes([nodes[2]]);
		log(`After removing node ${nodes[2]} → nodeCount=${network.nodeCount}`);

		if (typeof network.module._CXNetworkCompact === 'function') {
			network.compact();
			log(`After compact() → nodeCount=${network.nodeCount}, edgeCount=${network.edgeCount}, nodeCapacity=${network.nodeCapacity}`);
		} else {
			log('compact()', 'CXNetworkCompact not exported in this build');
		}
	} finally {
		network.dispose();
		log('Network disposed');
	}
}

run().catch((error) => {
	console.error(error);
	log(`Example failed: ${error.message}`);
});
