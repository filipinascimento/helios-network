import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network-inline.js';

function makeRng(seed = 1) {
	let state = seed >>> 0;
	return () => {
		state ^= state << 13;
		state ^= state >>> 17;
		state ^= state << 5;
		return (state >>> 0) / 0xffffffff;
	};
}

function buildBalancedLabels(nodeCount, communityCount) {
	const labels = new Uint32Array(nodeCount);
	for (let i = 0; i < nodeCount; i += 1) {
		labels[i] = Math.floor((i * communityCount) / nodeCount);
	}
	return labels;
}

function generateSbmEdges(labels, pIn, pOut, rng) {
	const n = labels.length;
	const edges = [];
	for (let i = 0; i < n; i += 1) {
		for (let j = i + 1; j < n; j += 1) {
			const same = labels[i] === labels[j];
			const p = same ? pIn : pOut;
			if (rng() < p) {
				edges.push({ from: i, to: j });
			}
		}
	}
	return edges;
}

test('steppable Leiden session reports progress and finalizes', async () => {
	const n = 300;
	const k = 3;
	const truth = buildBalancedLabels(n, k);
	const edges = generateSbmEdges(truth, 0.14, 0.01, makeRng(123));

	const network = await HeliosNetwork.create({ directed: false, initialNodes: n, initialEdges: edges.length });
	try {
		network.defineEdgeAttribute('w', AttributeType.Float, 1);
		const edgeIds = network.addEdges(edges);
		const weights = network.getEdgeAttributeBuffer('w').view;
		for (let i = 0; i < edgeIds.length; i += 1) {
			const e = edges[i];
			weights[edgeIds[i]] = truth[e.from] === truth[e.to] ? 2.0 : 0.5;
		}

		const session = network.createLeidenSession({
			resolution: 1,
			edgeWeightAttribute: 'w',
			seed: 42,
			maxLevels: 16,
			maxPasses: 8,
			outNodeCommunityAttribute: 'community_session',
		});

		try {
			let last = session.getProgress();
			expect(Number.isFinite(last.progressCurrent)).toBe(true);
			expect(Number.isFinite(last.progressTotal)).toBe(true);
			expect(last.progressCurrent).toBeGreaterThanOrEqual(0);
			expect(last.progressTotal).toBeGreaterThanOrEqual(0);
			if (last.progressTotal > 0) {
				expect(last.progressCurrent).toBeLessThanOrEqual(last.progressTotal);
			}

			last = await session.run({
				stepOptions: { timeoutMs: 0, chunkBudget: 25 },
				yield: () => Promise.resolve(),
				maxIterations: 5000,
			});
			expect(last.phase).toBe(5);
			expect(session.isComplete()).toBe(true);

			const { communityCount, modularity } = session.finalize();
			expect(communityCount).toBeGreaterThan(1);
			expect(modularity).toBeGreaterThanOrEqual(0);
			expect(session.isFinalized()).toBe(true);
			const view = network.getNodeAttributeBuffer('community_session').view;
			expect(view.length).toBeGreaterThanOrEqual(network.nodeCount);
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
}, 30000);

test('steppable sessions cancel when tracked versions change', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 10, initialEdges: 10 });
	try {
		network.defineEdgeAttribute('w', AttributeType.Float, 1);
		network.addEdges([
			{ from: 0, to: 1 },
			{ from: 1, to: 2 },
			{ from: 2, to: 3 },
			{ from: 3, to: 4 },
			{ from: 4, to: 0 },
		]);

		const session = network.createLeidenSession({
			edgeWeightAttribute: 'w',
			seed: 1,
			maxLevels: 2,
			maxPasses: 2,
		});

		try {
			network.bumpEdgeAttributeVersion('w');
			expect(() => session.step({ budget: 10 })).toThrow(/Session canceled/i);
			expect(() => session.getProgress()).toThrow(/Session canceled/i);
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
}, 30000);

test('Leiden accepts passes alias and prioritizes it over maxPasses', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 8, initialEdges: 16 });
	try {
		network.addEdges([
			{ from: 0, to: 1 },
			{ from: 1, to: 2 },
			{ from: 2, to: 3 },
			{ from: 3, to: 0 },
			{ from: 4, to: 5 },
			{ from: 5, to: 6 },
			{ from: 6, to: 7 },
			{ from: 7, to: 4 },
			{ from: 0, to: 4 },
			{ from: 1, to: 5 },
		]);

		const session = network.createLeidenSession({
			seed: 7,
			maxLevels: 4,
			maxPasses: 9,
			passes: 2,
			outNodeCommunityAttribute: 'community_alias_session',
		});

		try {
			session.step({ budget: 1 });
			const progress = session.getProgress();
			expect(progress.maxPasses).toBe(2);
		} finally {
			session.dispose();
		}

		const { communityCount, modularity } = network.leidenModularity({
			seed: 7,
			maxLevels: 4,
			maxPasses: 9,
			passes: 2,
			outNodeCommunityAttribute: 'community_alias_modularity',
		});
		expect(communityCount).toBeGreaterThan(0);
		expect(Number.isFinite(modularity)).toBe(true);
	} finally {
		network.dispose();
	}
}, 30000);
