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

function normalizedMutualInformation(truth, predicted) {
	const n = truth.length;
	if (predicted.length !== n || n === 0) {
		return 0;
	}

	const trueIds = new Map();
	const predIds = new Map();
	let trueCount = 0;
	let predCount = 0;

	const trueLabels = new Array(n);
	const predLabels = new Array(n);
	for (let i = 0; i < n; i += 1) {
		const t = truth[i];
		const p = predicted[i];
		if (!trueIds.has(t)) {
			trueIds.set(t, trueCount++);
		}
		if (!predIds.has(p)) {
			predIds.set(p, predCount++);
		}
		trueLabels[i] = trueIds.get(t);
		predLabels[i] = predIds.get(p);
	}

	const nI = new Float64Array(trueCount);
	const nJ = new Float64Array(predCount);
	const nIJ = new Map();

	for (let i = 0; i < n; i += 1) {
		const ti = trueLabels[i];
		const pj = predLabels[i];
		nI[ti] += 1;
		nJ[pj] += 1;
		const key = ti * predCount + pj;
		nIJ.set(key, (nIJ.get(key) ?? 0) + 1);
	}

	let mi = 0;
	for (const [key, count] of nIJ.entries()) {
		const ti = Math.floor(key / predCount);
		const pj = key - ti * predCount;
		const numerator = count * n;
		const denom = nI[ti] * nJ[pj];
		if (numerator > 0 && denom > 0) {
			mi += (count / n) * Math.log(numerator / denom);
		}
	}

	let hTrue = 0;
	for (let i = 0; i < trueCount; i += 1) {
		const p = nI[i] / n;
		if (p > 0) {
			hTrue -= p * Math.log(p);
		}
	}

	let hPred = 0;
	for (let j = 0; j < predCount; j += 1) {
		const p = nJ[j] / n;
		if (p > 0) {
			hPred -= p * Math.log(p);
		}
	}

	if (hTrue <= 0 || hPred <= 0) {
		return 0;
	}
	return mi / Math.sqrt(hTrue * hPred);
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

test('Leiden recovers SBM communities (unweighted)', async () => {
	const n = 400;
	const k = 4;
	const rng = makeRng(1337);
	const truth = buildBalancedLabels(n, k);
	const edges = generateSbmEdges(truth, 0.15, 0.01, rng);

	const network = await HeliosNetwork.create({ directed: false, initialNodes: n, initialEdges: edges.length });
	try {
		network.addEdges(edges);
		const { communityCount, modularity } = network.leidenModularity({
			resolution: 1,
			outNodeCommunityAttribute: 'community',
			seed: 42,
			maxLevels: 32,
			maxPasses: 10,
		});
		expect(communityCount).toBeGreaterThan(1);
		expect(modularity).toBeGreaterThan(0);

		const predictedView = network.getNodeAttributeBuffer('community').view;
		const predicted = new Uint32Array(n);
		for (let i = 0; i < n; i += 1) {
			predicted[i] = predictedView[i];
		}
		const nmi = normalizedMutualInformation(truth, predicted);
		expect(nmi).toBeGreaterThan(0.9);
	} finally {
		network.dispose();
	}
}, 20000);

test('Leiden handles weighted modularity with resolution', async () => {
	const n = 600;
	const k = 6;
	const rng = makeRng(2024);
	const truth = buildBalancedLabels(n, k);
	const edges = generateSbmEdges(truth, 0.08, 0.02, rng);

	const network = await HeliosNetwork.create({ directed: false, initialNodes: n, initialEdges: edges.length });
	try {
		network.defineEdgeAttribute('w', AttributeType.Float, 1);
		const edgeIds = network.addEdges(edges);
		const w = network.getEdgeAttributeBuffer('w').view;
		for (let i = 0; i < edgeIds.length; i += 1) {
			const e = edges[i];
			w[edgeIds[i]] = truth[e.from] === truth[e.to] ? 2.0 : 0.5;
		}

		const { communityCount } = network.leidenModularity({
			resolution: 0.8,
			edgeWeightAttribute: 'w',
			outNodeCommunityAttribute: 'community_w',
			seed: 7,
			maxLevels: 32,
			maxPasses: 12,
		});
		expect(communityCount).toBeGreaterThan(1);

		const predictedView = network.getNodeAttributeBuffer('community_w').view;
		const predicted = new Uint32Array(n);
		for (let i = 0; i < n; i += 1) {
			predicted[i] = predictedView[i];
		}
		const nmi = normalizedMutualInformation(truth, predicted);
		expect(nmi).toBeGreaterThan(0.65);
	} finally {
		network.dispose();
	}
}, 30000);
