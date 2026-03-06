import { expect, test } from 'vitest';
import HeliosNetwork, {
	AttributeType,
	NeighborDirection,
	StrengthMeasure,
	ClusteringCoefficientVariant,
	MeasurementExecutionMode,
	ConnectedComponentsMode,
} from '../src/helios-network.js';
import { withEdgeBuffer, withNodeBuffer } from './helpers/bufferAccess.js';

function expectNear(actual, expected, epsilon = 1e-4) {
	expect(Math.abs(actual - expected)).toBeLessThanOrEqual(epsilon);
}

async function buildNetwork({ directed = false, nodeCount = 0, edges = [], weightName = null, weights = null }) {
	const network = await HeliosNetwork.create({ directed, initialNodes: 0, initialEdges: 0 });
	network.addNodes(nodeCount);
	const edgeIds = network.addEdges(edges.map(([from, to]) => ({ from, to })));
	if (weightName) {
		network.defineEdgeAttribute(weightName, AttributeType.Float, 1);
		withEdgeBuffer(network, weightName, ({ view, bumpVersion }) => {
			for (let i = 0; i < edgeIds.length; i += 1) {
				view[edgeIds[i]] = Number(weights?.[i] ?? 1);
			}
			bumpVersion();
		});
	}
	return network;
}

test('degree and strength metrics match known directed/undirected values', async () => {
	const undirected = await buildNetwork({
		directed: false,
		nodeCount: 4,
		edges: [[0, 1], [0, 2], [0, 3]],
		weightName: 'w',
		weights: [2, 3, 4],
	});
	const directed = await buildNetwork({
		directed: true,
		nodeCount: 3,
		edges: [[0, 1], [2, 1]],
		weightName: 'w',
		weights: [2, 5],
	});

	try {
		const degree = undirected.measureDegree({ outNodeAttribute: 'degree_attr' });
		expect(degree.valuesByNode[0]).toBe(3);
		expect(degree.valuesByNode[1]).toBe(1);
		expect(degree.valuesByNode[2]).toBe(1);
		expect(degree.valuesByNode[3]).toBe(1);
		withNodeBuffer(undirected, 'degree_attr', ({ view }) => {
			expect(view[0]).toBe(3);
			expect(view[3]).toBe(1);
		});

		const strength = undirected.measureStrength({
			edgeWeightAttribute: 'w',
			measure: StrengthMeasure.Sum,
			outNodeAttribute: 'strength_attr',
		});
		expect(strength.valuesByNode[0]).toBe(9);
		expect(strength.valuesByNode[1]).toBe(2);
		expect(strength.valuesByNode[2]).toBe(3);
		expect(strength.valuesByNode[3]).toBe(4);
		withNodeBuffer(undirected, 'strength_attr', ({ view }) => {
			expect(view[0]).toBe(9);
			expect(view[2]).toBe(3);
		});

		const avgStrength = undirected.measureStrength({
			edgeWeightAttribute: 'w',
			measure: StrengthMeasure.Average,
		});
		expect(avgStrength.valuesByNode[0]).toBe(3);

		const inDegree = directed.measureDegree({ direction: NeighborDirection.In });
		const outDegree = directed.measureDegree({ direction: NeighborDirection.Out });
		const bothDegree = directed.measureDegree({ direction: NeighborDirection.Both });
		expect(inDegree.valuesByNode[1]).toBe(2);
		expect(outDegree.valuesByNode[0]).toBe(1);
		expect(outDegree.valuesByNode[2]).toBe(1);
		expect(bothDegree.valuesByNode[1]).toBe(2);

		const inStrength = directed.measureStrength({
			edgeWeightAttribute: 'w',
			direction: NeighborDirection.In,
			measure: StrengthMeasure.Sum,
		});
		expect(inStrength.valuesByNode[1]).toBe(7);
	} finally {
		undirected.dispose();
		directed.dispose();
	}
});

test('local clustering coefficient variants match known triangle/path values', async () => {
	const triangle = await buildNetwork({
		directed: false,
		nodeCount: 3,
		edges: [[0, 1], [1, 2], [0, 2]],
		weightName: 'w',
		weights: [1, 1, 1],
	});
	const path = await buildNetwork({
		directed: false,
		nodeCount: 3,
		edges: [[0, 1], [1, 2]],
		weightName: 'w',
		weights: [1, 1],
	});

	try {
		const unweighted = triangle.measureLocalClusteringCoefficient({
			variant: ClusteringCoefficientVariant.Unweighted,
			outNodeAttribute: 'clustering_attr',
		});
		const onnela = triangle.measureLocalClusteringCoefficient({
			variant: ClusteringCoefficientVariant.Onnela,
			edgeWeightAttribute: 'w',
		});
		const newman = triangle.measureLocalClusteringCoefficient({
			variant: ClusteringCoefficientVariant.Newman,
			edgeWeightAttribute: 'w',
		});
		for (let i = 0; i < 3; i += 1) {
			expectNear(unweighted.valuesByNode[i], 1);
			expectNear(onnela.valuesByNode[i], 1);
			expectNear(newman.valuesByNode[i], 1);
		}
		withNodeBuffer(triangle, 'clustering_attr', ({ view }) => {
			expectNear(view[0], 1);
			expectNear(view[2], 1);
		});

		const pathClustering = path.measureLocalClusteringCoefficient({
			variant: ClusteringCoefficientVariant.Unweighted,
		});
		expect(pathClustering.valuesByNode[0]).toBe(0);
		expect(pathClustering.valuesByNode[1]).toBe(0);
		expect(pathClustering.valuesByNode[2]).toBe(0);
	} finally {
		triangle.dispose();
		path.dispose();
	}
});

test('eigenvector centrality on star graph has expected center/leaf ratio', async () => {
	const network = await buildNetwork({
		directed: false,
		nodeCount: 5,
		edges: [[0, 1], [0, 2], [0, 3], [0, 4]],
	});
	try {
		const result = network.measureEigenvectorCentrality({
			maxIterations: 256,
			tolerance: 1e-8,
			executionMode: MeasurementExecutionMode.SingleThread,
			outNodeAttribute: 'eigen_attr',
		});
		expect(result.iterations).toBeGreaterThan(0);
		const center = result.valuesByNode[0];
		const leaf = result.valuesByNode[1];
		expect(center).toBeGreaterThan(leaf);
		expectNear(center / leaf, 2, 0.05);
		withNodeBuffer(network, 'eigen_attr', ({ view }) => {
			expectNear(view[0], center, 1e-4);
		});
	} finally {
		network.dispose();
	}
});

test('betweenness centrality matches known values on a path graph', async () => {
	const network = await buildNetwork({
		directed: false,
		nodeCount: 4,
		edges: [[0, 1], [1, 2], [2, 3]],
	});
	try {
		const result = network.measureBetweennessCentrality({
			normalize: true,
			executionMode: MeasurementExecutionMode.SingleThread,
			outNodeAttribute: 'betweenness_attr',
		});
		expectNear(result.valuesByNode[1], 2 / 3, 0.03);
		expectNear(result.valuesByNode[2], 2 / 3, 0.03);
		expectNear(result.valuesByNode[0], 0, 1e-6);
		expectNear(result.valuesByNode[3], 0, 1e-6);
		withNodeBuffer(network, 'betweenness_attr', ({ view }) => {
			expectNear(view[1], 2 / 3, 0.03);
			expectNear(view[3], 0, 1e-6);
		});
	} finally {
		network.dispose();
	}
});

test('coreness measurement and steppable session match known k-core values', async () => {
	const network = await buildNetwork({
		directed: false,
		nodeCount: 6,
		edges: [[0, 1], [1, 2], [2, 0], [2, 3], [3, 4]],
	});
	try {
		const measured = network.measureCoreness({
			executionMode: MeasurementExecutionMode.SingleThread,
			outNodeCorenessAttribute: 'coreness_one_shot',
		});
		expect(measured.maxCore).toBe(2);
		expect(measured.valuesByNode[0]).toBe(2);
		expect(measured.valuesByNode[1]).toBe(2);
		expect(measured.valuesByNode[2]).toBe(2);
		expect(measured.valuesByNode[3]).toBe(1);
		expect(measured.valuesByNode[4]).toBe(1);
		expect(measured.valuesByNode[5]).toBe(0);
		withNodeBuffer(network, 'coreness_one_shot', ({ view }) => {
			expect(view[2]).toBe(2);
		});

		const parallel = network.measureCoreness({
			executionMode: MeasurementExecutionMode.Parallel,
		});
		expect(parallel.maxCore).toBe(measured.maxCore);
		for (let i = 0; i < network.nodeCapacity; i += 1) {
			expect(parallel.valuesByNode[i]).toBe(measured.valuesByNode[i]);
		}

		const session = network.createCorenessSession({
			executionMode: MeasurementExecutionMode.SingleThread,
			outNodeCorenessAttribute: 'coreness_session',
		});
		let iterations = 0;
		let progressCurrentPrev = -1;
		while (!session.isComplete()) {
			const progress = session.step({ budget: 1, timeoutMs: null });
			expect(progress.progressCurrent).toBeGreaterThanOrEqual(progressCurrentPrev);
			progressCurrentPrev = progress.progressCurrent;
			iterations += 1;
			expect(iterations).toBeLessThan(10000);
		}

		const sessionResult = session.finalize();
		expect(sessionResult.maxCore).toBe(measured.maxCore);
		for (let i = 0; i < network.nodeCapacity; i += 1) {
			expect(sessionResult.valuesByNode[i]).toBe(measured.valuesByNode[i]);
		}
		withNodeBuffer(network, 'coreness_session', ({ view }) => {
			expect(view[4]).toBe(1);
		});
		session.dispose();
	} finally {
		network.dispose();
	}
});

test('weighted betweenness supports source chunk accumulation decomposition', async () => {
	const network = await buildNetwork({
		directed: false,
		nodeCount: 4,
		edges: [[0, 1], [1, 3], [0, 2], [2, 3]],
		weightName: 'w',
		weights: [1, 1, 1, 10],
	});
	try {
		const full = network.measureBetweennessCentrality({
			edgeWeightAttribute: 'w',
			normalize: false,
			executionMode: MeasurementExecutionMode.SingleThread,
		});

		const chunkA = network.measureBetweennessCentrality({
			edgeWeightAttribute: 'w',
			normalize: false,
			sourceNodes: [0, 1],
			executionMode: MeasurementExecutionMode.SingleThread,
		});
		const chunkB = network.measureBetweennessCentrality({
			edgeWeightAttribute: 'w',
			normalize: false,
			sourceNodes: [2, 3],
			accumulate: true,
			initialValues: chunkA.valuesByNode,
			executionMode: MeasurementExecutionMode.SingleThread,
		});

		for (let i = 0; i < network.nodeCapacity; i += 1) {
			expectNear(chunkB.valuesByNode[i], full.valuesByNode[i], 1e-4);
		}
		expect(full.valuesByNode[1]).toBeGreaterThan(full.valuesByNode[2]);
	} finally {
		network.dispose();
	}
});

test('connected components supports weak/strong modes, sessions, and extraction helpers', async () => {
	const network = await buildNetwork({
		directed: true,
		nodeCount: 6,
		edges: [[0, 1], [1, 0], [1, 2], [3, 4], [4, 3]],
	});
	try {
		const measured = network.measureConnectedComponents({
			outNodeComponentAttribute: 'component_one_shot',
		});
		expect(measured.componentCount).toBe(3);
		expect(measured.largestComponentSize).toBe(3);
		const oneShot = measured.valuesByNode;
			expect(oneShot[0]).toBe(oneShot[1]);
			expect(oneShot[2]).toBe(oneShot[1]);
			expect(oneShot[3]).toBe(oneShot[4]);
			expect(oneShot[0]).not.toBe(oneShot[3]);
			expect(oneShot[5]).toBeGreaterThan(0);
			withNodeBuffer(network, 'component_one_shot', ({ view }) => {
				expect(view[5]).toBe(oneShot[5]);
			});

		const session = network.createConnectedComponentsSession({
			outNodeComponentAttribute: 'component_session',
		});
		let iterations = 0;
		let progressCurrentPrev = -1;
		while (!session.isComplete()) {
			const progress = session.step({ budget: 1, timeoutMs: null });
			expect(progress.progressCurrent).toBeGreaterThanOrEqual(progressCurrentPrev);
			progressCurrentPrev = progress.progressCurrent;
			iterations += 1;
			expect(iterations).toBeLessThan(10000);
		}

		const sessionResult = session.finalize();
		expect(sessionResult.componentCount).toBe(measured.componentCount);
		expect(sessionResult.largestComponentSize).toBe(measured.largestComponentSize);
		for (let i = 0; i < network.nodeCapacity; i += 1) {
			expect(sessionResult.valuesByNode[i]).toBe(oneShot[i]);
			}
			withNodeBuffer(network, 'component_session', ({ view }) => {
				expect(view[0]).toBe(oneShot[0]);
			});
			session.dispose();

			const strong = network.measureConnectedComponents({
				mode: ConnectedComponentsMode.Strong,
				outNodeComponentAttribute: 'component_strong',
			});
			expect(strong.componentCount).toBe(4);
			expect(strong.largestComponentSize).toBe(2);
			expect(strong.valuesByNode[0]).toBe(strong.valuesByNode[1]);
			expect(strong.valuesByNode[2]).not.toBe(strong.valuesByNode[1]);
			expect(strong.valuesByNode[3]).toBe(strong.valuesByNode[4]);
			expect(strong.valuesByNode[5]).not.toBe(strong.valuesByNode[3]);
			withNodeBuffer(network, 'component_strong', ({ view }) => {
				expect(view[4]).toBe(strong.valuesByNode[4]);
			});

			const strongSession = network.createConnectedComponentsSession({
				mode: ConnectedComponentsMode.Strong,
				outNodeComponentAttribute: 'component_strong_session',
			});
			let strongIterations = 0;
			let strongPrevProgress = -1;
			while (!strongSession.isComplete()) {
				const progress = strongSession.step({ budget: 1, timeoutMs: null });
				expect(progress.progressCurrent).toBeGreaterThanOrEqual(strongPrevProgress);
				strongPrevProgress = progress.progressCurrent;
				strongIterations += 1;
				expect(strongIterations).toBeLessThan(10000);
			}
			const strongSessionResult = strongSession.finalize();
			expect(strongSessionResult.componentCount).toBe(strong.componentCount);
			expect(strongSessionResult.largestComponentSize).toBe(strong.largestComponentSize);
			for (let i = 0; i < network.nodeCapacity; i += 1) {
				expect(strongSessionResult.valuesByNode[i]).toBe(strong.valuesByNode[i]);
			}
			strongSession.dispose();

			const extracted = network.extractConnectedComponents({ mode: 'strong' });
			expect(extracted.length).toBe(4);
			expect(extracted[0].size).toBe(2);
			expect(extracted[1].size).toBe(2);
			expect(extracted[2].size).toBe(1);
			expect(extracted[3].size).toBe(1);

			const largest = network.extractLargestConnectedComponent({ mode: 'strong', asNetwork: true });
			expect(largest).not.toBeNull();
			expect(largest.size).toBe(2);
			expect(largest.network.nodeCount).toBe(2);
			expect(largest.network.edgeCount).toBe(2);
			largest.network.dispose();
		} finally {
			network.dispose();
		}
	});

test('eigenvector centrality supports iterative stepping via initialValues', async () => {
	const network = await buildNetwork({
		directed: false,
		nodeCount: 6,
		edges: [[0, 1], [1, 2], [2, 3], [3, 4], [4, 5], [2, 5]],
	});
	try {
		const full = network.measureEigenvectorCentrality({
			maxIterations: 256,
			tolerance: 1e-8,
		});

		let current = null;
		for (let step = 0; step < 20; step += 1) {
			const next = network.measureEigenvectorCentrality({
				maxIterations: 8,
				tolerance: 1e-8,
				initialValues: current,
			});
			current = next.valuesByNode;
			if (next.converged) {
				break;
			}
		}

		for (let i = 0; i < network.nodeCapacity; i += 1) {
			expectNear(current[i], full.valuesByNode[i], 0.02);
		}
	} finally {
		network.dispose();
	}
});
