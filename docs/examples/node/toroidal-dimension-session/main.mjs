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
			{ label: '1D', sides: [80], maxLevel: 5 },
			{ label: '2D', sides: [16, 16], maxLevel: 5 },
			{ label: '3D', sides: [8, 8, 8], maxLevel: 4 },
			{ label: '4D', sides: [6, 6, 6, 6], maxLevel: 3 },
		];
	}
	if (large) {
		return [
			{ label: '1D', sides: [32768], maxLevel: 8, sampleNodes: 48 },
			{ label: '2D', sides: [224, 224], maxLevel: 8, sampleNodes: 48 },
			{ label: '3D', sides: [32, 32, 32], maxLevel: 6, sampleNodes: 32 },
			{ label: '4D', sides: [14, 14, 14, 14], maxLevel: 4, sampleNodes: 24 },
		];
	}
	return [
		{ label: '1D', sides: [192], maxLevel: 8 },
		{ label: '2D', sides: [28, 28], maxLevel: 8 },
		{ label: '3D', sides: [10, 10, 10], maxLevel: 6 },
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

function printTable(rows) {
	const headers = ['Dim', 'Stored max attr name', 'Max global D(r)', 'First sample max d_i'];
	const lines = rows.map((row) => [
		row.label,
		row.maxAttrName,
		row.maxGlobal.toFixed(3),
		row.firstSampleStoredMax.toFixed(3),
	]);
	const widths = headers.map((header, col) => {
		let w = header.length;
		for (const line of lines) {
			w = Math.max(w, line[col].length);
		}
		return w;
	});
	const pad = (value, col) => value.padEnd(widths[col], ' ');
	const divider = widths.map((w) => '-'.repeat(w)).join('-+-');
	console.log(headers.map((h, i) => pad(h, i)).join(' | '));
	console.log(divider);
	for (const line of lines) {
		console.log(line.map((value, i) => pad(value, i)).join(' | '));
	}
}

async function runConfig(network, config) {
	const sampleCap = config.sampleNodes ?? 128;
	const sampleNodes = Array.from(network.nodeIndices.slice(0, Math.min(sampleCap, network.nodeCount)));
	const id = config.label.toLowerCase();
	const outNodeMaxDimensionAttribute = `dim_max_${id}`;
	const outNodeDimensionLevelsAttribute = `dim_levels_${id}`;

	const session = network.createDimensionSession({
		maxLevel: config.maxLevel,
		method: 'leastsquares',
		order: 2,
		nodes: sampleNodes,
		captureNodeDimensionProfiles: true,
		outNodeMaxDimensionAttribute,
		outNodeDimensionLevelsAttribute,
		dimensionLevelsEncoding: 'vector',
	});

	try {
		let progress = session.getProgress();
		while (!session.isComplete()) {
			progress = session.step({ budget: 6, timeoutMs: 3, chunkBudget: 6 });
			if (progress.phase !== 5 && progress.processedNodes % 48 === 0) {
				console.log(`[${config.label}] progress ${progress.processedNodes}/${progress.nodeCount}`);
			}
		}
		const result = session.finalize();
		const maxGlobal = result.globalDimension.reduce((acc, value) => Math.max(acc, value), Number.NEGATIVE_INFINITY);
		const { view } = network.getNodeAttributeBuffer(outNodeMaxDimensionAttribute);
		return {
			label: config.label,
			maxAttrName: outNodeMaxDimensionAttribute,
			maxGlobal,
			firstSampleStoredMax: view[sampleNodes[0]],
		};
	} finally {
		session.dispose();
	}
}

async function main() {
	const args = parseArgs(process.argv.slice(2));
	const helios = await loadHelios();
	if (typeof helios.default?.prototype?.createDimensionSession !== 'function') {
		throw new Error(
			'createDimensionSession() is not available in the loaded bundle. Run `npm run build` to refresh dist/ artifacts.'
		);
	}
	const { default: HeliosNetwork } = helios;
	const configs = makeConfigs(args);
	const rows = [];

	const mode = args.quick ? 'quick' : args.large ? 'large' : 'full';
	console.log(`[init] Running steppable toroidal dimension sessions (${mode} mode)`);
	for (const config of configs) {
		const network = await createToroidalNetwork(HeliosNetwork, config.sides);
		try {
			rows.push(await runConfig(network, config));
		} finally {
			network.dispose();
		}
	}

	console.log('\n[session results]');
	printTable(rows);
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
