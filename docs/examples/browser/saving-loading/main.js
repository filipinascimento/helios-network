import { loadHelios } from '../utils/load-helios.js';

const output = document.getElementById('output');
const downloadButton = document.getElementById('download');
const uploadButton = document.getElementById('upload');
const fileInput = document.getElementById('file-input');

let activeNetwork;
let HeliosNetworkCtor;

function log(message) {
	console.log(message);
	if (output) {
		output.textContent += `${message}\n`;
	}
}

function base64ToUint8Array(base64) {
	const binary = atob(base64);
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; i += 1) {
		bytes[i] = binary.charCodeAt(i);
	}
	return bytes;
}

function randomInteger(min, max) {
	return Math.floor(Math.random() * (max - min + 1)) + min;
}

function randomFloat(min, max) {
	return Math.random() * (max - min) + min;
}

function getAverageDegree(network) {
	if (!network || network.nodeCount === 0) {
		return 0;
	}
	return (network.edgeCount * 2) / network.nodeCount;
}

function formatStats(network) {
	return `nodes=${network.nodeCount}, edges=${network.edgeCount}, avgDegree=${getAverageDegree(network).toFixed(2)}`;
}

function triggerDownload(uint8array, filename) {
	const blob = new Blob([uint8array], { type: 'application/octet-stream' });
	const url = URL.createObjectURL(blob);
	const anchor = document.createElement('a');
	anchor.href = url;
	anchor.download = filename;
	document.body.appendChild(anchor);
	anchor.click();
	anchor.remove();
	URL.revokeObjectURL(url);
}

async function createRandomNetwork() {
	const nodeCount = randomInteger(100, 500);
	const targetAverageDegree = randomFloat(4, 8);
	const network = await HeliosNetworkCtor.create({ directed: false, initialNodes: nodeCount });

	const targetEdges = Math.round((nodeCount * targetAverageDegree) / 2);
	const edges = [];
	for (let i = 0; i < targetEdges; i += 1) {
		const from = randomInteger(0, nodeCount - 1);
		let to = randomInteger(0, nodeCount - 1);
		if (from === to && nodeCount > 1) {
			to = (to + 1) % nodeCount;
		}
		edges.push({ from, to });
	}
	if (edges.length > 0) {
		network.addEdges(edges);
	}

	log(`Generated random network (target avgDegree=${targetAverageDegree.toFixed(2)}, targetEdges≈${targetEdges}).`);
	log(`Actual stats → ${formatStats(network)}`);

	return network;
}

function wireControls() {
	if (downloadButton) {
		downloadButton.addEventListener('click', async () => {
			if (!activeNetwork) {
				log('Download aborted: network not ready.');
				return;
			}
			const base64 = await activeNetwork.saveZXNet({ format: 'base64', compressionLevel: 3 });
			const data = base64ToUint8Array(base64);
			const filename = `helios-network-${Date.now()}.zxnet`;
			triggerDownload(data, filename);
			log(`Downloaded ${filename} → ${formatStats(activeNetwork)}`);
		});
	}

	if (uploadButton && fileInput) {
		uploadButton.addEventListener('click', () => {
			fileInput.click();
		});

		fileInput.addEventListener('change', async (event) => {
			const [file] = event.target.files || [];
			if (!file) {
				return;
			}

			try {
				const buffer = await file.arrayBuffer();
				const restored = await HeliosNetworkCtor.fromZXNet(new Uint8Array(buffer));
				if (activeNetwork) {
					activeNetwork.dispose();
					log('Previous network disposed.');
				}
				activeNetwork = restored;
				log(`Uploaded ${file.name} → ${formatStats(restored)}`);
			} catch (error) {
				log(`Upload failed: ${error.message}`);
			} finally {
				fileInput.value = '';
			}
		});
	}
}

async function demoPersistence(network) {
	if (typeof network.saveBXNet !== 'function') {
		return;
	}

	const blob = await network.saveBXNet({ format: 'blob' });
	log(`Saved .bxnet blob (${blob.size} bytes) → ${formatStats(network)}`);

	const restoredBX = await HeliosNetworkCtor.fromBXNet(blob);
	log(`Restored from .bxnet → ${formatStats(restoredBX)}`);
	restoredBX.dispose();

	const base64 = await network.saveZXNet({ format: 'base64', compressionLevel: 3 });
	log(`Saved .zxnet base64 (preview: ${base64.slice(0, 24)}…) → ${formatStats(network)}`);

	const decoded = base64ToUint8Array(base64);
	const restoredZX = await HeliosNetworkCtor.fromZXNet(decoded);
	log(`Restored from .zxnet → ${formatStats(restoredZX)}`);
	restoredZX.dispose();
}

async function run() {
	const { default: HeliosNetwork } = await loadHelios();
	HeliosNetworkCtor = HeliosNetwork;
	wireControls();

	const demoNetwork = await createRandomNetwork();
	await demoPersistence(demoNetwork);
	demoNetwork.dispose();
	log('Network disposed after demo run.');

	activeNetwork = await createRandomNetwork();
	log('Use the buttons above to download or upload a network at any time.');
}

run().catch((error) => {
	console.error(error);
	log(`Example failed: ${error.message}`);
});
