import { loadHelios } from '../utils/load-helios.mjs';

// Downstream apps can do: `import HeliosNetwork from 'helios-network';`

function log(step, message) {
	console.log(`[${step}] ${message}`);
}

async function main() {
	const { default: HeliosNetwork } = await loadHelios();
	log('init', 'Creating a directed network with two seed nodes');
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 2, initialEdges: 0 });

	try {
		log('counts', `nodeCount=${network.nodeCount}, edgeCount=${network.edgeCount}`);

		const newNodes = network.addNodes(3);
		log('addNodes', `Created nodes → ${Array.from(newNodes).join(', ')}`);
		log('counts', `nodeCount=${network.nodeCount}`);

		const newEdges = network.addEdges([
			{ from: 0, to: newNodes[0] },
			{ from: newNodes[0], to: newNodes[1] },
		]);
		log('addEdges', `Created edges → ${Array.from(newEdges).join(', ')}`);
		log('counts', `edgeCount=${network.edgeCount}`);

		const neighbors = network.getOutNeighbors(newNodes[0]);
		log('neighbors', `Node ${newNodes[0]} connects to → ${Array.from(neighbors.nodes).join(', ')}`);
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
