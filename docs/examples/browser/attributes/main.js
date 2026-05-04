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

		network.nodeAttribute('weight', (_current, id) => (id === nodes[0] ? 5.5 : 7.25), { type: AttributeType.Float });
		network.edgeAttribute('capacity', 42, { type: AttributeType.Double });
		network.networkAttribute('meta', { name: 'Browser attribute demo', createdAt: new Date().toISOString() });

		const snapshot = network.withBufferAccess(() => {
			const weights = network.getNodeAttributeBuffer('weight').view;
			const capacity = network.getEdgeAttributeBuffer('capacity').view;
			const meta = network.getNetworkAttributeBuffer('meta');

			return {
				weights: Array.from(weights.slice(0, network.nodeCount)),
				capacity: Array.from(capacity.slice(0, network.edgeCount)),
				meta: meta.get(0),
			};
		});

		log('node weights', snapshot.weights);
		log('edge capacities', snapshot.capacity);
		log('network meta', snapshot.meta);
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

run().catch((error) => {
	console.error(error);
	log('Example failed', error.message);
});
