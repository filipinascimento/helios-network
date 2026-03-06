import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { beforeAll, afterAll, describe, expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';
import { withEdgeBuffer, withNetworkBuffer, withNodeBuffer } from './helpers/bufferAccess.js';

describe('HeliosNetwork (Node runtime)', () => {
	let network;

	beforeAll(async () => {
		network = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
	});

	afterAll(() => {
		if (network) {
			network.dispose();
			network = undefined;
		}
	});

	test('adds and removes nodes and edges', () => {
		const createdNodes = network.addNodes(4);
		expect(Array.from(createdNodes)).toHaveLength(4);
		expect(network.nodeCount).toBeGreaterThanOrEqual(4);

		const createdEdges = network.addEdges([
			{ from: createdNodes[0], to: createdNodes[1] },
			{ from: createdNodes[1], to: createdNodes[2] },
			{ from: createdNodes[2], to: createdNodes[3] },
		]);
		expect(Array.from(createdEdges)).toHaveLength(3);
		expect(network.edgeCount).toBeGreaterThanOrEqual(3);

		const outNeighbors = network.getOutNeighbors(createdNodes[1]);
		expect(outNeighbors.nodes).toContain(createdNodes[2]);

		network.removeEdges([createdEdges[0]]);
		expect(network.edgeCount).toBeGreaterThanOrEqual(2);

		network.removeNodes([createdNodes[3]]);
		const activeNodes = network.withBufferAccess(() => Array.from(network.nodeIndices), { nodeIndices: true });
		expect(activeNodes).not.toContain(createdNodes[3]);
	});

	test('collects multi-source and concentric neighbors', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(6);
			net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[0], to: nodes[2] },
				{ from: nodes[1], to: nodes[3] },
				{ from: nodes[2], to: nodes[4] },
				{ from: nodes[3], to: nodes[5] },
				{ from: nodes[4], to: nodes[5] },
			]);

			const oneHop = net.getNeighborsForNodes([nodes[0], nodes[1]], { direction: 'out', includeSourceNodes: false });
			expect(Array.from(oneHop.nodes).sort((a, b) => a - b)).toEqual(
				Array.from([nodes[2], nodes[3]]).sort((a, b) => a - b)
			);

			const level2 = net.getNeighborsAtLevel(nodes[0], 2, { direction: 'out', includeEdges: false });
			expect(Array.from(level2).sort((a, b) => a - b)).toEqual(
				Array.from([nodes[3], nodes[4]]).sort((a, b) => a - b)
			);

			const upTo2 = net.getNeighborsUpToLevel(nodes[0], 2, { direction: 'out', includeEdges: false });
			expect(Array.from(upTo2).sort((a, b) => a - b)).toEqual(
				Array.from([nodes[1], nodes[2], nodes[3], nodes[4]]).sort((a, b) => a - b)
			);

			const selector = net.createNodeSelector([nodes[0]]);
			const selectorLevel2 = selector.neighborsAtLevel(2, { mode: 'out' });
			expect(Array.from(selectorLevel2).sort((a, b) => a - b)).toEqual(
				Array.from([nodes[3], nodes[4]]).sort((a, b) => a - b)
			);
			selector.dispose();
		} finally {
			net.dispose();
		}
	});

	test('handles attributes before and after adding nodes/edges', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			expect(net.nodeCount).toBe(0);
			expect(net.edgeCount).toBe(0);

			net.defineNodeAttribute('pre_node_weight', AttributeType.Float, 1);
			net.defineEdgeAttribute('pre_edge_flag', AttributeType.Boolean, 1);
			expect(() => net.getNodeAttributeBuffer('pre_node_weight')).toThrow(/outside buffer access/i);
			expect(() => net.getEdgeAttributeBuffer('pre_edge_flag')).toThrow(/outside buffer access/i);

			const nodes = net.addNodes(3);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			net.withBufferAccess(() => {
				const nodeWeights = net.getNodeAttributeBuffer('pre_node_weight').view;
				const edgeFlags = net.getEdgeAttributeBuffer('pre_edge_flag').view;
				nodeWeights[nodes[0]] = 1.5;
				nodeWeights[nodes[1]] = 2.5;
				nodeWeights[nodes[2]] = 3.5;
				edgeFlags[edges[0]] = 1;
				edgeFlags[edges[1]] = 0;
			});

			net.defineNodeAttribute('label', AttributeType.String, 1);
			net.defineEdgeAttribute('capacity', AttributeType.Double, 1);
			net.setNodeStringAttribute('label', nodes[0], 'alpha');
			net.setNodeStringAttribute('label', nodes[1], 'beta');
			net.setNodeStringAttribute('label', nodes[2], 'gamma');
			net.withBufferAccess(() => {
				const nodeWeights = net.getNodeAttributeBuffer('pre_node_weight').view;
				const capacity = net.getEdgeAttributeBuffer('capacity').view;
				nodeWeights[nodes[1]] = 4.75;
				capacity[edges[0]] = 10.5;
				capacity[edges[1]] = 20.25;
				capacity[edges[1]] = 30;
			});
			net.setNodeStringAttribute('label', nodes[2], 'gamma-updated');
			net.withBufferAccess(() => {
				const nodeWeights = net.getNodeAttributeBuffer('pre_node_weight').view;
				const edgeFlags = net.getEdgeAttributeBuffer('pre_edge_flag').view;
				const capacity = net.getEdgeAttributeBuffer('capacity').view;
				expect(nodeWeights[nodes[0]]).toBeCloseTo(1.5);
				expect(nodeWeights[nodes[1]]).toBeCloseTo(4.75);
				expect(nodeWeights[nodes[2]]).toBeCloseTo(3.5);
				expect(edgeFlags[edges[0]]).toBe(1);
				expect(edgeFlags[edges[1]]).toBe(0);
				expect(capacity[edges[0]]).toBeCloseTo(10.5);
				expect(capacity[edges[1]]).toBeCloseTo(30);
			});
			expect(net.getNodeStringAttribute('label', nodes[0])).toBe('alpha');
			expect(net.getNodeStringAttribute('label', nodes[1])).toBe('beta');
			expect(net.getNodeStringAttribute('label', nodes[2])).toBe('gamma-updated');
		} finally {
			net.dispose();
		}
	});

	test('uses 32-bit arrays for Integer types and BigInt64 for BigInteger types', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(1);
			net.defineNodeAttribute('i32', AttributeType.Integer, 1);
			net.defineNodeAttribute('u32', AttributeType.UnsignedInteger, 1);
			net.defineNodeAttribute('i64', AttributeType.BigInteger, 1);
			net.defineNodeAttribute('u64', AttributeType.UnsignedBigInteger, 1);

			net.withBufferAccess(() => {
				const i32 = net.getNodeAttributeBuffer('i32').view;
				const u32 = net.getNodeAttributeBuffer('u32').view;
				const i64 = net.getNodeAttributeBuffer('i64').view;
				const u64 = net.getNodeAttributeBuffer('u64').view;

				expect(i32).toBeInstanceOf(Int32Array);
				expect(u32).toBeInstanceOf(Uint32Array);
				expect(i64).toBeInstanceOf(BigInt64Array);
				expect(u64).toBeInstanceOf(BigUint64Array);

				i32[nodes[0]] = -12345;
				u32[nodes[0]] = 4294967295;
				i64[nodes[0]] = 1n << 40n;
				u64[nodes[0]] = (1n << 60n) - 1n;

				expect(i32[nodes[0]]).toBe(-12345);
				expect(u32[nodes[0]]).toBe(4294967295);
				expect(i64[nodes[0]]).toBe(1n << 40n);
				expect(u64[nodes[0]]).toBe((1n << 60n) - 1n);
			});
		} finally {
			net.dispose();
		}
	});

	test('manages primitive attribute buffers', () => {
		const nodes = network.addNodes(2);
		network.defineNodeAttribute('weight', AttributeType.Float, 1);

		withNodeBuffer(network, 'weight', ({ view }) => {
			view[nodes[0]] = 1.5;
			view[nodes[1]] = 2.5;
			expect(view[nodes[0]]).toBeCloseTo(1.5);
			expect(view[nodes[1]]).toBeCloseTo(2.5);
		});
	});

	test('supports string and JS-managed attributes', () => {
		const nodes = network.addNodes(1);
		const edges = network.addEdges([{ from: nodes[0], to: nodes[0] }]);

		network.defineNodeAttribute('label', AttributeType.String, 1);
		network.defineEdgeAttribute('meta', AttributeType.Javascript, 1);

		network.setNodeStringAttribute('label', nodes[0], 'node-zero');
		expect(network.getNodeStringAttribute('label', nodes[0])).toBe('node-zero');

		withEdgeBuffer(network, 'meta', (edgeMeta) => {
			edgeMeta.set(edges[0], { custom: 42 });
			expect(edgeMeta.get(edges[0])).toEqual({ custom: 42 });
		});
	});

	test('categorizes and decategorizes string attributes', () => {
		const nodes = network.addNodes(4);
		network.defineNodeAttribute('group', AttributeType.String, 1);

		network.setNodeStringAttribute('group', nodes[0], 'apple');
		network.setNodeStringAttribute('group', nodes[1], '');
		network.setNodeStringAttribute('group', nodes[2], '__NA__');
		network.setNodeStringAttribute('group', nodes[3], 'banana');

		network.categorizeNodeAttribute('group', { sortOrder: 'frequency' });
		withNodeBuffer(network, 'group', ({ view: categorized }) => {
			expect(categorized).toBeInstanceOf(Int32Array);
			expect(categorized[nodes[0]]).toBeGreaterThanOrEqual(0);
			expect(categorized[nodes[1]]).toBe(-1);
			expect(categorized[nodes[2]]).toBe(-1);
			expect(categorized[nodes[3]]).toBeGreaterThanOrEqual(0);
		});

		network.decategorizeNodeAttribute('group');
		expect(network.getNodeStringAttribute('group', nodes[0])).toBe('apple');
		expect(network.getNodeStringAttribute('group', nodes[1])).toBe('__NA__');
		expect(network.getNodeStringAttribute('group', nodes[2])).toBe('__NA__');
		expect(network.getNodeStringAttribute('group', nodes[3])).toBe('banana');
	});

	test('reads category dictionaries for categorized attributes', () => {
		const nodes = network.addNodes(3);
		network.defineNodeAttribute('species', AttributeType.String, 1);
		network.setNodeStringAttribute('species', nodes[0], 'alpha');
		network.setNodeStringAttribute('species', nodes[1], 'beta');
		network.setNodeStringAttribute('species', nodes[2], 'alpha');

		network.categorizeNodeAttribute('species', { sortOrder: 'alphabetical' });
		const dict = network.getNodeAttributeCategoryDictionary('species');
		expect(dict.entries.length).toBeGreaterThanOrEqual(2);
		const labels = dict.labels.slice();
		expect(labels).toEqual(expect.arrayContaining(['alpha', 'beta']));
		expect(dict.ids.length).toBe(dict.entries.length);
		expect(dict.entries.every((entry) => typeof entry.id === 'number' && typeof entry.label === 'string')).toBe(true);
	});

	test('includes missing label in category dictionary', () => {
		const nodes = network.addNodes(3);
		network.defineNodeAttribute('missingGroup', AttributeType.String, 1);
		network.setNodeStringAttribute('missingGroup', nodes[0], 'alpha');
		network.setNodeStringAttribute('missingGroup', nodes[1], '');
		network.setNodeStringAttribute('missingGroup', nodes[2], '__NA__');

		network.categorizeNodeAttribute('missingGroup', { sortOrder: 'frequency' });
		const dict = network.getNodeAttributeCategoryDictionary('missingGroup');
		const missingEntry = dict.entries.find((entry) => entry.label === '__NA__');
		expect(missingEntry).toBeDefined();
		expect(missingEntry.id).toBe(-1);
	});

	test('categorizes fully assigned categories (main.js style)', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodeCount = 200;
			const nodes = net.addNodes(nodeCount);
			net.defineNodeAttribute('category', AttributeType.String, 1);
			const categoryCount = 8;
			const total = Math.max(1, nodes.length);
			for (let i = 0; i < nodes.length; i += 1) {
				const bucket = Math.min(categoryCount - 1, Math.floor((i / total) * categoryCount));
				const label = `category${bucket + 1}`;
				net.setNodeStringAttribute('category', nodes[i], label);
			}
			net.categorizeNodeAttribute('category', { sortOrder: 'frequency' });

			const dict = net.getNodeAttributeCategoryDictionary('category');
			const labels = dict.entries.map((entry) => entry.label).sort();
			expect(labels).toEqual([
				'category1',
				'category2',
				'category3',
				'category4',
				'category5',
				'category6',
				'category7',
				'category8',
			]);
			expect(dict.entries.some((entry) => entry.label === '__NA__' || entry.id === -1)).toBe(false);

			withNodeBuffer(net, 'category', ({ view: codes }) => {
				const counts = new Map();
				for (let i = 0; i < nodes.length; i += 1) {
					const code = codes[nodes[i]];
					expect(code).toBeGreaterThanOrEqual(0);
					counts.set(code, (counts.get(code) ?? 0) + 1);
				}
				for (const entry of dict.entries) {
					expect(counts.get(entry.id) ?? 0).toBeGreaterThan(0);
				}
			});
		} finally {
			net.dispose();
		}
	});

	test('creates node and edge selectors', () => {
		const nodeSelector = network.createNodeSelector();
		const edgeSelector = network.createEdgeSelector();

		expect(nodeSelector.count).toBe(network.nodeCount);
		expect(edgeSelector.count).toBe(network.edgeCount);

		const partialNodes = network.addNodes(3);
		const customSelector = network.createNodeSelector(partialNodes);
		expect(Array.from(customSelector.toTypedArray())).toEqual(Array.from(partialNodes));

		nodeSelector.dispose();
		edgeSelector.dispose();
		customSelector.dispose();
	});

	test('reports buffer memory usage', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(3);
			net.defineNodeAttribute('pos', AttributeType.Float, 2);
			net.defineEdgeAttribute('weight', AttributeType.Double, 1);
			net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			const report = net.getBufferMemoryUsage();
			expect(report.wasm.heapBytes).toBeGreaterThan(0);

			expect(report.buffers['topology.edgeFromTo']).toBe(net.edgeCapacity * 2 * Uint32Array.BYTES_PER_ELEMENT);
			expect(report.buffers['topology.nodeFreeList']).toBe(net.nodeCapacity * Uint32Array.BYTES_PER_ELEMENT);
			expect(report.buffers['topology.edgeFreeList']).toBe(net.edgeCapacity * Uint32Array.BYTES_PER_ELEMENT);

			expect(report.buffers['sparse.node.attribute.pos']).toBeGreaterThan(0);

		} finally {
			net.dispose();
		}
	});

	test('reports buffer versions', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			net.addEdges([{ from: nodes[0], to: nodes[1] }]);

			net.defineNodeAttribute('pos', AttributeType.Float, 2);
			const versions = net.getBufferVersions();
			expect(typeof versions.topology.node).toBe('number');
			expect(typeof versions.topology.edge).toBe('number');
			expect(typeof versions.attributes.node.pos).toBe('number');
		} finally {
			net.dispose();
		}
	});

	test('omits node-to-edge passthrough attributes when saving', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			net.addEdges([{ from: nodes[0], to: nodes[1] }]);
			net.defineNodeAttribute('score', AttributeType.Float, 1);
			net.defineNodeToEdgeAttribute('score', 'score_passthrough', 'both', true);

			const passthroughsBefore = net.getNodeToEdgePassthroughs().map((p) => p.edgeName);
			expect(passthroughsBefore).toContain('score_passthrough');

			const bytes = await net.saveXNet({ format: 'uint8array' });
			const text = Buffer.from(bytes).toString('utf8');
			expect(text).not.toContain('#e "score_passthrough"');

			const loaded = await HeliosNetwork.fromXNet(bytes);
			try {
				expect(loaded.hasEdgeAttribute('score_passthrough')).toBe(false);
			} finally {
				loaded.dispose();
			}

			const passthroughsAfter = net.getNodeToEdgePassthroughs().map((p) => p.edgeName);
			expect(passthroughsAfter).toContain('score_passthrough');
			expect(net.hasEdgeAttribute('score_passthrough')).toBe(true);
		} finally {
			net.dispose();
		}
	});

	test('checks active index membership', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(3);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			expect(net.hasNodeIndex(nodes[0])).toBe(true);
			expect(net.hasEdgeIndex(edges[0])).toBe(true);
			expect(net.hasNodeIndex(9999)).toBe(false);
			expect(net.hasEdgeIndex(9999)).toBe(false);

			expect(net.hasNodeIndices(nodes)).toEqual([true, true, true]);
			expect(net.hasEdgeIndices(edges)).toEqual([true, true]);

			net.removeNodes([nodes[1]]);
			net.removeEdges([edges[1]]);
			expect(net.hasNodeIndex(nodes[1])).toBe(false);
			expect(net.hasEdgeIndex(edges[1])).toBe(false);

			expect(net.hasNodeIndices(nodes)).toEqual([true, false, true]);
			expect(net.hasEdgeIndices(edges)).toEqual([false, false]); // node removal clears incident edges

			const mixedNodes = [nodes[0], -1, 1.5, 9999];
			expect(net.hasNodeIndices(mixedNodes)).toEqual([true, false, false, false]);
		} finally {
			net.dispose();
		}
	});

	test('exposes active index views and full-coverage selectors', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(4);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[2], to: nodes[3] },
			]);

			net.withBufferAccess(() => {
				expect(Array.from(net.nodeIndices)).toEqual(Array.from(nodes));
				expect(Array.from(net.edgeIndices)).toEqual(Array.from(edges));
				expect(net.nodeIndices.buffer).toBe(net.module.HEAPU8.buffer);
				expect(net.edgeIndices.buffer).toBe(net.module.HEAPU8.buffer);
				expect(net.nodeIndices.byteOffset).toBe(net.module._CXNetworkActiveNodeIndices(net.ptr));
				expect(net.edgeIndices.byteOffset).toBe(net.module._CXNetworkActiveEdgeIndices(net.ptr));
			}, { nodeIndices: true, edgeIndices: true });

			expect(() => net.nodeIndices).toThrow(/buffer access/i);
			expect(() => net.edgeIndices).toThrow(/buffer access/i);
			expect(() => net.withBufferAccess(() => net.nodeIndices, { nodeIndices: true })).not.toThrow();
			expect(() => net.withBufferAccess(() => net.edgeIndices, { edgeIndices: true })).not.toThrow();

			const allNodes = net.nodes;
			const allEdges = net.edges;
			expect(allNodes.count).toBe(net.nodeCount);
			expect(allEdges.count).toBe(net.edgeCount);
			expect(new Set(allNodes)).toEqual(new Set(nodes));
			expect(new Set(allEdges)).toEqual(new Set(edges));
			expect(() => net.withBufferAccess(() => [...net.nodes])).not.toThrow();

			net.withBufferAccess(() => {
				const edgeIndicesView = net.edgeIndices;
				edgeIndicesView[0] = 123456;
				expect(net.edgeIndices[0]).toBe(123456);
			}, { edgeIndices: true });
			const next = net.addEdges([{ from: nodes[0], to: nodes[2] }]);
			net.withBufferAccess(() => {
				expect(net.edgeIndices[0]).not.toBe(123456);
			}, { edgeIndices: true });
		} finally {
			net.dispose();
		}
	});

	test('selector proxies expose attributes and topology helpers', () => {
		const nodes = network.addNodes(3);
		const edges = network.addEdges([
			{ from: nodes[0], to: nodes[1] },
			{ from: nodes[1], to: nodes[2] },
		]);

		network.defineNodeAttribute('proxy_label', AttributeType.String);
		network.defineEdgeAttribute('proxy_capacity', AttributeType.Double);

		network.setNodeStringAttribute('proxy_label', nodes[0], 'root');
		network.setNodeStringAttribute('proxy_label', nodes[1], 'middle');
		withEdgeBuffer(network, 'proxy_capacity', (capacity) => {
			capacity.view[edges[0]] = 1.25;
			capacity.view[edges[1]] = 2.5;
		});

		const subset = Array.from(nodes.slice(0, 2));
		const nodeSelector = network.createNodeSelector(subset);
		expect(Array.from(nodeSelector)).toEqual(subset);
		expect(nodeSelector.toArray()).toEqual(subset);
		expect(nodeSelector.proxy_label).toEqual(['root', 'middle']);
		expect(nodeSelector.attribute('proxy_label')).toEqual(['root', 'middle']);
		expect(nodeSelector.degree({ mode: 'out' })).toEqual([1, 1]);

		const neighborInfo = nodeSelector.neighbors({ includeEdges: true });
		expect(Array.from(neighborInfo.nodes)).toEqual(expect.arrayContaining([nodes[1], nodes[2]]));
		expect(Array.from(neighborInfo.edges)).toEqual(expect.arrayContaining(Array.from(edges)));

		const incidentSelector = nodeSelector.incidentEdges({ asSelector: true });
		expect(Array.from(incidentSelector)).toEqual(expect.arrayContaining(Array.from(edges)));
		incidentSelector.dispose();

		const edgeSelector = network.createEdgeSelector(edges);
		expect(edgeSelector.proxy_capacity).toEqual([1.25, 2.5]);
		expect(Array.from(edgeSelector.sources())).toEqual([nodes[0], nodes[1]]);
		expect(Array.from(edgeSelector.targets())).toEqual([nodes[1], nodes[2]]);
		expect(Array.from(edgeSelector.nodes())).toEqual(expect.arrayContaining(Array.from(nodes)));

		const uniqueSources = edgeSelector.sources({ unique: true, asSelector: true });
		expect(uniqueSources.count).toBe(2);
		uniqueSources.dispose();

		nodeSelector.dispose();
		edgeSelector.dispose();
	});

	test('withBufferAccess rejects first-time attribute metadata lookup', () => {
		const local = network;
		const attr = `buffer_guard_${randomUUID().replace(/-/g, '')}`;
		local.defineNodeAttribute(attr, AttributeType.Float);
		const meta = local._nodeAttributes.get(attr);
		if (meta) {
			meta.attributePtr = 0;
			meta.stride = 0;
		}
		expect(() => local.withBufferAccess(() => local.getNodeAttributeBuffer(attr))).toThrow(/buffer access|attribute metadata lookup/i);
		// Prime metadata outside the guarded section; access then becomes safe.
		const primedMeta = local._ensureAttributeMetadata('node', attr);
		local._attributePointers('node', attr, primedMeta);
		expect(() => local.withBufferAccess(() => local.getNodeAttributeBuffer(attr))).not.toThrow();
	});

	test('withBufferAccess rejects attribute removal paths that allocate names', () => {
		const local = network;
		local.defineNodeAttribute('buffer_remove_node', AttributeType.Float);
		local.defineEdgeAttribute('buffer_remove_edge', AttributeType.Float);

		expect(() => local.withBufferAccess(() => local.removeNodeAttribute('buffer_remove_node'))).toThrow(/buffer access|remove node attribute/i);

		expect(() => local.removeNodeAttribute('buffer_remove_node')).not.toThrow();
		expect(() => local.removeEdgeAttribute('buffer_remove_edge')).not.toThrow();
	});

	test('lists, inspects, and detects attributes', () => {
		const nodes = network.addNodes(1);
		network.defineNodeAttribute('mass', AttributeType.Double, 2);
		network.defineEdgeAttribute('flag', AttributeType.Boolean);
		network.defineNetworkAttribute('version', AttributeType.UnsignedInteger);

		expect(network.getNodeAttributeNames()).toEqual(expect.arrayContaining(['mass']));
		expect(network.getEdgeAttributeNames()).toEqual(expect.arrayContaining(['flag']));
		expect(network.getNetworkAttributeNames()).toEqual(expect.arrayContaining(['version']));

		expect(network.hasNodeAttribute('mass')).toBe(true);
		expect(network.hasEdgeAttribute('flag')).toBe(true);
		expect(network.hasNetworkAttribute('version')).toBe(true);
		expect(network.hasNodeAttribute('missing')).toBe(false);

		const nodeInfo = network.getNodeAttributeInfo('mass');
		expect(nodeInfo).toEqual({ type: AttributeType.Double, dimension: 2, complex: false });
		const edgeInfo = network.getEdgeAttributeInfo('flag');
		expect(edgeInfo).toEqual({ type: AttributeType.Boolean, dimension: 1, complex: false });
		const netInfo = network.getNetworkAttributeInfo('version');
		expect(netInfo).toEqual({ type: AttributeType.UnsignedInteger, dimension: 1, complex: false });

		const selector = network.createNodeSelector(nodes);
		expect(selector.hasAttribute('mass')).toBe(true);
		selector.dispose();
	});

	test('round-trips .bxnet payloads via in-memory buffers', async () => {
		const original = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			if (typeof original.module._CXNetworkWriteBXNet !== 'function' || typeof original.module._CXNetworkReadBXNet !== 'function') {
				await expect(original.saveBXNet()).rejects.toThrow(/CXNetworkWriteBXNet is not available/);
				return;
			}

			const nodes = original.addNodes(2);
			const edges = original.addEdges([{ from: nodes[0], to: nodes[1] }]);

			original.defineNodeAttribute('weight', AttributeType.Float);
			original.defineEdgeAttribute('capacity', AttributeType.Double);

			withNodeBuffer(original, 'weight', ({ view }) => {
				view[nodes[0]] = 3.5;
			});
			withEdgeBuffer(original, 'capacity', ({ view }) => {
				view[edges[0]] = 7.25;
			});

			const payload = await original.saveBXNet();
			expect(payload).toBeInstanceOf(Uint8Array);
			expect(payload.byteLength).toBeGreaterThan(0);

			const restored = await HeliosNetwork.fromBXNet(payload);
				try {
					expect(restored.directed).toBe(true);
					expect(restored.nodeCount).toBe(2);
					expect(restored.edgeCount).toBe(1);
					withNodeBuffer(restored, 'weight', ({ view: restoredWeight }) => {
						expect(restoredWeight[0]).toBeCloseTo(3.5);
					});
					withEdgeBuffer(restored, 'capacity', ({ view: restoredCapacity }) => {
						expect(restoredCapacity[0]).toBeCloseTo(7.25);
					});
				} finally {
					restored.dispose();
				}
		} finally {
			original.dispose();
		}
	});

	test('fills active index buffers for sparse networks and segments', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(8);
			net.removeNodes([nodes[1], nodes[6]]);
			const edgeSpecs = [
				{ from: nodes[0], to: nodes[2] },
				{ from: nodes[3], to: nodes[4] },
				{ from: nodes[4], to: nodes[5] },
			];
			const edges = net.addEdges(edgeSpecs);
			net.removeEdges([edges[1]]);

			const mod = net.module;

			const tinyNodePtr = mod._malloc(Uint32Array.BYTES_PER_ELEMENT);
			const tinyNodeBuf = new Uint32Array(mod.HEAPU32.buffer, tinyNodePtr, 1);
			tinyNodeBuf[0] = 0xdeadbeef;
			const requiredNodes = net.writeActiveNodes(tinyNodeBuf);
			expect(requiredNodes).toBe(net.nodeCount);
			expect(tinyNodeBuf[0]).toBe(0xdeadbeef);
			mod._free(tinyNodePtr);

			const nodePtr = mod._malloc(requiredNodes * Uint32Array.BYTES_PER_ELEMENT);
			const nodeBuf = new Uint32Array(mod.HEAPU32.buffer, nodePtr, requiredNodes);
			const writtenNodes = net.writeActiveNodes(nodeBuf);
			expect(writtenNodes).toBe(requiredNodes);
			expect(writtenNodes).toBe(net.nodeCount);
			mod._free(nodePtr);

			const tinyEdgePtr = mod._malloc(Uint32Array.BYTES_PER_ELEMENT);
			const tinyEdgeBuf = new Uint32Array(mod.HEAPU32.buffer, tinyEdgePtr, 1);
			const requiredEdges = net.writeActiveEdges(tinyEdgeBuf);
			expect(requiredEdges).toBe(net.edgeCount);
			mod._free(tinyEdgePtr);

			const edgePtr = mod._malloc(requiredEdges * Uint32Array.BYTES_PER_ELEMENT);
			const edgeBuf = new Uint32Array(mod.HEAPU32.buffer, edgePtr, requiredEdges);
			const writtenEdges = net.writeActiveEdges(edgeBuf);
			expect(writtenEdges).toBe(requiredEdges);
			expect(writtenEdges).toBe(net.edgeCount);
			mod._free(edgePtr);

			net.defineNodeAttribute('position', AttributeType.Float, 4);
			withNodeBuffer(net, 'position', ({ view: posBuf }) => {
				for (let i = 0; i < nodes.length; i++) {
					const base = nodes[i] * 4;
					posBuf[base + 0] = i + 0.1;
					posBuf[base + 1] = i + 0.2;
					posBuf[base + 2] = i + 0.3;
					posBuf[base + 3] = 1.0;
				}
			});

			net.defineNodeAttribute('size', AttributeType.Float, 1);
			net.defineNodeToEdgeAttribute('size', 'size_endpoints', 'both');
			withNodeBuffer(net, 'size', ({ view: sizeBuf }) => {
				for (let i = 0; i < nodes.length; i++) {
					sizeBuf[nodes[i]] = i + 10;
				}
			});
			net.bumpNodeAttributeVersion('size');

			const segPtr = mod._malloc(Float32Array.BYTES_PER_ELEMENT * requiredEdges * 8);
			const segments = new Float32Array(mod.HEAPF32.buffer, segPtr, requiredEdges * 8);
			const segEdges = withNodeBuffer(net, 'position', ({ view: posBuf }) => net.writeActiveEdgeSegments(posBuf, segments, 4));
			expect(segEdges).toBe(requiredEdges);

			const packedEdges = edgeSpecs.filter((_, idx) => idx !== 1);
			withNodeBuffer(net, 'position', ({ view: posBuf }) => {
				for (let i = 0; i < requiredEdges; i++) {
					const edge = packedEdges[i];
					const fromBase = edge.from * 4;
					const toBase = edge.to * 4;
					const outBase = i * 8;
					expect(Array.from(segments.slice(outBase, outBase + 4))).toEqual([
						posBuf[fromBase + 0],
						posBuf[fromBase + 1],
						posBuf[fromBase + 2],
						posBuf[fromBase + 3],
					]);
					expect(Array.from(segments.slice(outBase + 4, outBase + 8))).toEqual([
						posBuf[toBase + 0],
						posBuf[toBase + 1],
						posBuf[toBase + 2],
						posBuf[toBase + 3],
					]);
				}
			});
			mod._free(segPtr);

			const activeEdgeIndices = Array.from(edges).filter((_, idx) => idx !== 1);
			net.withBufferAccess(() => {
				const edgeSizeView = net.getEdgeAttributeBuffer('size_endpoints').view;
				const sizeBuf = net.getNodeAttributeBuffer('size').view;
				for (let i = 0; i < requiredEdges; i++) {
					const edge = packedEdges[i];
					const sparseBase = activeEdgeIndices[i] * 2;
					expect(edgeSizeView[sparseBase + 0]).toBeCloseTo(sizeBuf[edge.from]);
					expect(edgeSizeView[sparseBase + 1]).toBeCloseTo(sizeBuf[edge.to]);
				}
			});
		} finally {
			net.dispose();
		}
	});

	test('copies node attributes to edge attributes with selectable endpoints', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(3);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			net.defineNodeAttribute('vec', AttributeType.Float, 2);
			withNodeBuffer(net, 'vec', ({ view: vec }) => {
				for (let i = 0; i < nodes.length; i++) {
					const base = nodes[i] * 2;
					vec[base + 0] = i + 0.5;
					vec[base + 1] = i + 1.5;
				}
			});

			net.defineNodeToEdgeAttribute('vec', 'from_only', 'source', false);
			net.defineNodeToEdgeAttribute('vec', 'to_only', 'destination', false);
			net.defineNodeToEdgeAttribute('vec', 'both_vec', 'both');
			net.defineNodeToEdgeAttribute('vec', 'from_only_double', 'source', true);
			net.defineNodeToEdgeAttribute('vec', 'to_only_double', 'destination', true);

			net.withBufferAccess(() => {
				const vec = net.getNodeAttributeBuffer('vec').view;
				const fromView = net.getEdgeAttributeBuffer('from_only').view;
				const toView = net.getEdgeAttributeBuffer('to_only').view;
				const bothView = net.getEdgeAttributeBuffer('both_vec').view;
				const fromDoubleView = net.getEdgeAttributeBuffer('from_only_double').view;
				const toDoubleView = net.getEdgeAttributeBuffer('to_only_double').view;

				for (let i = 0; i < edges.length; i++) {
					const edgeIdx = edges[i];
					const fromNode = net.edgesView[edgeIdx * 2];
					const toNode = net.edgesView[edgeIdx * 2 + 1];
					const fromBase = fromNode * 2;
					const toBase = toNode * 2;
					const edgeBase = edgeIdx * 2;
					expect(fromView[edgeBase + 0]).toBeCloseTo(vec[fromBase + 0]);
					expect(fromView[edgeBase + 1]).toBeCloseTo(vec[fromBase + 1]);
					expect(toView[edgeBase + 0]).toBeCloseTo(vec[toBase + 0]);
					expect(toView[edgeBase + 1]).toBeCloseTo(vec[toBase + 1]);

					const bothBase = edgeIdx * 4;
					expect(bothView[bothBase + 0]).toBeCloseTo(vec[fromBase + 0]);
					expect(bothView[bothBase + 1]).toBeCloseTo(vec[fromBase + 1]);
					expect(bothView[bothBase + 2]).toBeCloseTo(vec[toBase + 0]);
					expect(bothView[bothBase + 3]).toBeCloseTo(vec[toBase + 1]);

					const doubleBase = edgeIdx * 4;
					expect(fromDoubleView[doubleBase + 0]).toBeCloseTo(vec[fromBase + 0]);
					expect(fromDoubleView[doubleBase + 1]).toBeCloseTo(vec[fromBase + 1]);
					expect(fromDoubleView[doubleBase + 2]).toBeCloseTo(vec[fromBase + 0]);
					expect(fromDoubleView[doubleBase + 3]).toBeCloseTo(vec[fromBase + 1]);
					expect(toDoubleView[doubleBase + 0]).toBeCloseTo(vec[toBase + 0]);
					expect(toDoubleView[doubleBase + 1]).toBeCloseTo(vec[toBase + 1]);
					expect(toDoubleView[doubleBase + 2]).toBeCloseTo(vec[toBase + 0]);
					expect(toDoubleView[doubleBase + 3]).toBeCloseTo(vec[toBase + 1]);
				}
			});
		} finally {
			net.dispose();
		}
	});

	test('defines passthrough node-to-edge attributes and keeps sparse edge buffers in sync', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);

			net.defineNodeAttribute('size', AttributeType.Float, 1);
			withNodeBuffer(net, 'size', ({ view: sizes }) => {
				sizes[nodes[0]] = 1.25;
				sizes[nodes[1]] = 2.5;
			});

			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			withEdgeBuffer(net, 'size_passthrough', ({ view: edgeView }) => {
				expect(edgeView[0]).toBeCloseTo(1.25);
				expect(edgeView[1]).toBeCloseTo(2.5);
			});

			withNodeBuffer(net, 'size', ({ view: sizes }) => {
				sizes[nodes[0]] = 9;
				sizes[nodes[1]] = 10;
			});
			net.bumpNodeAttributeVersion('size');
			withEdgeBuffer(net, 'size_passthrough', ({ view: edgeView }) => {
				expect(edgeView[0]).toBeCloseTo(9);
				expect(edgeView[1]).toBeCloseTo(10);
			});

			// Remove passthrough; subsequent node changes should not propagate
			net.removeNodeToEdgeAttribute('size_passthrough');
			withNodeBuffer(net, 'size', ({ view: sizes }) => {
				sizes[nodes[0]] = 100;
				sizes[nodes[1]] = 200;
			});
			net.bumpNodeAttributeVersion('size');
			withEdgeBuffer(net, 'size_passthrough', ({ view: edgeView }) => {
				expect(edgeView[0]).toBeCloseTo(9);
				expect(edgeView[1]).toBeCloseTo(10);
			});
		} finally {
			net.dispose();
		}
	});

	test('allows passthrough attributes to be registered before topology buffers exist', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			net.defineNodeAttribute('size', AttributeType.Float, 1);
			expect(() => net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both')).not.toThrow();

			const nodes = net.addNodes(2);
			withNodeBuffer(net, 'size', ({ view: sizes }) => {
				sizes[nodes[0]] = 4;
				sizes[nodes[1]] = 7;
			});
			net.bumpNodeAttributeVersion('size');

			net.addEdges([{ from: nodes[0], to: nodes[1] }]);
			withEdgeBuffer(net, 'size_passthrough', ({ view: edgeView }) => {
				expect(edgeView[0]).toBeCloseTo(4);
				expect(edgeView[1]).toBeCloseTo(7);
			});
		} finally {
			net.dispose();
		}
	});

	test('removes attributes and recreates them, clearing passthrough state', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);
			net.defineNodeAttribute('size', AttributeType.Float, 1);
			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			withNodeBuffer(net, 'size', ({ view: sizeBuf }) => {
				sizeBuf[nodes[0]] = 1;
				sizeBuf[nodes[1]] = 2;
			});
			net.bumpNodeAttributeVersion('size');
			withEdgeBuffer(net, 'size_passthrough', ({ view: initView }) => {
				expect(initView[0]).toBeCloseTo(1);
				expect(initView[1]).toBeCloseTo(2);
			});

			// Removing edge attribute clears passthrough; buffers should be unavailable
			net.removeEdgeAttribute('size_passthrough');
			expect(() => net.withBufferAccess(() => net.getEdgeAttributeBuffer('size_passthrough'))).toThrow(/Unknown edge attribute|attribute metadata lookup/i);

			// Recreate passthrough after removal
			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			withNodeBuffer(net, 'size', ({ view: sizeBuf }) => {
				sizeBuf[nodes[0]] = 5;
				sizeBuf[nodes[1]] = 6;
			});
			net.bumpNodeAttributeVersion('size');
			withEdgeBuffer(net, 'size_passthrough', ({ view: recView }) => {
				expect(recView[0]).toBeCloseTo(5);
				expect(recView[1]).toBeCloseTo(6);
			});

			// Removing the source node attribute breaks passthrough until redefined
			net.removeNodeAttribute('size');
			withEdgeBuffer(net, 'size_passthrough', ({ view: afterNodeRemovalView }) => {
				expect(afterNodeRemovalView[0]).toBeCloseTo(5);
				expect(afterNodeRemovalView[1]).toBeCloseTo(6);
			});
			net.defineNodeAttribute('size', AttributeType.Float, 1);
			net.removeEdgeAttribute('size_passthrough');
			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			withNodeBuffer(net, 'size', ({ view }) => {
				view[nodes[0]] = 7;
				view[nodes[1]] = 8;
			});
			net.bumpNodeAttributeVersion('size');
			withEdgeBuffer(net, 'size_passthrough', ({ view: finalView }) => {
				expect(finalView[0]).toBeCloseTo(7);
				expect(finalView[1]).toBeCloseTo(8);
			});
		} finally {
			net.dispose();
		}
	});

	test('copies node attributes into sparse edge buffers on demand', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);
			expect(edges.length).toBe(1);

			net.defineNodeAttribute('weight', AttributeType.Float, 2);
			net.defineEdgeAttribute('weight_edge', AttributeType.Float, 2);
			withNodeBuffer(net, 'weight', ({ view: weights }) => {
				weights[nodes[0] * 2] = 3;
				weights[nodes[0] * 2 + 1] = 4;
				weights[nodes[1] * 2] = 5;
				weights[nodes[1] * 2 + 1] = 6;
			});

			net.copyNodeAttributeToEdgeAttribute('weight', 'weight_edge', 'destination', false);
			withEdgeBuffer(net, 'weight_edge', ({ view: edgeWeights }) => {
				expect(edgeWeights[0]).toBeCloseTo(5);
				expect(edgeWeights[1]).toBeCloseTo(6);
			});

			net.defineEdgeAttribute('from_dup', AttributeType.Float, 4);
			net.copyNodeAttributeToEdgeAttribute('weight', 'from_dup', 'source', true);
			withEdgeBuffer(net, 'from_dup', ({ view: fromDup }) => {
				expect(Array.from(fromDup.slice(0, 4))).toEqual([3, 4, 3, 4]);
			});

			// Edge attribute already defined should block passthrough registration
			net.defineEdgeAttribute('existing_edge', AttributeType.Float, 2);
			expect(() => net.defineNodeToEdgeAttribute('weight', 'existing_edge', 'both')).toThrow(/already exists/);
		} finally {
			net.dispose();
		}
	});


	test('tracks valid ranges and slices sparse buffers', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			expect(net.nodeValidRange).toEqual({ start: 0, end: 0 });
			const nodes = net.addNodes(3);
			net.defineNodeAttribute('value', AttributeType.Float, 1);
			withNodeBuffer(net, 'value', ({ view: valView }) => {
				valView[nodes[0]] = 1;
				valView[nodes[1]] = 2;
				valView[nodes[2]] = 3;
			});

			expect(net.nodeValidRange.start).toBe(0);
			expect(net.nodeValidRange.end).toBeGreaterThanOrEqual(nodes[2] + 1);

			net.removeNodes([nodes[0]]);
			const rangeAfterRemove = net.nodeValidRange;
			expect(rangeAfterRemove.start).toBe(nodes[1]);
			expect(rangeAfterRemove.end).toBeGreaterThan(rangeAfterRemove.start);

			const slice = net.getNodeAttributeBufferSlice('value');
			expect(slice.start).toBe(rangeAfterRemove.start);
			expect(slice.end).toBe(rangeAfterRemove.end);
			expect(Array.from(slice.view)).toEqual([2, 3]);
		} finally {
			net.dispose();
		}
	});

	test('round-trips .bxnet payloads with string attributes', async () => {
		const networkInstance = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const mod = networkInstance.module;
			if (typeof mod._CXNetworkWriteBXNet !== 'function' || typeof mod._CXNetworkReadBXNet !== 'function') {
				await expect(networkInstance.saveBXNet()).rejects.toThrow(/CXNetworkWriteBXNet is not available/);
				return;
			}

			const nodes = networkInstance.addNodes(3);
			const edges = networkInstance.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);
			expect(edges.length).toBe(2);

			networkInstance.defineNodeAttribute('label', AttributeType.String, 1);
			networkInstance.defineEdgeAttribute('kind', AttributeType.String, 1);
			networkInstance.defineNetworkAttribute('title', AttributeType.String, 1);

			networkInstance.setNodeStringAttribute('label', nodes[0], 'Alpha');
			networkInstance.setNodeStringAttribute('label', nodes[1], '');
			networkInstance.setNodeStringAttribute('label', nodes[2], null);
			networkInstance.setEdgeStringAttribute('kind', 0, 'forward');
			networkInstance.setEdgeStringAttribute('kind', 1, 'return\ntrip');
			networkInstance.setNetworkStringAttribute('title', 'BX snapshot');

			const payload = await networkInstance.saveBXNet();
			expect(payload).toBeInstanceOf(Uint8Array);
			expect(payload.byteLength).toBeGreaterThan(0);

			const restored = await HeliosNetwork.fromBXNet(payload);
			try {
				expect(restored.directed).toBe(false);
				expect(restored.nodeCount).toBe(3);
				expect(restored.getNodeStringAttribute('label', 0)).toBe('Alpha');
				expect(restored.getNodeStringAttribute('label', 1)).toBe('');
				expect(restored.getNodeStringAttribute('label', 2)).toBeNull();
				expect(restored.getEdgeStringAttribute('kind', 1)).toBe('return\ntrip');
				expect(restored.getNetworkStringAttribute('title')).toBe('BX snapshot');
			} finally {
				restored.dispose();
			}
		} finally {
			networkInstance.dispose();
		}
	});

	test('round-trips .zxnet payloads via filesystem paths and alternate outputs', async () => {
		const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'helios-'));
		const targetPath = path.join(tmpDir, `network-${randomUUID()}.zxnet`);
		const networkInstance = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			if (typeof networkInstance.module._CXNetworkWriteZXNet !== 'function' || typeof networkInstance.module._CXNetworkReadZXNet !== 'function') {
				await expect(networkInstance.saveZXNet()).rejects.toThrow(/CXNetworkWriteZXNet is not available/);
				return;
			}

			const nodes = networkInstance.addNodes(3);
			const edges = networkInstance.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);
			expect(edges.length).toBe(2);

			const result = await networkInstance.saveZXNet({ path: targetPath, compressionLevel: 4 });
			expect(result).toBeUndefined();

			const stats = await fs.stat(targetPath);
			expect(stats.size).toBeGreaterThan(0);

			const reloaded = await HeliosNetwork.fromZXNet(targetPath);
			try {
				expect(reloaded.directed).toBe(false);
				expect(reloaded.nodeCount).toBe(networkInstance.nodeCount);
				expect(reloaded.edgeCount).toBe(networkInstance.edgeCount);

				const base64 = await reloaded.saveZXNet({ format: 'base64' });
				expect(typeof base64).toBe('string');
				expect(base64.length).toBeGreaterThan(0);
			} finally {
				reloaded.dispose();
			}
		} finally {
			networkInstance.dispose();
			await fs.rm(tmpDir, { recursive: true, force: true });
		}
	});

	test('round-trips .zxnet payloads with string attributes', async () => {
		const networkInstance = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const mod = networkInstance.module;
			if (typeof mod._CXNetworkWriteZXNet !== 'function' || typeof mod._CXNetworkReadZXNet !== 'function') {
				await expect(networkInstance.saveZXNet()).rejects.toThrow(/CXNetworkWriteZXNet is not available/);
				return;
			}

			const nodes = networkInstance.addNodes(2);
			const edges = networkInstance.addEdges([{ from: nodes[0], to: nodes[1] }]);
			expect(edges.length).toBe(1);

			networkInstance.defineNodeAttribute('status', AttributeType.String, 1);
			networkInstance.defineEdgeAttribute('label', AttributeType.String, 1);
			networkInstance.defineNetworkAttribute('subtitle', AttributeType.String, 1);

			networkInstance.setNodeStringAttribute('status', nodes[0], 'Delta');
			networkInstance.setNodeStringAttribute('status', nodes[1], 'line\nbreak');
			networkInstance.setEdgeStringAttribute('label', 0, '');
			networkInstance.setNetworkStringAttribute('subtitle', null);

			const payload = await networkInstance.saveZXNet({ compressionLevel: 2 });
			expect(payload).toBeInstanceOf(Uint8Array);
			expect(payload.byteLength).toBeGreaterThan(0);

			const restored = await HeliosNetwork.fromZXNet(payload);
			try {
				expect(restored.directed).toBe(true);
				expect(restored.nodeCount).toBe(2);
				expect(restored.getNodeStringAttribute('status', 0)).toBe('Delta');
				expect(restored.getNodeStringAttribute('status', 1)).toBe('line\nbreak');
				expect(restored.getEdgeStringAttribute('label', 0)).toBe('');
				expect(restored.getNetworkStringAttribute('subtitle')).toBeNull();
			} finally {
				restored.dispose();
			}
		} finally {
			networkInstance.dispose();
		}
	});

	test('round-trips 32-bit integers and big integers across bxnet/zxnet/xnet', async () => {
		const formats = ['bxnet', 'zxnet', 'xnet'];

		for (const format of formats) {
			const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
			const mod = net.module;
			const supported = format === 'bxnet'
				? typeof mod._CXNetworkWriteBXNet === 'function' && typeof mod._CXNetworkReadBXNet === 'function'
				: format === 'zxnet'
					? typeof mod._CXNetworkWriteZXNet === 'function' && typeof mod._CXNetworkReadZXNet === 'function'
					: typeof mod._CXNetworkWriteXNet === 'function' && typeof mod._CXNetworkReadXNet === 'function';

			try {
				if (!supported) {
					const savePromise = format === 'bxnet'
						? net.saveBXNet()
						: format === 'zxnet'
							? net.saveZXNet()
							: net.saveXNet();
					await expect(savePromise).rejects.toThrow();
					continue;
				}

				const nodes = net.addNodes(2);
				net.defineNodeAttribute('i32', AttributeType.Integer, 1);
				net.defineNodeAttribute('u32', AttributeType.UnsignedInteger, 1);
				net.defineNodeAttribute('i64', AttributeType.BigInteger, 1);
				net.defineNodeAttribute('u64', AttributeType.UnsignedBigInteger, 1);

				net.withBufferAccess(() => {
					const i32 = net.getNodeAttributeBuffer('i32').view;
					const u32 = net.getNodeAttributeBuffer('u32').view;
					const i64 = net.getNodeAttributeBuffer('i64').view;
					const u64 = net.getNodeAttributeBuffer('u64').view;

					i32[nodes[0]] = -1;
					i32[nodes[1]] = 2147483647;
					u32[nodes[0]] = 0;
					u32[nodes[1]] = 4294967295;
					i64[nodes[0]] = -(1n << 40n);
					i64[nodes[1]] = 1n << 41n;
					u64[nodes[0]] = 1n;
					u64[nodes[1]] = (1n << 60n) - 5n;
				});

				const payload = await (format === 'bxnet'
					? net.saveBXNet()
					: format === 'zxnet'
						? net.saveZXNet({ compressionLevel: 1 })
						: net.saveXNet());
				const restored = await (format === 'bxnet'
					? HeliosNetwork.fromBXNet(payload)
					: format === 'zxnet'
						? HeliosNetwork.fromZXNet(payload)
						: HeliosNetwork.fromXNet(payload));

				try {
					for (const name of ['i32', 'u32', 'i64', 'u64']) {
						const meta = restored._ensureAttributeMetadata('node', name);
						restored._attributePointers('node', name, meta);
					}
					restored.withBufferAccess(() => {
						const ri32 = restored.getNodeAttributeBuffer('i32').view;
						const ru32 = restored.getNodeAttributeBuffer('u32').view;
						const ri64 = restored.getNodeAttributeBuffer('i64').view;
						const ru64 = restored.getNodeAttributeBuffer('u64').view;

						expect(ri32).toBeInstanceOf(Int32Array);
						expect(ru32).toBeInstanceOf(Uint32Array);
						expect(ri64).toBeInstanceOf(BigInt64Array);
						expect(ru64).toBeInstanceOf(BigUint64Array);

						expect(Array.from(ri32.slice(0, restored.nodeCount))).toEqual([-1, 2147483647]);
						expect(Array.from(ru32.slice(0, restored.nodeCount))).toEqual([0, 4294967295]);
						expect(Array.from(ri64.slice(0, restored.nodeCount))).toEqual([-(1n << 40n), 1n << 41n]);
						expect(Array.from(ru64.slice(0, restored.nodeCount))).toEqual([1n, (1n << 60n) - 5n]);
					});
				} finally {
					restored.dispose();
				}
			} finally {
				net.dispose();
			}
		}
	});

	test('round-trips .xnet payloads with string attributes and compaction', async () => {
		const networkInstance = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const mod = networkInstance.module;
			if (typeof mod._CXNetworkWriteXNet !== 'function' || typeof mod._CXNetworkReadXNet !== 'function') {
				await expect(networkInstance.saveXNet()).rejects.toThrow(/CXNetworkWriteXNet is not available/);
				return;
			}

			const nodes = networkInstance.addNodes(3);
			networkInstance.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			networkInstance.defineNodeAttribute('score', AttributeType.Float, 1);
			networkInstance.defineNodeAttribute('label', AttributeType.String, 1);
			networkInstance.defineEdgeAttribute('kind', AttributeType.String, 1);
			networkInstance.defineNetworkAttribute('title', AttributeType.String, 1);

			withNodeBuffer(networkInstance, 'score', ({ view }) => {
				view[nodes[0]] = 1.25;
				view[nodes[1]] = 2.5;
				view[nodes[2]] = 3.75;
			});
			networkInstance.setNodeStringAttribute('label', nodes[0], 'Alpha');
			networkInstance.setNodeStringAttribute('label', nodes[1], 'Beta Value');
			networkInstance.setNodeStringAttribute('label', nodes[2], 'Gamma#Tag');
			networkInstance.setEdgeStringAttribute('kind', 0, 'forward');
			networkInstance.setEdgeStringAttribute('kind', 1, 'return\ntrip');
			networkInstance.setNetworkStringAttribute('title', 'Human Readable XNET');

			const payload = await networkInstance.saveXNet();
			expect(payload).toBeInstanceOf(Uint8Array);
			expect(payload.byteLength).toBeGreaterThan(0);

			const restored = await HeliosNetwork.fromXNet(payload);
			const tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'helios-xnet-'));
			const targetPath = path.join(tmpDir, `graph-${randomUUID()}.xnet`);
				try {
					expect(restored.directed).toBe(true);
					expect(restored.nodeCount).toBe(3);
					expect(restored.edgeCount).toBe(2);
					expect(restored.getNodeAttributeNames()).toEqual(expect.arrayContaining(['score', 'label', '_original_ids_']));
					expect(restored.getEdgeAttributeNames()).toEqual(expect.arrayContaining(['kind']));
					expect(restored.getNetworkAttributeNames()).toEqual(expect.arrayContaining(['title']));
					withNodeBuffer(restored, 'score', ({ view }) => {
						expect(view[1]).toBeCloseTo(2.5);
					});
					expect(restored.getNodeStringAttribute('label', 2)).toBe('Gamma#Tag');
					expect(restored.getEdgeStringAttribute('kind', 1)).toBe('return\ntrip');
					expect(restored.getNetworkStringAttribute('title')).toBe('Human Readable XNET');
					expect(restored.getNodeStringAttribute('_original_ids_', 0)).toBe('0');

				await restored.saveXNet({ path: targetPath });
				const stats = await fs.stat(targetPath);
				expect(stats.size).toBeGreaterThan(0);

				const upgraded = await HeliosNetwork.fromXNet(targetPath);
				try {
					expect(upgraded.nodeCount).toBe(3);
					expect(upgraded.getNodeStringAttribute('_original_ids_', 1)).toBe('1');
				} finally {
					upgraded.dispose();
				}
			} finally {
				restored.dispose();
				await fs.rm(tmpDir, { recursive: true, force: true });
			}
			} finally {
				networkInstance.dispose();
			}
		});

		test('loads legacy .xnet string attributes into attribute name lists', async () => {
			const legacyText = [
				'#vertices 3 nonweighted',
				'"Node A"',
				'"Node B"',
				'"Node C"',
				'#edges nonweighted directed',
				'0 1',
				'1 2',
				'#v "Main Category" s',
				'"Alpha"',
				'"Beta"',
				'"Gamma"',
			].join('\n');
			const payload = new TextEncoder().encode(legacyText);
			const restored = await HeliosNetwork.fromXNet(payload);
			try {
				expect(restored.getNodeAttributeNames()).toEqual(expect.arrayContaining(['Label', 'Main Category']));
				expect(restored.hasNodeAttribute('Label')).toBe(true);
				expect(restored.hasNodeAttribute('Main Category')).toBe(true);
				expect(restored.getNodeStringAttribute('Label', 0)).toBe('Node A');
				expect(restored.getNodeStringAttribute('Main Category', 2)).toBe('Gamma');
			} finally {
				restored.dispose();
			}
		});

		test('saveXNet respects attribute allow/ignore filters', async () => {
			const networkInstance = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = networkInstance.addNodes(3);
			const edges = networkInstance.addEdges([[nodes[0], nodes[1]]]);

			networkInstance.defineNodeAttribute('score', AttributeType.Float, 1);
			networkInstance.defineNodeAttribute('label', AttributeType.String, 1);
			networkInstance.defineNodeAttribute('hidden', AttributeType.Float, 1);
			networkInstance.defineEdgeAttribute('weight', AttributeType.Float, 1);
			networkInstance.defineEdgeAttribute('status', AttributeType.String, 1);
			networkInstance.defineNetworkAttribute('title', AttributeType.String, 1);
			networkInstance.defineNetworkAttribute('version', AttributeType.Integer, 1);

			withNodeBuffer(networkInstance, 'score', ({ view }) => {
				view[0] = 1.5;
			});
			withNodeBuffer(networkInstance, 'hidden', ({ view }) => {
				view[0] = 99;
			});
			networkInstance.setNodeStringAttribute('label', 0, 'keep?');
			withEdgeBuffer(networkInstance, 'weight', ({ view }) => {
				view[edges[0]] = 2.25;
			});
			networkInstance.setEdgeStringAttribute('status', edges[0], 'ok');
			networkInstance.setNetworkStringAttribute('title', 'Filtered XNET');
			withNetworkBuffer(networkInstance, 'version', ({ view }) => {
				view[0] = 7;
			});

			const payload = await networkInstance.saveXNet({
				allowAttributes: {
					node: ['score', 'label'],
					edge: ['weight'],
					network: ['title', 'version'],
				},
				ignoreAttributes: {
					node: ['label'],
					network: ['version'],
				},
			});
			const restored = await HeliosNetwork.fromXNet(payload);
			try {
				expect(restored.hasNodeAttribute('score')).toBe(true);
				expect(restored.hasNodeAttribute('label')).toBe(false);
				expect(restored.hasNodeAttribute('hidden')).toBe(false);
				expect(restored.hasEdgeAttribute('weight')).toBe(true);
				expect(restored.hasEdgeAttribute('status')).toBe(false);
				expect(restored.hasNetworkAttribute('title')).toBe(true);
				expect(restored.hasNetworkAttribute('version')).toBe(false);
				expect(restored.hasNodeAttribute('_original_ids_')).toBe(true);
				withNodeBuffer(restored, 'score', ({ view }) => {
					expect(view[0]).toBeCloseTo(1.5);
				});
				withEdgeBuffer(restored, 'weight', ({ view }) => {
					expect(view[edges[0]]).toBeCloseTo(2.25);
				});
				expect(restored.getNetworkStringAttribute('title')).toBe('Filtered XNET');
			} finally {
				restored.dispose();
			}
		} finally {
			networkInstance.dispose();
		}
	});

	test('saveBXNet respects attribute allow/ignore filters', async () => {
		const networkInstance = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = networkInstance.addNodes(3);
			const edges = networkInstance.addEdges([[nodes[0], nodes[1]], [nodes[1], nodes[2]]]);

			networkInstance.defineNodeAttribute('score', AttributeType.Float, 1);
			networkInstance.defineNodeAttribute('rank', AttributeType.Integer, 1);
			networkInstance.defineEdgeAttribute('weight', AttributeType.Float, 1);
			networkInstance.defineEdgeAttribute('flag', AttributeType.Boolean, 1);
			networkInstance.defineNetworkAttribute('version', AttributeType.Integer, 1);

			withNodeBuffer(networkInstance, 'score', ({ view }) => {
				view[0] = 2.5;
			});
			withNodeBuffer(networkInstance, 'rank', ({ view }) => {
				view[0] = 4;
			});
			withEdgeBuffer(networkInstance, 'weight', ({ view }) => {
				view[edges[0]] = 3.5;
			});
			withEdgeBuffer(networkInstance, 'flag', ({ view }) => {
				view[edges[0]] = 1;
			});
			withNetworkBuffer(networkInstance, 'version', ({ view }) => {
				view[0] = 42;
			});

			const payload = await networkInstance.saveBXNet({
				allowAttributes: {
					node: ['score'],
					edge: ['weight', 'flag'],
					network: ['version'],
				},
				ignoreAttributes: {
					edge: ['flag'],
				},
			});
			const restored = await HeliosNetwork.fromBXNet(payload);
			try {
				expect(restored.hasNodeAttribute('score')).toBe(true);
				expect(restored.hasNodeAttribute('rank')).toBe(false);
				expect(restored.hasEdgeAttribute('weight')).toBe(true);
				expect(restored.hasEdgeAttribute('flag')).toBe(false);
				expect(restored.hasNetworkAttribute('version')).toBe(true);
				withNodeBuffer(restored, 'score', ({ view }) => {
					expect(view[0]).toBeCloseTo(2.5);
				});
				withEdgeBuffer(restored, 'weight', ({ view }) => {
					expect(view[edges[0]]).toBeCloseTo(3.5);
				});
				withNetworkBuffer(restored, 'version', ({ view }) => {
					expect(view[0]).toBe(42);
				});
			} finally {
				restored.dispose();
			}
		} finally {
			networkInstance.dispose();
		}
	});

	test('compact reindexes networks and preserves attribute stores', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			if (typeof net.module._CXNetworkCompact !== 'function') {
				expect(() => net.compact()).toThrow(/CXNetworkCompact is not available/);
				return;
			}

			const nodes = net.addNodes(5);
			net.defineNodeAttribute('jsMeta', AttributeType.Javascript);
			net.defineNodeAttribute('label', AttributeType.String);
			net.defineNodeAttribute('score', AttributeType.Float);
			net.defineEdgeAttribute('flag', AttributeType.Boolean);

			withNodeBuffer(net, 'jsMeta', (meta) => {
				meta.set(nodes[1], { name: 'one' });
				meta.set(nodes[3], { name: 'three' });
			});

			net.setNodeStringAttribute('label', nodes[1], 'one');
			net.setNodeStringAttribute('label', nodes[3], 'three');

			withNodeBuffer(net, 'score', ({ view: scores }) => {
				scores[nodes[1]] = 1.25;
				scores[nodes[3]] = 3.5;
			});

			const edges = net.addEdges([
				{ from: nodes[1], to: nodes[3] },
				{ from: nodes[3], to: nodes[4] },
			]);
			withEdgeBuffer(net, 'flag', ({ view }) => {
				view[edges[0]] = 1;
			});

			net.removeNodes([nodes[0], nodes[2]]);
			net.removeEdges([edges[0]]);

			net.compact({
				nodeOriginalIndexAttribute: 'origin_node',
				edgeOriginalIndexAttribute: 'origin_edge',
			});

			expect(net.nodeCount).toBe(3);
			expect(net.nodeCapacity).toBe(net.nodeCount);
			expect(net.edgeCount).toBe(1);
			expect(net.edgeCapacity).toBe(net.edgeCount);

			expect(net.getNodeStringAttribute('label', 0)).toBe('one');
			expect(net.getNodeStringAttribute('label', 1)).toBe('three');
			expect(net.getNodeStringAttribute('label', 2)).toBe(null);

			for (const [scope, name] of [
				['node', 'jsMeta'],
				['node', 'score'],
				['node', 'origin_node'],
				['edge', 'origin_edge'],
				['edge', 'flag'],
			]) {
				const meta = net._ensureAttributeMetadata(scope, name);
				net._attributePointers(scope, name, meta);
			}
			net.withBufferAccess(() => {
				const compactMeta = net.getNodeAttributeBuffer('jsMeta');
				expect(compactMeta.get(0)).toEqual({ name: 'one' });
				expect(compactMeta.get(1)).toEqual({ name: 'three' });
				expect(compactMeta.get(2)).toBeNull();

				const compactScores = net.getNodeAttributeBuffer('score').view;
				expect(compactScores[0]).toBeCloseTo(1.25);
				expect(compactScores[1]).toBeCloseTo(3.5);

				const originNodes = net.getNodeAttributeBuffer('origin_node').view;
				expect(Array.from(originNodes.slice(0, net.nodeCount), Number)).toEqual([1, 3, 4]);

				const originEdges = net.getEdgeAttributeBuffer('origin_edge').view;
				expect(Array.from(originEdges.slice(0, net.edgeCount), Number)).toEqual([1]);

				const edgeFlags = net.getEdgeAttributeBuffer('flag').view;
				expect(edgeFlags[0]).toBe(0);
			});
		} finally {
			net.dispose();
		}
	});
});
