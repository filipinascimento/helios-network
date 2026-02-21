import { loadHelios } from '../utils/load-helios.mjs';

function parseArgs(argv) {
	const quick = argv.includes('--quick');
	const large = argv.includes('--large');
	if (quick && large) {
		throw new Error('Use either --quick or --large, not both');
	}
	return {
		quick,
		large,
	};
}

function makeConfigs({ quick, large }) {
	if (quick) {
		return [
			{ label: '1D', sides: [96], maxLevel: 6 },
			{ label: '2D', sides: [18, 18], maxLevel: 6 },
			{ label: '3D', sides: [8, 8, 8], maxLevel: 4 },
			{ label: '4D', sides: [6, 6, 6, 6], maxLevel: 3 },
		];
	}
	if (large) {
		return [
			{ label: '1D', sides: [32768], maxLevel: 8, sampleNodes: 64 },
			{ label: '2D', sides: [224, 224], maxLevel: 8, sampleNodes: 64 },
			{ label: '3D', sides: [32, 32, 32], maxLevel: 6, sampleNodes: 48 },
			{ label: '4D', sides: [14, 14, 14, 14], maxLevel: 4, sampleNodes: 32 },
		];
	}
	return [
		{ label: '1D', sides: [256], maxLevel: 8 },
		{ label: '2D', sides: [32, 32], maxLevel: 8 },
		{ label: '3D', sides: [12, 12, 12], maxLevel: 6 },
		{ label: '4D', sides: [8, 8, 8, 8], maxLevel: 4 },
	];
}

function stridesFromSides(sides) {
	const strides = new Array(sides.length).fill(1);
	for (let d = 1; d < sides.length; d += 1) {
		strides[d] = strides[d - 1] * sides[d - 1];
	}
	return strides;
}

function coordinatesFromLinear(index, sides) {
	const out = new Array(sides.length).fill(0);
	let value = index;
	for (let d = 0; d < sides.length; d += 1) {
		out[d] = value % sides[d];
		value = Math.floor(value / sides[d]);
	}
	return out;
}

function linearFromCoordinates(coords, strides) {
	let index = 0;
	for (let d = 0; d < coords.length; d += 1) {
		index += coords[d] * strides[d];
	}
	return index;
}

async function createToroidalNetwork(HeliosNetwork, sides) {
	const totalNodes = sides.reduce((acc, v) => acc * v, 1);
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	const nodes = network.addNodes(totalNodes);
	const strides = stridesFromSides(sides);

	const edges = new Array(totalNodes * sides.length);
	let cursor = 0;
	for (let idx = 0; idx < totalNodes; idx += 1) {
		const coords = coordinatesFromLinear(idx, sides);
		for (let d = 0; d < sides.length; d += 1) {
			const neighbor = coords.slice();
			neighbor[d] = (neighbor[d] + 1) % sides[d];
			const nIdx = linearFromCoordinates(neighbor, strides);
			edges[cursor] = { from: nodes[idx], to: nodes[nIdx] };
			cursor += 1;
		}
	}
	network.addEdges(edges);
	return network;
}

function formatRows(rows) {
	const headers = ['Dim', 'Sides', 'Nodes', 'Edges', 'Max local d_i(r)', 'Max global D(r)'];
	const fields = rows.map((row) => [
		row.label,
		`[${row.sides.join('x')}]`,
		String(row.nodeCount),
		String(row.edgeCount),
		row.maxLocal.toFixed(3),
		row.maxGlobal.toFixed(3),
	]);
	const widths = headers.map((header, col) => {
		let w = header.length;
		for (const line of fields) {
			w = Math.max(w, line[col].length);
		}
		return w;
	});
	const pad = (value, col) => value.padEnd(widths[col], ' ');
	const divider = widths.map((w) => '-'.repeat(w)).join('-+-');
	console.log(headers.map((h, i) => pad(h, i)).join(' | '));
	console.log(divider);
	for (const line of fields) {
		console.log(line.map((v, i) => pad(v, i)).join(' | '));
	}
}

async function main() {
	const args = parseArgs(process.argv.slice(2));
	const helios = await loadHelios();
	if (!helios.DimensionDifferenceMethod) {
		throw new Error(
			'Dimension APIs are not available in the loaded bundle. Run `npm run build` to refresh dist/ artifacts.'
		);
	}
	const { default: HeliosNetwork, DimensionDifferenceMethod } = helios;
	const configs = makeConfigs(args);
	const rows = [];

	const mode = args.quick ? 'quick' : args.large ? 'large' : 'full';
	console.log(`[init] Running toroidal dimension measurements (${mode} mode)`);
	for (const config of configs) {
		const network = await createToroidalNetwork(HeliosNetwork, config.sides);
		try {
			const sampleCap = config.sampleNodes ?? 192;
			const sampleCount = Math.min(sampleCap, network.nodeCount);
			const sampleNodes = Array.from(network.nodeIndices.slice(0, sampleCount));
			const local = network.measureNodeDimension(0, {
				maxLevel: config.maxLevel,
				method: DimensionDifferenceMethod.LeastSquares,
				order: 2,
			});
			const global = network.measureDimension({
				maxLevel: config.maxLevel,
				method: DimensionDifferenceMethod.LeastSquares,
				order: 2,
				nodes: sampleNodes,
			});

			const maxLocal = local.dimension.reduce((acc, value) => Math.max(acc, value), Number.NEGATIVE_INFINITY);
			const maxGlobal = global.globalDimension.reduce((acc, value) => Math.max(acc, value), Number.NEGATIVE_INFINITY);

			rows.push({
				label: config.label,
				sides: config.sides,
				nodeCount: network.nodeCount,
				edgeCount: network.edgeCount,
				maxLocal,
				maxGlobal,
			});

			console.log(`[${config.label}] local curve: ${Array.from(local.dimension).map((v) => v.toFixed(3)).join(', ')}`);
		} finally {
			network.dispose();
		}
	}

	console.log('\n[max dimension table]');
	formatRows(rows);
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
