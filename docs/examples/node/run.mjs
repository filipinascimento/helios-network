import HeliosNetwork, { AttributeType } from '../../../dist/helios-network.js';

function log(step, payload) {
	console.log(`[${step}]`, payload);
}

async function main() {
	log('setup', 'Initializing Helios Network...');
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 4, initialEdges: 0 });

	try {
		log('nodes', `Starting node count: ${network.nodeCount}`);

		const newNodes = network.addNodes(6);
		log('nodes', `Added nodes -> ${Array.from(newNodes).join(', ')}`);

		const newEdges = network.addEdges([
			{ from: 0, to: newNodes[0] },
			{ from: newNodes[0], to: newNodes[1] },
			{ from: newNodes[1], to: newNodes[2] },
		]);
		log('edges', `Created edges -> ${Array.from(newEdges).join(', ')}`);

		network.defineNodeAttribute('score', AttributeType.Float, 1);
		const score = network.getNodeAttributeBuffer('score');
		score.view[newNodes[1]] = 3.14;
		log('attributes', `Score of node ${newNodes[1]} set to ${score.view[newNodes[1]]}`);

		const selector = network.createNodeSelector(newNodes);
		log('selectors', `Selector size -> ${selector.count}`);
		selector.dispose();

		log('neighbors', network.getOutNeighbors(newNodes[0]));
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
	}
}

main().catch((error) => {
	console.error('Example failed', error);
	process.exitCode = 1;
});
