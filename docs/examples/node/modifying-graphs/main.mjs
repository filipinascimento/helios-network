import { loadHelios } from '../utils/load-helios.mjs';

// Real projects: `import HeliosNetwork, { AttributeType } from 'helios-network';`

function log(step, payload) {
	console.log(`[${step}]`, payload);
}

async function main() {
	const { default: HeliosNetwork, AttributeType } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
	log('init', 'Network ready for mutation demo');

	try {
		const nodes = network.addNodes(5);
		const edges = network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
			{ from: nodes[2], to: nodes[3] },
			{ from: nodes[3], to: nodes[4] },
		]);
		log('structure', { nodeCount: network.nodeCount, edgeCount: network.edgeCount });

		network.defineNodeAttribute('score', AttributeType.Float, 1);
		const score = network.getNodeAttributeBuffer('score').view;
		score[nodes[2]] = 2.5;

		network.removeEdges([edges[1]]);
		log('removeEdges', `Edge ${edges[1]} removed → remaining edgeCount=${network.edgeCount}`);

		network.removeNodes([nodes[4]]);
		log('removeNodes', `Node ${nodes[4]} removed → nodeCount=${network.nodeCount}`);

		if (typeof network.module._CXNetworkCompact === 'function') {
			network.compact({
				nodeOriginalIndexAttribute: 'origin_node',
				edgeOriginalIndexAttribute: 'origin_edge',
			});
			log('compact', {
				nodeCount: network.nodeCount,
				edgeCount: network.edgeCount,
				nodeCapacity: network.nodeCapacity,
			});
		} else {
			log('compact', 'CXNetworkCompact not available in this build');
		}
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
