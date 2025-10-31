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

		network.defineNodeAttribute('weight', AttributeType.Float, 1);
		network.defineEdgeAttribute('flag', AttributeType.Boolean, 1);
		network.defineNetworkAttribute('meta', AttributeType.Javascript, 1);

		const weight = network.getNodeAttributeBuffer('weight').view;
		weight[nodes[0]] = 1.5;
		weight[nodes[1]] = 3.25;
		log('nodeAttribute', Array.from(weight.slice(0, network.nodeCount)));

		const edgeFlags = network.getEdgeAttributeBuffer('flag').view;
		edgeFlags[edges[0]] = 1;
		log('edgeAttribute', Array.from(edgeFlags.slice(0, network.edgeCount)));

		const meta = network.getNetworkAttributeBuffer('meta');
		meta.set(0, { description: 'demo network', version: 1 });
		log('networkAttribute', meta.get(0));
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
