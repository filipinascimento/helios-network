import { loadHelios } from '../utils/load-helios.js';

// In production code: `import HeliosNetwork, { AttributeType } from 'helios-network';`

const output = document.getElementById('output');

function log(message, data) {
	const printable = data !== undefined ? `${message}: ${JSON.stringify(data)}` : message;
	console.log(printable);
	if (output) {
		output.textContent += `${printable}\n`;
	}
}

async function run() {
    const { default: HeliosNetwork, AttributeType } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 1 });

	try {
		const nodes = network.addNodes(2);
		const edges = network.addEdges([{ from: nodes[0], to: nodes[1] }]);

		network.defineNodeAttribute('weight', AttributeType.Float, 1);
		network.defineEdgeAttribute('capacity', AttributeType.Double, 1);
		network.defineNetworkAttribute('meta', AttributeType.Javascript, 1);

		const weights = network.getNodeAttributeBuffer('weight').view;
		weights[nodes[0]] = 5.5;
		weights[nodes[1]] = 7.25;

		const capacity = network.getEdgeAttributeBuffer('capacity').view;
		capacity[edges[0]] = 42;

		const meta = network.getNetworkAttributeBuffer('meta');
		meta.set(0, { name: 'Browser attribute demo', createdAt: new Date().toISOString() });

		log('node weights', Array.from(weights.slice(0, network.nodeCount)));
		log('edge capacities', Array.from(capacity.slice(0, network.edgeCount)));
		log('network meta', meta.get(0));
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

run().catch((error) => {
	console.error(error);
	log('Example failed', error.message);
});
