import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network-inline.js';

function makeRingEdges(n) {
	const edges = [];
	for (let i = 0; i < n; i += 1) {
		edges.push({ from: i, to: (i + 1) % n });
	}
	return edges;
}

function makeCliqueEdges(n) {
	const edges = [];
	for (let i = 0; i < n; i += 1) {
		for (let j = i + 1; j < n; j += 1) {
			edges.push({ from: i, to: j });
		}
	}
	return edges;
}

test('Leiden can run in a worker and apply results', async () => {
	const n = 200;
	const edges = makeRingEdges(n);

	const network = await HeliosNetwork.create({ directed: false, initialNodes: n, initialEdges: edges.length });
	try {
		network.defineEdgeAttribute('w', AttributeType.Float, 1);
		const edgeIds = network.addEdges(edges);
		const weights = network.getEdgeAttributeBuffer('w').view;
		for (let i = 0; i < edgeIds.length; i += 1) {
			weights[edgeIds[i]] = 1.0;
		}
		network.bumpEdgeAttributeVersion('w');

		const session = network.createLeidenSession({
			resolution: 1,
			edgeWeightAttribute: 'w',
			seed: 123,
			maxLevels: 8,
			maxPasses: 4,
			outNodeCommunityAttribute: 'community_worker',
		});

		const result = await session.runWorker({
			stepOptions: { timeoutMs: 0, chunkBudget: 50 },
			yieldMs: 0,
			onProgress: () => {},
		});

		expect(result.communityCount).toBeGreaterThan(0);
		expect(Number.isFinite(result.modularity)).toBe(true);

		expect(network.getNodeAttributeVersion('community_worker')).toBeGreaterThan(0);
		const comm = network.getNodeAttributeBuffer('community_worker').view;
		expect(comm.length).toBeGreaterThanOrEqual(network.nodeCount);
	} finally {
		network.dispose();
	}
}, 30000);

test('Leiden worker resolution affects result', async () => {
	const n = 20;
	const edges = makeCliqueEdges(n);

	const network = await HeliosNetwork.create({ directed: false, initialNodes: n, initialEdges: edges.length });
	try {
		network.addEdges(edges);

		const low = network.createLeidenSession({
			resolution: 1e-4,
			edgeWeightAttribute: null,
			seed: 1,
			maxLevels: 16,
			maxPasses: 8,
			outNodeCommunityAttribute: 'community_worker_low',
		});
		const lowResult = await low.runWorker({
			stepOptions: { timeoutMs: 0, chunkBudget: 200 },
			yieldMs: 0,
		});

		const high = network.createLeidenSession({
			resolution: 1e4,
			edgeWeightAttribute: null,
			seed: 1,
			maxLevels: 16,
			maxPasses: 8,
			outNodeCommunityAttribute: 'community_worker_high',
		});
		const highResult = await high.runWorker({
			stepOptions: { timeoutMs: 0, chunkBudget: 200 },
			yieldMs: 0,
		});

		expect(lowResult.communityCount).toBeGreaterThan(0);
		expect(highResult.communityCount).toBeGreaterThan(0);
		// With a large change in resolution (gamma), we expect a different partition on a clique.
		expect(lowResult.communityCount).not.toBe(highResult.communityCount);
	} finally {
		network.dispose();
	}
}, 30000);
