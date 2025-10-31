import { loadHelios } from '../utils/load-helios.js';

// Real-world usage: `import HeliosNetwork from 'helios-network';`

const output = document.getElementById('output');

function log(message, data) {
	const printable = data !== undefined ? `${message}: ${JSON.stringify(data)}` : message;
	console.log(printable);
	if (output) {
		output.textContent += `${printable}\n`;
	}
}

async function run() {
	const { default: HeliosNetwork } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0 });

	try {
		const nodes = network.addNodes(4);
		network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
			{ from: nodes[2], to: nodes[3] },
			{ from: nodes[0], to: nodes[3] },
		]);

		const selector = network.createNodeSelector(nodes.slice(0, 3));
		log('selector.count', selector.count);

		const degrees = selector.degree({ mode: 'out' });
		log('selector.degree(out)', Array.from(degrees));

		for (const node of selector) {
			const { nodes: neighbours } = network.getOutNeighbors(node);
			log(`out neighbours for ${node}`, Array.from(neighbours));
		}

		const incidentEdges = selector.incidentEdges({ asSelector: true });
		log('incident edge ids', Array.from(incidentEdges));

		incidentEdges.dispose();
		selector.dispose();
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

run().catch((error) => {
	console.error(error);
	log('Example failed', error.message);
});
