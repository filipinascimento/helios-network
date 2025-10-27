import HeliosNetwork, { AttributeType } from '../../../dist/helios-network.js';

const logTarget = document.getElementById('output');

function log(message) {
	console.log(message);
	if (logTarget) {
		logTarget.textContent += `${message}\n`;
	}
}

async function run() {
	log('Bootstrapping Helios Network...');
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 2 });

	log(`Initial nodes: ${network.nodeCount}`);

	const additionalNodes = network.addNodes(3);
	log(`Added nodes: ${Array.from(additionalNodes).join(', ')}`);

	const edges = network.addEdges([
		{ from: additionalNodes[0], to: additionalNodes[1] },
		{ from: additionalNodes[1], to: additionalNodes[2] },
	]);
	log(`Created edges: ${Array.from(edges).join(', ')}`);

	network.defineNodeAttribute('importance', AttributeType.Float, 1);
	const importance = network.getNodeAttributeBuffer('importance');
	importance.view[additionalNodes[0]] = 42.5;
	log(`Importance of node ${additionalNodes[0]}: ${importance.view[additionalNodes[0]]}`);

	const selector = network.createNodeSelector(additionalNodes);
	log(`Selector count: ${selector.count}`);
	selector.dispose();

	network.dispose();
	log('Network disposed.');
}

run().catch((err) => {
	console.error(err);
	log(`Example failed: ${err.message}`);
});
