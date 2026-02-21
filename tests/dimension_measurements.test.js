import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType, DimensionDifferenceMethod } from '../src/helios-network.js';

function linearIndex(coords, strides) {
	let index = 0;
	for (let d = 0; d < coords.length; d += 1) {
		index += coords[d] * strides[d];
	}
	return index;
}

function coordinatesFromLinear(index, sides) {
	const coords = new Array(sides.length).fill(0);
	let value = index;
	for (let d = 0; d < sides.length; d += 1) {
		coords[d] = value % sides[d];
		value = Math.floor(value / sides[d]);
	}
	return coords;
}

async function createToroidalNetwork(sides) {
	const totalNodes = sides.reduce((acc, value) => acc * value, 1);
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	const nodes = network.addNodes(totalNodes);

	const strides = new Array(sides.length).fill(1);
	for (let d = 1; d < sides.length; d += 1) {
		strides[d] = strides[d - 1] * sides[d - 1];
	}

	const edges = [];
	for (let idx = 0; idx < totalNodes; idx += 1) {
		const coords = coordinatesFromLinear(idx, sides);
		for (let d = 0; d < sides.length; d += 1) {
			const neighborCoords = coords.slice();
			neighborCoords[d] = (neighborCoords[d] + 1) % sides[d];
			const neighborLinear = linearIndex(neighborCoords, strides);
			edges.push({ from: nodes[idx], to: nodes[neighborLinear] });
		}
	}
	network.addEdges(edges);
	return network;
}

test('measureNodeDimension matches toroidal 2D lattice capacities at small radii', async () => {
	const network = await createToroidalNetwork([16, 16]);
	try {
		const nodeMeasure = network.measureNodeDimension(0, {
			maxLevel: 3,
			method: DimensionDifferenceMethod.LeastSquares,
			order: 2,
		});
		expect(Array.from(nodeMeasure.capacity)).toEqual([1, 5, 13, 25]);
		expect(nodeMeasure.dimension[2]).toBe(0);
		expect(nodeMeasure.dimension[3]).toBeGreaterThan(1.4);
	} finally {
		network.dispose();
	}
});

test('measureDimension captures dimensional gap between 1D and 2D toroidal regular networks', async () => {
	const ring = await createToroidalNetwork([96]);
	const torus2d = await createToroidalNetwork([32, 32]);
	try {
		const ringNodes = Array.from({ length: 64 }, (_, i) => i);
		const torusNodes = Array.from({ length: 128 }, (_, i) => i);
		const ringStats = ring.measureDimension({
			maxLevel: 8,
			method: 'leastsquares',
			order: 2,
			nodes: ringNodes,
		});
		const torusStats = torus2d.measureDimension({
			maxLevel: 8,
			method: 'leastsquares',
			order: 2,
			nodes: torusNodes,
		});

		const ringDim = ringStats.globalDimension[6];
		const torusDim = torusStats.globalDimension[6];
		expect(ringDim).toBeGreaterThan(0.8);
		expect(ringDim).toBeLessThan(1.2);
		expect(torusDim).toBeGreaterThan(1.6);
		expect(torusDim).toBeLessThan(2.3);
		expect(torusDim).toBeGreaterThan(ringDim + 0.6);
	} finally {
		ring.dispose();
		torus2d.dispose();
	}
});

test('measureDimension supports FW/BK/CE/LS estimators and node subsets', async () => {
	const network = await createToroidalNetwork([24, 24]);
	try {
		const methods = ['forward', 'backward', 'central', 'leastsquares'];
		for (const method of methods) {
			const stats = network.measureDimension({
				maxLevel: 6,
				method,
				order: 2,
				nodes: [0, 1, 2, 3, 4, 5],
			});
			expect(stats.selectedCount).toBe(6);
			expect(Number.isFinite(stats.globalDimension[3])).toBe(true);
			expect(Number.isFinite(stats.averageNodeDimension[3])).toBe(true);
			expect(stats.averageCapacity[3]).toBeGreaterThan(0);
		}
	} finally {
		network.dispose();
	}
});

test('createDimensionSession supports stepwise progress and node max/vector outputs', async () => {
	const network = await createToroidalNetwork([20, 20]);
	try {
		const selectedNodes = Array.from({ length: 60 }, (_, i) => i);
		const session = network.createDimensionSession({
			maxLevel: 5,
			method: 'leastsquares',
			order: 2,
			nodes: selectedNodes,
			outNodeMaxDimensionAttribute: 'dim_max',
			outNodeDimensionLevelsAttribute: 'dim_levels',
			dimensionLevelsEncoding: 'vector',
		});
		try {
			const initial = session.getProgress();
			expect(initial.phase).toBe(0);
			expect(initial.progressCurrent).toBe(0);
			expect(initial.progressTotal).toBe(selectedNodes.length);

			let progress = initial;
			while (!session.isComplete()) {
				progress = session.step({ budget: 7, timeoutMs: null });
			}
			expect(progress.phase).toBe(5);
			expect(progress.progressCurrent).toBe(selectedNodes.length);

			const result = session.finalize();
			expect(result.selectedCount).toBe(selectedNodes.length);
			expect(result.globalDimension[4]).toBeGreaterThan(1.4);
			expect(result.globalDimension[4]).toBeLessThan(2.4);

			const maxInfo = network.getNodeAttributeInfo('dim_max');
			expect(maxInfo?.type).toBe(AttributeType.Float);
			expect(maxInfo?.dimension).toBe(1);

			const levelsInfo = network.getNodeAttributeInfo('dim_levels');
			expect(levelsInfo?.type).toBe(AttributeType.Float);
			expect(levelsInfo?.dimension).toBe(6);

			const maxView = network.getNodeAttributeBuffer('dim_max').view;
			expect(maxView[selectedNodes[0]]).toBeGreaterThan(1.2);

			const levelsView = network.getNodeAttributeBuffer('dim_levels').view;
			const base = selectedNodes[0] * 6;
			expect(levelsView[base + 2]).toBe(0);
			expect(levelsView[base + 3]).toBeGreaterThan(1.2);
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
});

test('createDimensionSession can write concentric levels as JSON strings', async () => {
	const network = await createToroidalNetwork([24]);
	try {
		const session = network.createDimensionSession({
			maxLevel: 4,
			method: 'central',
			order: 1,
			nodes: [0, 1, 2, 3],
			captureNodeDimensionProfiles: true,
			outNodeMaxDimensionAttribute: 'dim_max_string',
		});
		try {
			await session.run({
				stepOptions: { budget: 1, timeoutMs: null },
				yield: () => Promise.resolve(),
				maxIterations: 64,
			});
			const result = session.finalize({
				outNodeDimensionLevelsAttribute: 'dim_levels_json',
				dimensionLevelsEncoding: 'string',
				dimensionLevelsStringPrecision: 4,
			});
			expect(result.selectedCount).toBe(4);

			const info = network.getNodeAttributeInfo('dim_levels_json');
			expect(info?.type).toBe(AttributeType.String);
			expect(info?.dimension).toBe(1);

			const raw = network.getNodeStringAttribute('dim_levels_json', 0);
			expect(typeof raw).toBe('string');
			const parsed = JSON.parse(raw);
			expect(Array.isArray(parsed)).toBe(true);
			expect(parsed.length).toBe(5);
			expect(parsed[2]).toBeGreaterThan(0.7);
			expect(parsed[2]).toBeLessThan(1.3);
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
});

test('dimension session cancels when topology changes', async () => {
	const network = await createToroidalNetwork([16, 16]);
	try {
		const session = network.createDimensionSession({
			maxLevel: 3,
			nodes: [0, 1, 2, 3],
		});
		try {
			network.addNodes(1);
			expect(() => session.step({ budget: 1 })).toThrow(/Session canceled/i);
			expect(() => session.getProgress()).toThrow(/Session canceled/i);
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
});

test('dimension order bounds follow CV limits for FW/BK/CE', async () => {
	const network = await createToroidalNetwork([24, 24]);
	try {
		expect(() => network.measureNodeDimension(0, { maxLevel: 6, method: 'forward', order: 7 })).toThrow(/order/i);
		expect(() => network.measureNodeDimension(0, { maxLevel: 6, method: 'backward', order: 7 })).toThrow(/order/i);
		expect(() => network.measureNodeDimension(0, { maxLevel: 6, method: 'central', order: 5 })).toThrow(/order/i);
		expect(() => network.createDimensionSession({ maxLevel: 6, method: 'central', order: 5, nodes: [0, 1, 2] })).toThrow(/order/i);
	} finally {
		network.dispose();
	}
});

test('dimension session global curve matches direct measureDimension for same subset', async () => {
	const network = await createToroidalNetwork([28, 28]);
	try {
		const nodes = Array.from({ length: 96 }, (_, i) => i);
		const direct = network.measureDimension({
			maxLevel: 7,
			method: 'forward',
			order: 2,
			nodes,
		});

		const session = network.createDimensionSession({
			maxLevel: 7,
			method: 'forward',
			order: 2,
			nodes,
		});
		try {
			while (!session.isComplete()) {
				session.step({ budget: 9, timeoutMs: null });
			}
			const stepped = session.finalize();
			expect(stepped.selectedCount).toBe(direct.selectedCount);
			for (let r = 0; r <= 7; r += 1) {
				expect(Math.abs(stepped.globalDimension[r] - direct.globalDimension[r])).toBeLessThan(1e-5);
				expect(Math.abs(stepped.averageCapacity[r] - direct.averageCapacity[r])).toBeLessThan(1e-5);
			}
		} finally {
			session.dispose();
		}
	} finally {
		network.dispose();
	}
});
