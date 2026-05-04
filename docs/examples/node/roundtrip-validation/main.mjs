import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { loadHelios } from '../utils/load-helios.mjs';

const log = (step, payload) => {
	console.log(`[${step}]`, payload);
};

const EPSILON = 1e-6;

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
	return network.withBufferAccess(() => {
		const nodeIndices = Array.from(network.nodeIndices);
		const edgeIndices = Array.from(network.edgeIndices);
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
	}, { nodeIndices: true, edgeIndices: true });
}

async function validateSnapshot(label, network, expected) {
	try {
		log(label, `Verifying ${network.nodeCount} nodes / ${network.edgeCount} edges`);

		expect(network.directed === expected.directed, `${label}: directed flag mismatch`);
		expect(network.nodeCount === expected.nodeIndices.length, `${label}: node count mismatch`);
		expect(network.edgeCount === expected.edgeIndices.length, `${label}: edge count mismatch`);
		expect(network.getNetworkStringAttribute('title') === expected.title, `${label}: network title mismatch`);
		network.getNodeAttributeInfo('rank');
		network.getNodeAttributeInfo('score');
		network.getNodeAttributeInfo('label');
		network.getEdgeAttributeInfo('weight');
		network.getEdgeAttributeInfo('status');

		network.withBufferAccess(() => {
			const rankView = network.getNodeAttributeBuffer('rank').view;
			const scoreView = network.getNodeAttributeBuffer('score').view;
			const weightView = network.getEdgeAttributeBuffer('weight').view;

			expected.nodeIndices.forEach((nodeIndex, order) => {
				expect(rankView[nodeIndex] === expected.nodeRanks[order], `${label}: rank mismatch for node ${nodeIndex}`);
				approxEqual(scoreView[nodeIndex], expected.nodeScores[order], `${label}: score mismatch for node ${nodeIndex}`);
				expect(
					network.getNodeStringAttribute('label', nodeIndex) === expected.nodeLabels[order],
					`${label}: label mismatch for node ${nodeIndex}`
				);
			});

			expected.edgeIndices.forEach((edgeIndex, order) => {
				approxEqual(weightView[edgeIndex], expected.edgeWeights[order], `${label}: weight mismatch for edge ${edgeIndex}`);
				expect(
					network.getEdgeStringAttribute('status', edgeIndex) === expected.edgeStatuses[order],
					`${label}: status mismatch for edge ${edgeIndex}`
				);
			});
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

		network
			.nodeAttribute('rank', [1, 2, 3, 4], { type: AttributeType.Integer })
			.nodeAttribute('score', [1.25, 2.5, 3.75, 5.0], { type: AttributeType.Float })
			.nodeAttribute('label', ['Alpha', 'Beta', 'Gamma', 'Delta'])
			.edgeAttribute('weight', [1.5, 2.0, 0.75], { type: AttributeType.Float })
			.edgeAttribute('status', ['active', 'active', 'experimental'])
			.networkAttribute('title', 'Roundtrip Example');

		// Mutate attributes after creation.
		network
			.nodeAttribute('label', (current, id) => (id === nodes[1] ? 'Beta (updated)' : current))
			.edgeAttribute('weight', (current, id) => (id === edges[1] ? 2.25 : current))
			.edgeAttribute('status', (current, id) => {
				if (id === edges[1]) return 'updated';
				if (id === edges[2]) return 'deprecated';
				return current;
			});

		// Remove one edge and one node before persisting.
		network.removeEdges([edges[2]]);
		network.removeNodes([nodes[3]]);

		log('after-removals', {
			nodeCount: network.nodeCount,
			edgeCount: network.edgeCount,
			...network.withBufferAccess(() => ({
				activeNodes: Array.from(network.nodeIndices),
				activeEdges: Array.from(network.edgeIndices),
			}), { nodeIndices: true, edgeIndices: true }),
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
