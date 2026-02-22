import { expect, test } from 'vitest';
import HeliosNetwork, {
	AttributeType,
	NeighborDirection,
	StrengthMeasure,
	ClusteringCoefficientVariant,
	MeasurementExecutionMode,
} from '../src/helios-network.js';

function expectNear(actual, expected, epsilon = 1e-4) {
	expect(Math.abs(actual - expected)).toBeLessThanOrEqual(epsilon);
}

async function buildNetwork({ directed = false, nodeCount = 0, edges = [], weightName = null, weights = null }) {
	const network = await HeliosNetwork.create({ directed, initialNodes: 0, initialEdges: 0 });
	network.addNodes(nodeCount);
	const edgeIds = network.addEdges(edges.map(([from, to]) => ({ from, to })));
	if (weightName) {
		network.defineEdgeAttribute(weightName, AttributeType.Float, 1);
		const { view, bumpVersion } = network.getEdgeAttributeBuffer(weightName);
		for (let i = 0; i < edgeIds.length; i += 1) {
			view[edgeIds[i]] = Number(weights?.[i] ?? 1);
		}
		bumpVersion();
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
		const degree = undirected.measureDegree();
		expect(degree.valuesByNode[0]).toBe(3);
		expect(degree.valuesByNode[1]).toBe(1);
		expect(degree.valuesByNode[2]).toBe(1);
		expect(degree.valuesByNode[3]).toBe(1);

		const strength = undirected.measureStrength({
			edgeWeightAttribute: 'w',
			measure: StrengthMeasure.Sum,
		});
		expect(strength.valuesByNode[0]).toBe(9);
		expect(strength.valuesByNode[1]).toBe(2);
		expect(strength.valuesByNode[2]).toBe(3);
		expect(strength.valuesByNode[3]).toBe(4);

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
		});
		expect(result.iterations).toBeGreaterThan(0);
		const center = result.valuesByNode[0];
		const leaf = result.valuesByNode[1];
		expect(center).toBeGreaterThan(leaf);
		expectNear(center / leaf, 2, 0.05);
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
		});
		expectNear(result.valuesByNode[1], 2 / 3, 0.03);
		expectNear(result.valuesByNode[2], 2 / 3, 0.03);
		expectNear(result.valuesByNode[0], 0, 1e-6);
		expectNear(result.valuesByNode[3], 0, 1e-6);
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
