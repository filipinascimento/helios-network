import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { loadHelios } from '../utils/load-helios.mjs';

const log = (step, payload) => {
	console.log(`[${step}]`, payload);
};

const EPSILON = 1e-6;

function listActiveIndices(view) {
	const indices = [];
	for (let i = 0; i < view.length; i += 1) {
		if (view[i]) {
			indices.push(i);
		}
	}
	return indices;
}

function approxEqual(actual, expected, label) {
	const delta = Math.abs(Number(actual) - Number(expected));
	if (delta > EPSILON) {
		throw new Error(`${label}: expected ${expected}, received ${actual} (Δ=${delta})`);
	}
}

function expect(condition, message) {
	if (!condition) {
		throw new Error(message);
	}
}

function captureExpectedState(network) {
	const nodeIndices = listActiveIndices(network.nodeActivityView);
	const edgeIndices = listActiveIndices(network.edgeActivityView);

	const rankView = network.getNodeAttributeBuffer('rank').view;
	const scoreView = network.getNodeAttributeBuffer('score').view;
	const weightView = network.getEdgeAttributeBuffer('weight').view;

	return {
		directed: network.directed,
		title: network.getNetworkStringAttribute('title'),
		nodeIndices,
		nodeRanks: nodeIndices.map((index) => rankView[index]),
		nodeScores: nodeIndices.map((index) => scoreView[index]),
		nodeLabels: nodeIndices.map((index) => network.getNodeStringAttribute('label', index)),
		edgeIndices,
		edgeWeights: edgeIndices.map((index) => weightView[index]),
		edgeStatuses: edgeIndices.map((index) => network.getEdgeStringAttribute('status', index)),
	};
}

async function validateSnapshot(label, network, expected) {
	try {
		log(label, `Verifying ${network.nodeCount} nodes / ${network.edgeCount} edges`);

		expect(network.directed === expected.directed, `${label}: directed flag mismatch`);
		expect(network.nodeCount === expected.nodeIndices.length, `${label}: node count mismatch`);
		expect(network.edgeCount === expected.edgeIndices.length, `${label}: edge count mismatch`);
		expect(network.getNetworkStringAttribute('title') === expected.title, `${label}: network title mismatch`);

		const rankView = network.getNodeAttributeBuffer('rank').view;
		const scoreView = network.getNodeAttributeBuffer('score').view;
		const weightView = network.getEdgeAttributeBuffer('weight').view;

		expected.nodeIndices.forEach((nodeIndex, order) => {
			expect(network.nodeActivityView[nodeIndex] === 1, `${label}: node ${nodeIndex} inactive`);
			expect(rankView[nodeIndex] === expected.nodeRanks[order], `${label}: rank mismatch for node ${nodeIndex}`);
			approxEqual(scoreView[nodeIndex], expected.nodeScores[order], `${label}: score mismatch for node ${nodeIndex}`);
			expect(
				network.getNodeStringAttribute('label', nodeIndex) === expected.nodeLabels[order],
				`${label}: label mismatch for node ${nodeIndex}`
			);
		});

		expected.edgeIndices.forEach((edgeIndex, order) => {
			expect(network.edgeActivityView[edgeIndex] === 1, `${label}: edge ${edgeIndex} inactive`);
			approxEqual(weightView[edgeIndex], expected.edgeWeights[order], `${label}: weight mismatch for edge ${edgeIndex}`);
			expect(
				network.getEdgeStringAttribute('status', edgeIndex) === expected.edgeStatuses[order],
				`${label}: status mismatch for edge ${edgeIndex}`
			);
		});

		log(label, 'Verification OK');
	} finally {
		network.dispose();
	}
}

async function main() {
	const { default: HeliosNetwork, AttributeType } = await loadHelios({ preferSource: true });

	const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'helios-roundtrip-'));
	const paths = {
		bxnet: path.join(tmpDir, 'example.bxnet'),
		zxnet: path.join(tmpDir, 'example.zxnet'),
		xnet: path.join(tmpDir, 'example.xnet'),
	};
	log('temp-dir', tmpDir);

	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
	log('init', { directed: network.directed });

	try {
		const nodes = network.addNodes(4);
		const edges = network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
			{ from: nodes[2], to: nodes[3] },
		]);
		log('structure', { nodes: Array.from(nodes), edges: Array.from(edges) });

		network.defineNodeAttribute('rank', AttributeType.Integer, 1);
		network.defineNodeAttribute('score', AttributeType.Float, 1);
		network.defineNodeAttribute('label', AttributeType.String, 1);
		network.defineEdgeAttribute('weight', AttributeType.Float, 1);
		network.defineEdgeAttribute('status', AttributeType.String, 1);
		network.defineNetworkAttribute('title', AttributeType.String, 1);

		const rankView = network.getNodeAttributeBuffer('rank').view;
		const scoreView = network.getNodeAttributeBuffer('score').view;
		const weights = network.getEdgeAttributeBuffer('weight').view;

		rankView[nodes[0]] = 1n;
		rankView[nodes[1]] = 2n;
		rankView[nodes[2]] = 3n;
		rankView[nodes[3]] = 4n;

		scoreView[nodes[0]] = 1.25;
		scoreView[nodes[1]] = 2.5;
		scoreView[nodes[2]] = 3.75;
		scoreView[nodes[3]] = 5.0;

		network.setNodeStringAttribute('label', nodes[0], 'Alpha');
		network.setNodeStringAttribute('label', nodes[1], 'Beta');
		network.setNodeStringAttribute('label', nodes[2], 'Gamma');
		network.setNodeStringAttribute('label', nodes[3], 'Delta');

		weights[edges[0]] = 1.5;
		weights[edges[1]] = 2.0;
		weights[edges[2]] = 0.75;

		network.setEdgeStringAttribute('status', edges[0], 'active');
		network.setEdgeStringAttribute('status', edges[1], 'active');
		network.setEdgeStringAttribute('status', edges[2], 'experimental');

		network.setNetworkStringAttribute('title', 'Roundtrip Example');

		// Mutate attributes after creation.
		network.setNodeStringAttribute('label', nodes[1], 'Beta (updated)');
		weights[edges[1]] = 2.25;
		network.setEdgeStringAttribute('status', edges[1], 'updated');
		network.setEdgeStringAttribute('status', edges[2], 'deprecated');

		// Remove one edge and one node before persisting.
		network.removeEdges([edges[2]]);
		network.removeNodes([nodes[3]]);

		log('after-removals', {
			nodeCount: network.nodeCount,
			edgeCount: network.edgeCount,
			activeNodes: listActiveIndices(network.nodeActivityView),
			activeEdges: listActiveIndices(network.edgeActivityView),
		});

		const expected = captureExpectedState(network);

		await network.saveBXNet({ path: paths.bxnet });
		await network.saveZXNet({ path: paths.zxnet, compressionLevel: 5 });
		await network.saveXNet({ path: paths.xnet });

		const sizes = await Promise.all(
			Object.entries(paths).map(async ([key, filePath]) => {
				const { size } = await fs.stat(filePath);
				return [key, size];
			})
		);
		log('snapshots', Object.fromEntries(sizes));

		await validateSnapshot('BX (*.bxnet)', await HeliosNetwork.fromBXNet(paths.bxnet), expected);
		await validateSnapshot('ZX (*.zxnet)', await HeliosNetwork.fromZXNet(paths.zxnet), expected);
		await validateSnapshot('XNet (*.xnet)', await HeliosNetwork.fromXNet(paths.xnet), expected);

		log('result', 'All formats verified successfully.');
	} finally {
		network.dispose();
		await fs.rm(tmpDir, { recursive: true, force: true });
		log('teardown', 'Network disposed and temporary files removed');
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
