import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { loadHelios } from '../utils/load-helios.mjs';

// Published package usage: `import HeliosNetwork, { AttributeType } from 'helios-network';`

function log(step, payload) {
	console.log(`[${step}]`, payload);
}

async function main() {
	const { default: HeliosNetwork, AttributeType } = await loadHelios();
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 1, initialEdges: 0 });
	let zxnetPath;

	try {
		const nodes = network.addNodes(2);
		const edges = network.addEdges([{ from: nodes[0], to: nodes[1] }]);
		log('structure', { nodes: Array.from(nodes), edges: Array.from(edges) });

		network.defineNodeAttribute('weight', AttributeType.Float, 1);
		network.getNodeAttributeBuffer('weight').view[nodes[0]] = 3.5;

		const bxPayload = await network.saveBXNet();
		log('saveBXNet', `In-memory payload size → ${bxPayload.byteLength} bytes`);

		const restored = await HeliosNetwork.fromBXNet(bxPayload);
		try {
			log('fromBXNet', `Restored nodeCount=${restored.nodeCount}, edgeCount=${restored.edgeCount}`);
		} finally {
			restored.dispose();
		}

		zxnetPath = path.join(os.tmpdir(), `helios-${Date.now()}.zxnet`);
		await network.saveZXNet({ path: zxnetPath, compressionLevel: 4 });
		const stats = await fs.stat(zxnetPath);
		log('saveZXNet:path', `Wrote ${stats.size} bytes to ${zxnetPath}`);

		const base64 = await network.saveZXNet({ format: 'base64', compressionLevel: 4 });
		log('saveZXNet:base64', `Preview → ${base64.slice(0, 28)}...`);

		const reloaded = await HeliosNetwork.fromZXNet(zxnetPath);
		try {
			log('fromZXNet', `Restored nodeCount=${reloaded.nodeCount}`);
		} finally {
			reloaded.dispose();
		}
	} finally {
		network.dispose();
		log('teardown', 'Network disposed');
		if (zxnetPath) {
			await fs.rm(zxnetPath, { force: true }).catch(() => {});
		}
	}
}

main().catch((error) => {
	console.error('[error]', error);
	process.exitCode = 1;
});
