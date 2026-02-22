import { expect, test } from 'vitest';
import HeliosNetwork from '../src/helios-network.js';

test('collects one-hop neighbors for multiple source nodes', async () => {
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
	try {
		const nodes = network.addNodes(5);
		network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[0], to: nodes[2] },
			{ from: nodes[1], to: nodes[3] },
			{ from: nodes[2], to: nodes[4] },
		]);

		const neighborInfo = network.getNeighborsForNodes([nodes[0], nodes[1]], {
			direction: 'out',
			includeSourceNodes: false,
		});
		expect(Array.from(neighborInfo.nodes).sort((a, b) => a - b)).toEqual(
			Array.from([nodes[2], nodes[3]]).sort((a, b) => a - b)
		);
		expect(neighborInfo.edges.length).toBe(3);

		const incomingTo3 = network.getNeighbors(nodes[3], {
			direction: 'in',
			includeEdges: false,
			includeSourceNodes: false,
		});
		expect(Array.from(incomingTo3)).toEqual([nodes[1]]);
	} finally {
		network.dispose();
	}
});

test('collects concentric neighbors at and up to a level', async () => {
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
	try {
		const nodes = network.addNodes(6);
		network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[0], to: nodes[2] },
			{ from: nodes[1], to: nodes[3] },
			{ from: nodes[2], to: nodes[4] },
			{ from: nodes[3], to: nodes[5] },
			{ from: nodes[4], to: nodes[5] },
		]);

		const level2 = network.getNeighborsAtLevel(nodes[0], 2, {
			direction: 'out',
			includeEdges: false,
		});
		expect(Array.from(level2).sort((a, b) => a - b)).toEqual(
			Array.from([nodes[3], nodes[4]]).sort((a, b) => a - b)
		);

		const upTo2 = network.getNeighborsUpToLevel(nodes[0], 2, {
			direction: 'out',
			includeEdges: false,
		});
		expect(Array.from(upTo2).sort((a, b) => a - b)).toEqual(
			Array.from([nodes[1], nodes[2], nodes[3], nodes[4]]).sort((a, b) => a - b)
		);

		const selector = network.createNodeSelector([nodes[0]]);
		const selectorLevel2 = selector.neighborsAtLevel(2, { mode: 'out', includeEdges: false });
		expect(Array.from(selectorLevel2).sort((a, b) => a - b)).toEqual(
			Array.from([nodes[3], nodes[4]]).sort((a, b) => a - b)
		);

		const selectorUpTo2 = selector.neighborsUpToLevel(2, { mode: 'out', includeEdges: false });
		expect(Array.from(selectorUpTo2).sort((a, b) => a - b)).toEqual(
			Array.from([nodes[1], nodes[2], nodes[3], nodes[4]]).sort((a, b) => a - b)
		);
		selector.dispose();
	} finally {
		network.dispose();
	}
});
