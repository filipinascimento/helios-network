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
			expect(last.progress01).toBeGreaterThanOrEqual(0);
			expect(last.progress01).toBeLessThanOrEqual(1);

			let iterations = 0;
			while (last.phase !== 5 && last.phase !== 6) {
				last = session.step({ timeoutMs: 0, chunkBudget: 25 });
				expect(last.progress01).toBeGreaterThanOrEqual(0);
				expect(last.progress01).toBeLessThanOrEqual(1);
				iterations += 1;
				if (iterations > 5000) {
					throw new Error('Leiden session did not converge in expected steps');
				}
			}
			expect(last.phase).toBe(5);

			const { communityCount, modularity } = session.finalize();
			expect(communityCount).toBeGreaterThan(1);
			expect(modularity).toBeGreaterThanOrEqual(0);
			const view = network.getNodeAttributeBuffer('community_session').view;
			expect(view.length).toBeGreaterThanOrEqual(network.nodeCount);
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
}, 30000);
