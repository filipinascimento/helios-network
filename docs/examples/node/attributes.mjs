import HeliosNetwork, { AttributeType } from '../../../dist/helios-network.js';

function section(title) {
	console.log(`\n=== ${title} ===`);
}

function formatVector(view, index, dimension) {
	const start = index * dimension;
	return Array.from(view.slice(start, start + dimension));
}

async function main() {
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });

	try {
		section('Setup');
		const nodes = Array.from(network.addNodes(3));
		const edges = Array.from(network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
		]));
		console.log('Nodes:', nodes);
		console.log('Edges:', edges);

		section('Node attributes');
		network.defineNodeAttribute('label', AttributeType.String);
		network.defineNodeAttribute('active', AttributeType.Boolean);
		network.defineNodeAttribute('position', AttributeType.Float, 3);
		network.defineNodeAttribute('rank', AttributeType.Integer);
		network.defineNodeAttribute('metadata', AttributeType.Javascript);

		nodes.forEach((node, idx) => network.setNodeStringAttribute('label', node, `node-${idx}`));

		const active = network.getNodeAttributeBuffer('active');
		const position = network.getNodeAttributeBuffer('position');
		const rank = network.getNodeAttributeBuffer('rank');
		const metadata = network.getNodeAttributeBuffer('metadata');

		active.view[nodes[0]] = 1;
		active.view[nodes[1]] = 0;
		active.view[nodes[2]] = 1;

		nodes.forEach((node, idx) => {
			const base = node * position.dimension;
			position.view[base + 0] = idx * 10 + 0.1;
			position.view[base + 1] = idx * 10 + 0.2;
			position.view[base + 2] = idx * 10 + 0.3;
		});

		rank.view[nodes[0]] = 1n;
		rank.view[nodes[1]] = 2n;
		rank.view[nodes[2]] = 3n;

		metadata.set(nodes[0], { role: 'source' });
		metadata.set(nodes[1], { role: 'relay', notes: ['beta'] });

		console.log('label[0]:', network.getNodeStringAttribute('label', nodes[0]));
		console.log('active flags:', nodes.map((node) => active.view[node]));
		console.log('position[2]:', formatVector(position.view, nodes[2], position.dimension));
		console.log('rank[1]:', rank.view[nodes[1]].toString());
		console.log('metadata[1]:', metadata.get(nodes[1]));

		metadata.set(nodes[1], { role: 'relay', notes: ['beta', 'patched'] });
		metadata.delete(nodes[0]);
		network.setNodeStringAttribute('label', nodes[1], null);
		console.log('metadata[0] after delete:', metadata.get(nodes[0]));
		console.log('label[1] after clear:', network.getNodeStringAttribute('label', nodes[1]));

		section('Edge attributes');
		network.defineEdgeAttribute('weight', AttributeType.Double);
		network.defineEdgeAttribute('traffic', AttributeType.UnsignedInteger);
		network.defineEdgeAttribute('kind', AttributeType.Category);
		network.defineEdgeAttribute('payload', AttributeType.Data);
		network.defineEdgeAttribute('marker', AttributeType.String);

		const weight = network.getEdgeAttributeBuffer('weight');
		const traffic = network.getEdgeAttributeBuffer('traffic');
		const kind = network.getEdgeAttributeBuffer('kind');
		const payload = network.getEdgeAttributeBuffer('payload');

		edges.forEach((edge, idx) => {
			weight.view[edge] = idx === 0 ? 0.5 : 0.9;
			traffic.view[edge] = BigInt(1_000 + idx * 250);
			kind.view[edge] = idx; // 0 -> category A, 1 -> category B
			payload.set(edge, Buffer.from([edge, idx]));
			network.setEdgeStringAttribute('marker', edge, `edge-${edge}`);
		});

		console.log('weights:', edges.map((edge) => weight.view[edge]));
		console.log('traffic:', edges.map((edge) => traffic.view[edge].toString()));
		console.log('kind raw values:', edges.map((edge) => kind.view[edge]));
		console.log('payload[0]:', payload.get(edges[0]));
		console.log('marker[1]:', network.getEdgeStringAttribute('marker', edges[1]));

		payload.delete(edges[0]);
		network.setEdgeStringAttribute('marker', edges[1], null);
		console.log('payload[0] after delete:', payload.get(edges[0]));
		console.log('marker[1] after clear:', network.getEdgeStringAttribute('marker', edges[1]));

		section('Network attributes');
		network.defineNetworkAttribute('title', AttributeType.String);
		network.defineNetworkAttribute('counters', AttributeType.UnsignedInteger, 2);
		network.defineNetworkAttribute('flags', AttributeType.Boolean, 4);
		network.defineNetworkAttribute('settings', AttributeType.Javascript);

		network.setNetworkStringAttribute('title', 'Attribute showcase');
		const counters = network.getNetworkAttributeBuffer('counters');
		const flags = network.getNetworkAttributeBuffer('flags');
		const settings = network.getNetworkAttributeBuffer('settings');

		counters.view[0] = 42n;
		counters.view[1] = 7n;
		flags.view.set([1, 0, 1, 1]);
		settings.set(0, { directed: network.directed, comment: 'stored in JS shadow' });

		console.log('network title:', network.getNetworkStringAttribute('title'));
		console.log('network counters:', Array.from(counters.view, (value) => value.toString()));
		console.log('network flags:', Array.from(flags.view));
		console.log('network settings:', settings.get(0));

		settings.delete(0);
		network.setNetworkStringAttribute('title', null);
		console.log('network settings after delete:', settings.get(0));
		console.log('network title after clear:', network.getNetworkStringAttribute('title'));
	} finally {
		section('Teardown');
		network.dispose();
	}
}

main().catch((error) => {
	console.error('Attribute showcase failed:', error);
	process.exitCode = 1;
});
