import { loadHelios } from '../utils/load-helios.js';

// In a consuming app, import directly: `import HeliosNetwork from 'helios-network';`

const output = document.getElementById('output');

function log(message) {
	console.log(message);
	if (output) {
		output.textContent += `${message}\n`;
	}
}

async function run() {
    const { default: HeliosNetwork } = await loadHelios();
	log('Initializing Helios Network…');
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 2, initialEdges: 0 });

	try {
		log(`Start: nodeCount=${network.nodeCount}, edgeCount=${network.edgeCount}`);

		const nodes = network.addNodes(3);
		log(`Added nodes → ${Array.from(nodes).join(', ')}`);

		const edges = network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
		]);
		log(`Added edges → ${Array.from(edges).join(', ')}`);

		const neighbors = network.getOutNeighbors(nodes[0]);
		log(`Out neighbours of ${nodes[0]} → ${Array.from(neighbors.nodes).join(', ')}`);
	} finally {
		network.dispose();
		log('Network disposed');
	}
}

run().catch((error) => {
	console.error(error);
	log(`Example failed: ${error.message}`);
});
