import { loadHelios } from '../utils/load-helios.mjs';

// Package import form: `import HeliosNetwork, { AttributeType } from 'helios-network';`

function log(step, payload) {
	console.log(`[${step}]`, payload);
}

async function main() {
	const { default: HeliosNetwork, AttributeType } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	log('init', 'Network ready');

	try {
		const nodes = network.addNodes(2);
		const edges = network.addEdges([{ from: nodes[0], to: nodes[1] }]);
		log('structure', { nodes: Array.from(nodes), edges: Array.from(edges) });

		network.nodeAttribute('weight', (_current, id) => (id === nodes[0] ? 1.5 : 3.25), { type: AttributeType.Float });
		network.edgeAttribute('flag', 1, { type: AttributeType.Boolean });
		network.networkAttribute('meta', { description: 'demo network', version: 1 });

		const snapshot = network.withBufferAccess(() => {
			const weight = network.getNodeAttributeBuffer('weight').view;
			const edgeFlags = network.getEdgeAttributeBuffer('flag').view;
			const meta = network.getNetworkAttributeBuffer('meta');
			return {
				weights: Array.from(weight.slice(0, network.nodeCount)),
				edgeFlags: Array.from(edgeFlags.slice(0, network.edgeCount)),
				meta: meta.get(0),
			};
		});
		log('nodeAttribute', snapshot.weights);
		log('edgeAttribute', snapshot.edgeFlags);
		log('networkAttribute', snapshot.meta);
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
