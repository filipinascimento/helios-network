import { expect, test } from 'vitest';
import HeliosNetwork from '../src/helios-network.js';

test('multi-category attributes provide CSR buffers and weights', async () => {
	const network = await HeliosNetwork.create({ directed: false });
	const nodes = network.addNodes(3);
	const edges = network.addEdges([
		{ from: nodes[0], to: nodes[1] },
		{ from: nodes[1], to: nodes[2] },
	]);

	network.defineNodeMultiCategoryAttribute('tags');
	network.defineEdgeMultiCategoryAttribute('topics', true);

	network.setNodeMultiCategoryEntry('tags', nodes[0], ['alpha', 'beta']);
	network.setNodeMultiCategoryEntry('tags', nodes[1], []);
	network.setNodeMultiCategoryEntry('tags', nodes[2], ['beta']);

	network.setEdgeMultiCategoryEntry('topics', edges[0], ['x', 'y'], [0.2, 0.8]);
	network.setEdgeMultiCategoryEntry('topics', edges[1], ['y'], [1.0]);

	const tags = network.withBufferAccess(() => network.getNodeMultiCategoryBuffers('tags'));
	expect(tags.offsetCount).toBe(network.nodeCapacity + 1);
	expect(tags.entryCount).toBe(3);
	expect(Array.from(tags.offsets.slice(0, 4))).toEqual([0, 2, 2, 3]);
	expect(Array.from(tags.ids)).toEqual([0, 1, 1]);

	const topics = network.withBufferAccess(() => network.getEdgeMultiCategoryBuffers('topics'));
	expect(topics.offsetCount).toBe(network.edgeCapacity + 1);
	expect(topics.entryCount).toBe(3);
	expect(topics.hasWeights).toBe(true);
	expect(Array.from(topics.offsets.slice(0, 3))).toEqual([0, 2, 3]);
	expect(Array.from(topics.ids)).toEqual([0, 1, 1]);
	expect(topics.weights[0]).toBeCloseTo(0.2);
	expect(topics.weights[1]).toBeCloseTo(0.8);
	expect(topics.weights[2]).toBeCloseTo(1.0);

	const range = network.getNodeMultiCategoryEntryRange('tags', nodes[0]);
	expect(range).toEqual({ start: 0, end: 2 });

	network.dispose();
});
