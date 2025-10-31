import { loadHelios } from '../utils/load-helios.mjs';

// Consumers typically do: `import HeliosNetwork from 'helios-network';`

function log(step, payload) {
	console.log(`[${step}]`, payload);
}

async function main() {
	const { default: HeliosNetwork } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
	log('init', 'Network ready for iteration demo');

	try {
		const nodes = network.addNodes(4);
		network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[0], to: nodes[2] },
			{ from: nodes[2], to: nodes[3] },
		]);

		log('nodes', `All nodes: ${Array.from({ length: network.nodeCount }, (_, idx) => idx).join(', ')}`);
		log('edges', `Edge count: ${network.edgeCount}`);

		const selector = network.createNodeSelector(nodes.slice(0, 3));
		log('selector.count', selector.count);

		const degrees = selector.degree({ mode: 'out' });
		log('selector.degree(out)', degrees);

		for (const node of selector) {
			const { nodes: neighbors } = network.getOutNeighbors(node);
			log('neighbors', { node, neighbors: Array.from(neighbors) });
		}

		const incident = selector.incidentEdges({ asSelector: true });
		log('incidentEdges.count', incident.count);
		incident.dispose();
		selector.dispose();
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
