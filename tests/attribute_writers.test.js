import { describe, expect, test, vi } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';

function readNodeValues(network, name) {
	return network.withBufferAccess(() => Array.from(network.getNodeAttributeBuffer(name).view));
}

function readEdgeValues(network, name) {
	return network.withBufferAccess(() => Array.from(network.getEdgeAttributeBuffer(name).view));
}

function readNetworkValues(network, name) {
	return network.withBufferAccess(() => Array.from(network.getNetworkAttributeBuffer(name).view));
}

describe('chainable attribute writers', () => {
	test('assign node scalar, callback, and string values and remains chainable', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 3 });
		try {
			const originalGetNodeBuffer = net.getNodeAttributeBuffer.bind(net);
			const getNodeBuffer = vi.spyOn(net, 'getNodeAttributeBuffer').mockImplementation((name) => {
				expect(net._bufferSessionDepth).toBeGreaterThan(0);
				return originalGetNodeBuffer(name);
			});

			const returned = net
				.nodeAttribute('weight', 2.5)
				.nodeAttribute('score', (_current, id, ordinal) => id + ordinal)
				.nodeAttribute('label', (_current, id) => `node-${id}`);

			expect(returned).toBe(net);
			expect(readNodeValues(net, 'weight').slice(0, 3)).toEqual([2.5, 2.5, 2.5]);
			expect(readNodeValues(net, 'score').slice(0, 3)).toEqual([0, 2, 4]);
			expect(net.getNodeStringAttribute('label', 0)).toBe('node-0');
			expect(net.getNodeStringAttribute('label', 1)).toBe('node-1');
			expect(net.getNodeStringAttribute('label', 2)).toBe('node-2');
			expect(getNodeBuffer).toHaveBeenCalled();
			getNodeBuffer.mockRestore();
			expect(() => net.getNodeAttributeBuffer('weight')).toThrow(/outside buffer access/i);
		} finally {
			net.dispose();
		}
	});

	test('writes numeric arrays, reuses existing attributes, and honors explicit schema', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 3 });
		try {
			net.defineNodeAttribute('existing', AttributeType.Double, 2);
			const beforeVersion = net.getNodeAttributeVersion('existing');
			const changed = [];
			const defined = [];
			net.on('attribute:changed', (event) => changed.push(event.detail));
			net.on('attribute:defined', (event) => defined.push(event.detail));

			net
				.nodeAttribute('existing', [[1, 2], [3, 4], [5, 6]])
				.nodeAttribute('rank', new Uint32Array([7, 8, 9]), { type: AttributeType.UnsignedInteger })
				.nodeAttribute('pair', [[0.5, 1.5], [2.5, 3.5], [4.5, 5.5]], { type: 'double', dimension: 2 });

			expect(net.getNodeAttributeInfo('existing')).toEqual({ type: AttributeType.Double, dimension: 2, complex: false });
			expect(net.getNodeAttributeVersion('existing')).toBeGreaterThan(beforeVersion);
			expect(defined.map((detail) => detail.name)).not.toContain('existing');
			expect(changed.filter((detail) => detail.name === 'existing')).toHaveLength(1);
			expect(readNodeValues(net, 'existing').slice(0, 6)).toEqual([1, 2, 3, 4, 5, 6]);
			expect(readNodeValues(net, 'rank').slice(0, 3)).toEqual([7, 8, 9]);
			expect(net.getNodeAttributeInfo('rank')).toEqual({ type: AttributeType.UnsignedInteger, dimension: 1, complex: false });
			expect(readNodeValues(net, 'pair').slice(0, 6)).toEqual([0.5, 1.5, 2.5, 3.5, 4.5, 5.5]);
			expect(net.getNodeAttributeInfo('pair')).toEqual({ type: AttributeType.Double, dimension: 2, complex: false });
		} finally {
			net.dispose();
		}
	});

	test('assigns multiple node attributes from callback arrays and objects', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 3 });
		try {
			net
				.nodeAttributes(['x', 'y'], (_current, id, ordinal) => [id * 10, ordinal + 1])
				.nodeAttributes(['a', 'b'], {
					a: [3, 4, 5],
					b: 9,
				});

			expect(readNodeValues(net, 'x').slice(0, 3)).toEqual([0, 10, 20]);
			expect(readNodeValues(net, 'y').slice(0, 3)).toEqual([1, 2, 3]);
			expect(readNodeValues(net, 'a').slice(0, 3)).toEqual([3, 4, 5]);
			expect(readNodeValues(net, 'b').slice(0, 3)).toEqual([9, 9, 9]);
		} finally {
			net.dispose();
		}
	});

	test('assigns edge attributes with arrays and callbacks', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(3);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);
			const changed = [];
			net.on('attribute:changed', (event) => changed.push(event.detail));

			net
				.edgeAttribute('capacity', new Float32Array([1.5, 2.5]))
				.edgeAttribute('label', (_current, id, ordinal) => `edge-${id}-${ordinal}`);

			expect(readEdgeValues(net, 'capacity').slice(edges[0], edges[1] + 1)).toEqual([1.5, 2.5]);
			expect(net.getEdgeStringAttribute('label', edges[0])).toBe(`edge-${edges[0]}-0`);
			expect(net.getEdgeStringAttribute('label', edges[1])).toBe(`edge-${edges[1]}-1`);
			expect(changed.filter((detail) => detail.name === 'capacity')).toHaveLength(1);
			expect(changed.filter((detail) => detail.name === 'label')).toHaveLength(1);
		} finally {
			net.dispose();
		}
	});

	test('uses active ordinals by default and supports id-indexed arrays', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(4);
			net.removeNodes([nodes[1]]);

			net
				.nodeAttribute('ordinalRank', [10, 20, 30])
				.nodeAttribute('idRank', [100, 101, 102, 103], { indexBy: 'id' });

			const ordinalRank = readNodeValues(net, 'ordinalRank');
			const idRank = readNodeValues(net, 'idRank');
			expect(ordinalRank[nodes[0]]).toBe(10);
			expect(ordinalRank[nodes[2]]).toBe(20);
			expect(ordinalRank[nodes[3]]).toBe(30);
			expect(idRank[nodes[0]]).toBe(100);
			expect(idRank[nodes[2]]).toBe(102);
			expect(idRank[nodes[3]]).toBe(103);
		} finally {
			net.dispose();
		}
	});

	test('bumps each modified attribute version exactly once', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 3 });
		try {
			const changed = [];
			net.on('attribute:changed', (event) => changed.push(event.detail));

			net.nodeAttribute('score', 1);
			const firstVersion = net.getNodeAttributeVersion('score');
			net.nodeAttribute('score', [2, 3, 4]);
			net.nodeAttribute('label', ['a', 'b', 'c']);

			const scoreEvents = changed.filter((detail) => detail.scope === 'node' && detail.name === 'score');
			const labelEvents = changed.filter((detail) => detail.scope === 'node' && detail.name === 'label');
			expect(scoreEvents).toHaveLength(2);
			expect(labelEvents).toHaveLength(1);
			expect(scoreEvents[1].version).toBeGreaterThan(firstVersion);
			expect(readNodeValues(net, 'score').slice(0, 3)).toEqual([2, 3, 4]);
			expect(net.getNodeStringAttribute('label', 2)).toBe('c');
		} finally {
			net.dispose();
		}
	});

	test('assigns network attributes with the same chainable interface', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 2 });
		try {
			const changed = [];
			net.on('attribute:changed', (event) => changed.push(event.detail));

			const returned = net
				.networkAttribute('title', 'Example graph')
				.networkAttribute('metadata', { source: 'unit-test', count: 2 })
				.networkAttribute('score', 2.5)
				.networkAttribute('bounds', [0, 1], { type: AttributeType.Double, dimension: 2 })
				.networkAttributes(['status', 'revision'], (_current, id, ordinal, network) => ({
					status: `${network.nodeCount}:${id}:${ordinal}`,
					revision: 7,
				}));

			expect(returned).toBe(net);
			expect(net.getNetworkStringAttribute('title')).toBe('Example graph');
			expect(net.getNetworkStringAttribute('status')).toBe('2:0:0');
			expect(net.withBufferAccess(() => net.getNetworkAttributeBuffer('metadata').get(0))).toEqual({ source: 'unit-test', count: 2 });
			expect(readNetworkValues(net, 'score')[0]).toBeCloseTo(2.5);
			expect(readNetworkValues(net, 'bounds')).toEqual([0, 1]);
			expect(readNetworkValues(net, 'revision')[0]).toBe(7);
			expect(net.getNetworkAttributeInfo('bounds')).toEqual({ type: AttributeType.Double, dimension: 2, complex: false });
			expect(net.getNetworkAttributeInfo('metadata')).toEqual({ type: AttributeType.Javascript, dimension: 1, complex: true });
			expect(changed.filter((detail) => detail.scope === 'network' && detail.name === 'title')).toHaveLength(1);
			expect(changed.filter((detail) => detail.scope === 'network' && detail.name === 'metadata')).toHaveLength(1);
			expect(changed.filter((detail) => detail.scope === 'network' && detail.name === 'status')).toHaveLength(1);
			expect(changed.filter((detail) => detail.scope === 'network' && detail.name === 'revision')).toHaveLength(1);
			expect(() => net.getNetworkAttributeBuffer('score')).toThrow(/outside buffer access/i);
		} finally {
			net.dispose();
		}
	});
});
