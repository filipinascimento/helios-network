import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';

test('can create network and add nodes/edges', async () => {
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 2 });
	expect(network.nodeCount).toBe(2);

	const newNodes = network.addNodes(3);
	expect(newNodes.length).toBe(3);
	expect(network.nodeCount).toBe(5);

	const edges = network.addEdges([
		{ from: newNodes[0], to: newNodes[1] },
		{ from: newNodes[1], to: newNodes[2] },
	]);
	expect(edges.length).toBe(2);
	expect(network.edgeCount).toBe(2);

	network.defineNodeAttribute('weight', AttributeType.Float, 1);
	const weightBuffer = network.getNodeAttributeBuffer('weight');
	weightBuffer.view[ newNodes[0] ] = 42.5;
	expect(weightBuffer.view[newNodes[0]]).toBeCloseTo(42.5);

	network.dispose();
});
