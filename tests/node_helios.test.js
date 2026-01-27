import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { beforeAll, afterAll, describe, expect, test } from 'vitest';
import HeliosNetwork, { AttributeType, DenseColorEncodingFormat } from '../src/helios-network.js';

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
		expect(Array.from(network.nodeIndices)).not.toContain(createdNodes[3]);
	});

	test('handles attributes before and after adding nodes/edges', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			expect(net.nodeCount).toBe(0);
			expect(net.edgeCount).toBe(0);

			net.defineNodeAttribute('pre_node_weight', AttributeType.Float, 1);
			net.defineEdgeAttribute('pre_edge_flag', AttributeType.Boolean, 1);
			expect(() => net.getNodeAttributeBuffer('pre_node_weight')).toThrow(/not available/);
			expect(() => net.getEdgeAttributeBuffer('pre_edge_flag')).toThrow(/not available/);

			const nodes = net.addNodes(3);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			let nodeWeights = net.getNodeAttributeBuffer('pre_node_weight').view;
			let edgeFlags = net.getEdgeAttributeBuffer('pre_edge_flag').view;
			nodeWeights[nodes[0]] = 1.5;
			nodeWeights[nodes[1]] = 2.5;
			nodeWeights[nodes[2]] = 3.5;
			edgeFlags[edges[0]] = 1;
			edgeFlags[edges[1]] = 0;

			net.defineNodeAttribute('label', AttributeType.String, 1);
			net.defineEdgeAttribute('capacity', AttributeType.Double, 1);
			net.setNodeStringAttribute('label', nodes[0], 'alpha');
			net.setNodeStringAttribute('label', nodes[1], 'beta');
			net.setNodeStringAttribute('label', nodes[2], 'gamma');
			const capacity = net.getEdgeAttributeBuffer('capacity').view;
			capacity[edges[0]] = 10.5;
			capacity[edges[1]] = 20.25;

			nodeWeights[nodes[1]] = 4.75;
			net.setNodeStringAttribute('label', nodes[2], 'gamma-updated');
			capacity[edges[1]] = 30;

			expect(nodeWeights[nodes[0]]).toBeCloseTo(1.5);
			expect(nodeWeights[nodes[1]]).toBeCloseTo(4.75);
			expect(nodeWeights[nodes[2]]).toBeCloseTo(3.5);
			expect(edgeFlags[edges[0]]).toBe(1);
			expect(edgeFlags[edges[1]]).toBe(0);
			expect(net.getNodeStringAttribute('label', nodes[0])).toBe('alpha');
			expect(net.getNodeStringAttribute('label', nodes[1])).toBe('beta');
			expect(net.getNodeStringAttribute('label', nodes[2])).toBe('gamma-updated');
			expect(capacity[edges[0]]).toBeCloseTo(10.5);
			expect(capacity[edges[1]]).toBeCloseTo(30);
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
		} finally {
			net.dispose();
		}
	});

	test('manages primitive attribute buffers', () => {
		const nodes = network.addNodes(2);
		network.defineNodeAttribute('weight', AttributeType.Float, 1);

		const { view } = network.getNodeAttributeBuffer('weight');
		view[nodes[0]] = 1.5;
		view[nodes[1]] = 2.5;

		expect(view[nodes[0]]).toBeCloseTo(1.5);
		expect(view[nodes[1]]).toBeCloseTo(2.5);
	});

	test('supports string and JS-managed attributes', () => {
		const nodes = network.addNodes(1);
		const edges = network.addEdges([{ from: nodes[0], to: nodes[0] }]);

		network.defineNodeAttribute('label', AttributeType.String, 1);
		network.defineEdgeAttribute('meta', AttributeType.Javascript, 1);

		network.setNodeStringAttribute('label', nodes[0], 'node-zero');
		expect(network.getNodeStringAttribute('label', nodes[0])).toBe('node-zero');

		const edgeMeta = network.getEdgeAttributeBuffer('meta');
		edgeMeta.set(edges[0], { custom: 42 });
		expect(edgeMeta.get(edges[0])).toEqual({ custom: 42 });
	});

	test('categorizes and decategorizes string attributes', () => {
		const nodes = network.addNodes(4);
		network.defineNodeAttribute('group', AttributeType.String, 1);

		network.setNodeStringAttribute('group', nodes[0], 'apple');
		network.setNodeStringAttribute('group', nodes[1], '');
		network.setNodeStringAttribute('group', nodes[2], '__NA__');
		network.setNodeStringAttribute('group', nodes[3], 'banana');

		network.categorizeNodeAttribute('group', { sortOrder: 'frequency' });
		const categorized = network.getNodeAttributeBuffer('group').view;
		expect(categorized).toBeInstanceOf(Int32Array);
		expect(categorized[nodes[0]]).toBeGreaterThanOrEqual(0);
		expect(categorized[nodes[1]]).toBe(-1);
		expect(categorized[nodes[2]]).toBe(-1);
		expect(categorized[nodes[3]]).toBeGreaterThanOrEqual(0);

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

			const codes = net.getNodeAttributeBuffer('category').view;
			const counts = new Map();
			for (let i = 0; i < nodes.length; i += 1) {
				const code = codes[nodes[i]];
				expect(code).toBeGreaterThanOrEqual(0);
				counts.set(code, (counts.get(code) ?? 0) + 1);
			}
			for (const entry of dict.entries) {
				expect(counts.get(entry.id) ?? 0).toBeGreaterThan(0);
			}
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

			net.addDenseNodeAttributeBuffer('pos');
			net.addDenseEdgeAttributeBuffer('weight');
			net.updateDenseNodeAttributeBuffer('pos');
			net.updateDenseEdgeAttributeBuffer('weight');

			const report = net.getBufferMemoryUsage();
			expect(report.wasm.heapBytes).toBeGreaterThan(0);

			expect(report.buffers['topology.edgeFromTo']).toBe(net.edgeCapacity * 2 * Uint32Array.BYTES_PER_ELEMENT);
			expect(report.buffers['topology.nodeFreeList']).toBe(net.nodeCapacity * Uint32Array.BYTES_PER_ELEMENT);
			expect(report.buffers['topology.edgeFreeList']).toBe(net.edgeCapacity * Uint32Array.BYTES_PER_ELEMENT);

			expect(report.buffers['sparse.node.attribute.pos']).toBeGreaterThan(0);

			expect(report.buffers['dense.node.attribute.pos']).toBeGreaterThanOrEqual(0);
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
			net.addDenseNodeAttributeBuffer('pos');
			net.updateDenseNodeAttributeBuffer('pos');

			const versions = net.getBufferVersions();
			expect(typeof versions.topology.node).toBe('number');
			expect(typeof versions.topology.edge).toBe('number');
			expect(typeof versions.attributes.node.pos).toBe('number');
			expect(typeof versions.dense.node.attributes.pos).toBe('number');
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

	test('exposes dense index copies and full-coverage selectors', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(4);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[2], to: nodes[3] },
			]);

			const nodeOrder = Uint32Array.from(nodes).reverse();
			net.setDenseNodeOrder(nodeOrder);
			expect(Array.from(net.nodeIndices)).toEqual(Array.from(nodes)); // native order, not dense
			net.updateDenseNodeIndexBuffer();
			expect(Array.from(net.getDenseNodeIndexView().view)).toEqual(Array.from(nodeOrder));

			const edgeOrder = Uint32Array.from(edges).reverse();
			net.setDenseEdgeOrder(edgeOrder);
			expect(Array.from(net.edgeIndices)).toEqual(Array.from(edges)); // native order, not dense
			net.updateDenseEdgeIndexBuffer();
			expect(Array.from(net.getDenseEdgeIndexView().view)).toEqual(Array.from(edgeOrder));

			expect(() => net.withBufferAccess(() => net.nodeIndices)).toThrow(/nodeIndices/);
			expect(() => net.withBufferAccess(() => net.edgeIndices)).toThrow(/edgeIndices/);

			const allNodes = net.nodes;
			const allEdges = net.edges;
			expect(allNodes.count).toBe(net.nodeCount);
			expect(allEdges.count).toBe(net.edgeCount);
			expect(new Set(allNodes)).toEqual(new Set(nodes));
			expect(new Set(allEdges)).toEqual(new Set(edges));
			expect(() => net.withBufferAccess(() => [...net.nodes])).toThrow(/nodeIndices/);

			const edgeIndicesCopy = net.edgeIndices;
			edgeIndicesCopy[0] = 123456;
			expect(net.edgeIndices[0]).toBe(123456); // cached snapshot reused until topology changes
			const next = net.addEdges([{ from: nodes[0], to: nodes[2] }]);
			expect(net.edgeIndices[0]).not.toBe(123456); // cache invalidated on topology change
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
		const capacity = network.getEdgeAttributeBuffer('proxy_capacity');
		capacity.view[edges[0]] = 1.25;
		capacity.view[edges[1]] = 2.5;

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

			original.getNodeAttributeBuffer('weight').view[nodes[0]] = 3.5;
			original.getEdgeAttributeBuffer('capacity').view[edges[0]] = 7.25;

			const payload = await original.saveBXNet();
			expect(payload).toBeInstanceOf(Uint8Array);
			expect(payload.byteLength).toBeGreaterThan(0);

			const restored = await HeliosNetwork.fromBXNet(payload);
				try {
					expect(restored.directed).toBe(true);
					expect(restored.nodeCount).toBe(2);
					expect(restored.edgeCount).toBe(1);
					const restoredWeight = restored.getNodeAttributeBuffer('weight').view;
					expect(restoredWeight[0]).toBeCloseTo(3.5);
					const restoredCapacity = restored.getEdgeAttributeBuffer('capacity').view;
					expect(restoredCapacity[0]).toBeCloseTo(7.25);
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
			const posBuf = net.getNodeAttributeBuffer('position').view;
			for (let i = 0; i < nodes.length; i++) {
				const base = nodes[i] * 4;
				posBuf[base + 0] = i + 0.1;
				posBuf[base + 1] = i + 0.2;
				posBuf[base + 2] = i + 0.3;
				posBuf[base + 3] = 1.0;
			}

			net.defineNodeAttribute('size', AttributeType.Float, 1);
			net.defineNodeToEdgeAttribute('size', 'size_endpoints', 'both');
			const sizeBuf = net.getNodeAttributeBuffer('size').view;
			for (let i = 0; i < nodes.length; i++) {
				sizeBuf[nodes[i]] = i + 10;
			}

			const segPtr = mod._malloc(Float32Array.BYTES_PER_ELEMENT * requiredEdges * 8);
			const segments = new Float32Array(mod.HEAPF32.buffer, segPtr, requiredEdges * 8);
			const segEdges = net.writeActiveEdgeSegments(posBuf, segments, 4);
			expect(segEdges).toBe(requiredEdges);

			const packedEdges = edgeSpecs.filter((_, idx) => idx !== 1);
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
			mod._free(segPtr);

			net.updateDenseEdgeAttributeBuffer('size_endpoints');
			const denseSize = net.getDenseEdgeAttributeView('size_endpoints');
			expect(denseSize.count).toBe(requiredEdges);
			expect(denseSize.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 2);
			expect(denseSize.validStart).toBe(net.edgeValidRange.start);
			const denseView = denseSize.view;
			const peekSize = net.getDenseEdgeAttributeView('size_endpoints');
			expect(peekSize.pointer).toBe(denseSize.pointer);
			const edgeSizeView = net.getEdgeAttributeBuffer('size_endpoints').view;
			const activeEdgeIndices = Array.from(edges).filter((_, idx) => idx !== 1);
			for (let i = 0; i < requiredEdges; i++) {
				const edge = packedEdges[i];
				const outBase = i * 2;
				const sparseBase = activeEdgeIndices[i] * 2;
				expect(denseView[outBase + 0]).toBeCloseTo(sizeBuf[edge.from]);
				expect(denseView[outBase + 1]).toBeCloseTo(sizeBuf[edge.to]);
				expect(edgeSizeView[sparseBase + 0]).toBeCloseTo(sizeBuf[edge.from]);
				expect(edgeSizeView[sparseBase + 1]).toBeCloseTo(sizeBuf[edge.to]);
			}
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
			const vec = net.getNodeAttributeBuffer('vec').view;
			for (let i = 0; i < nodes.length; i++) {
				const base = nodes[i] * 2;
				vec[base + 0] = i + 0.5;
				vec[base + 1] = i + 1.5;
			}

			net.defineNodeToEdgeAttribute('vec', 'from_only', 'source', false);
			net.defineNodeToEdgeAttribute('vec', 'to_only', 'destination', false);
			net.defineNodeToEdgeAttribute('vec', 'both_vec', 'both');
			net.defineNodeToEdgeAttribute('vec', 'from_only_double', 'source', true);
			net.defineNodeToEdgeAttribute('vec', 'to_only_double', 'destination', true);

			net.updateDenseEdgeAttributeBuffer('from_only');
			net.updateDenseEdgeAttributeBuffer('to_only');
			net.updateDenseEdgeAttributeBuffer('both_vec');
			net.updateDenseEdgeAttributeBuffer('from_only_double');
			net.updateDenseEdgeAttributeBuffer('to_only_double');

			const fromDense = net.getDenseEdgeAttributeView('from_only');
			const toDense = net.getDenseEdgeAttributeView('to_only');
			const bothDense = net.getDenseEdgeAttributeView('both_vec');
			const fromDoubleDense = net.getDenseEdgeAttributeView('from_only_double');
			const toDoubleDense = net.getDenseEdgeAttributeView('to_only_double');

			expect(fromDense.count).toBe(edges.length);
			expect(toDense.count).toBe(edges.length);
			expect(bothDense.count).toBe(edges.length);
			expect(fromDoubleDense.count).toBe(edges.length);
			expect(toDoubleDense.count).toBe(edges.length);
			expect(fromDense.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 2);
			expect(toDense.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 2);
			expect(bothDense.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 4);
			expect(fromDoubleDense.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 4);
			expect(toDoubleDense.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 4);

			net.updateDenseEdgeIndexBuffer();
			const edgeOrder = net.getDenseEdgeIndexView();
			const denseEdges = edgeOrder.view;

			const fromView = fromDense.view;
			const toView = toDense.view;
			const bothView = bothDense.view;
			const fromDoubleView = fromDoubleDense.view;
			const toDoubleView = toDoubleDense.view;

			for (let i = 0; i < denseEdges.length; i++) {
				const edgeIdx = denseEdges[i];
				const fromNode = net.edgesView[edgeIdx * 2];
				const toNode = net.edgesView[edgeIdx * 2 + 1];
				const fromBase = fromNode * 2;
				const toBase = toNode * 2;
				const denseBase = i * 2;
				expect(fromView[denseBase + 0]).toBeCloseTo(vec[fromBase + 0]);
				expect(fromView[denseBase + 1]).toBeCloseTo(vec[fromBase + 1]);
				expect(toView[denseBase + 0]).toBeCloseTo(vec[toBase + 0]);
				expect(toView[denseBase + 1]).toBeCloseTo(vec[toBase + 1]);

				const bothBase = i * 4;
				expect(bothView[bothBase + 0]).toBeCloseTo(vec[fromBase + 0]);
				expect(bothView[bothBase + 1]).toBeCloseTo(vec[fromBase + 1]);
				expect(bothView[bothBase + 2]).toBeCloseTo(vec[toBase + 0]);
				expect(bothView[bothBase + 3]).toBeCloseTo(vec[toBase + 1]);

				const doubleBase = i * 4;
				expect(fromDoubleView[doubleBase + 0]).toBeCloseTo(vec[fromBase + 0]);
				expect(fromDoubleView[doubleBase + 1]).toBeCloseTo(vec[fromBase + 1]);
				expect(fromDoubleView[doubleBase + 2]).toBeCloseTo(vec[fromBase + 0]);
				expect(fromDoubleView[doubleBase + 3]).toBeCloseTo(vec[fromBase + 1]);
				expect(toDoubleView[doubleBase + 0]).toBeCloseTo(vec[toBase + 0]);
				expect(toDoubleView[doubleBase + 1]).toBeCloseTo(vec[toBase + 1]);
				expect(toDoubleView[doubleBase + 2]).toBeCloseTo(vec[toBase + 0]);
				expect(toDoubleView[doubleBase + 3]).toBeCloseTo(vec[toBase + 1]);
			}
		} finally {
			net.dispose();
		}
	});

	test('defines passthrough node-to-edge attributes and repacks dense buffers', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);

			net.defineNodeAttribute('size', AttributeType.Float, 1);
			const sizes = net.getNodeAttributeBuffer('size').view;
			sizes[nodes[0]] = 1.25;
			sizes[nodes[1]] = 2.5;

			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const dense = net.getDenseEdgeAttributeView('size_passthrough');
			expect(dense.count).toBe(edges.length);
			expect(dense.stride).toBe(Float32Array.BYTES_PER_ELEMENT * 2);
			const denseView = dense.view;
			expect(denseView[0]).toBeCloseTo(1.25);
			expect(denseView[1]).toBeCloseTo(2.5);

			sizes[nodes[0]] = 9;
			sizes[nodes[1]] = 10;
			net.bumpNodeAttributeVersion('size');
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const refreshed = net.getDenseEdgeAttributeView('size_passthrough');
			const refreshedView = refreshed.view;
			expect(refreshedView[0]).toBeCloseTo(9);
			expect(refreshedView[1]).toBeCloseTo(10);

			// Remove passthrough; subsequent node changes should not propagate
			net.removeNodeToEdgeAttribute('size_passthrough');
			sizes[nodes[0]] = 100;
			sizes[nodes[1]] = 200;
			net.bumpNodeAttributeVersion('size');
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const afterRemoval = net.getDenseEdgeAttributeView('size_passthrough');
			const afterRemovalView = afterRemoval.view;
			expect(afterRemovalView[0]).toBeCloseTo(9);
			expect(afterRemovalView[1]).toBeCloseTo(10);
		} finally {
			net.dispose();
		}
	});

	test('removes attributes and recreates them, clearing dense/passthrough state', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);
			net.defineNodeAttribute('size', AttributeType.Float, 1);
			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			const sizeBuf = net.getNodeAttributeBuffer('size').view;
			sizeBuf[nodes[0]] = 1;
			sizeBuf[nodes[1]] = 2;
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const initialDense = net.getDenseEdgeAttributeView('size_passthrough');
			const initView = initialDense.view;
			expect(initView[0]).toBeCloseTo(1);
			expect(initView[1]).toBeCloseTo(2);

			// Removing edge attribute clears passthrough; buffers should be unavailable
			net.removeEdgeAttribute('size_passthrough');
			expect(() => net.getEdgeAttributeBuffer('size_passthrough')).toThrow(/Unknown edge attribute/);

			// Recreate passthrough after removal
			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			sizeBuf[nodes[0]] = 5;
			sizeBuf[nodes[1]] = 6;
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const recreatedDense = net.getDenseEdgeAttributeView('size_passthrough');
			const recView = recreatedDense.view;
			expect(recView[0]).toBeCloseTo(5);
			expect(recView[1]).toBeCloseTo(6);

			// Removing the source node attribute breaks passthrough until redefined
			net.removeNodeAttribute('size');
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const afterNodeRemoval = net.getDenseEdgeAttributeView('size_passthrough');
			const afterNodeRemovalView = afterNodeRemoval.view;
			expect(afterNodeRemovalView[0]).toBeCloseTo(5);
			expect(afterNodeRemovalView[1]).toBeCloseTo(6);
			net.defineNodeAttribute('size', AttributeType.Float, 1);
			net.removeEdgeAttribute('size_passthrough');
			net.defineNodeToEdgeAttribute('size', 'size_passthrough', 'both');
			net.getNodeAttributeBuffer('size').view[nodes[0]] = 7;
			net.getNodeAttributeBuffer('size').view[nodes[1]] = 8;
			net.updateDenseEdgeAttributeBuffer('size_passthrough');
			const finalDense = net.getDenseEdgeAttributeView('size_passthrough');
			const finalView = finalDense.view;
			expect(finalView[0]).toBeCloseTo(7);
			expect(finalView[1]).toBeCloseTo(8);
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
			const weights = net.getNodeAttributeBuffer('weight').view;
			weights[nodes[0] * 2] = 3;
			weights[nodes[0] * 2 + 1] = 4;
			weights[nodes[1] * 2] = 5;
			weights[nodes[1] * 2 + 1] = 6;

			net.copyNodeAttributeToEdgeAttribute('weight', 'weight_edge', 'destination', false);
			const edgeWeights = net.getEdgeAttributeBuffer('weight_edge').view;
			expect(edgeWeights[0]).toBeCloseTo(5);
			expect(edgeWeights[1]).toBeCloseTo(6);

			net.defineEdgeAttribute('from_dup', AttributeType.Float, 4);
			net.copyNodeAttributeToEdgeAttribute('weight', 'from_dup', 'source', true);
			const fromDup = net.getEdgeAttributeBuffer('from_dup').view;
			expect(Array.from(fromDup.slice(0, 4))).toEqual([3, 4, 3, 4]);

			// Edge attribute already defined should block passthrough registration
			net.defineEdgeAttribute('existing_edge', AttributeType.Float, 2);
			expect(() => net.defineNodeToEdgeAttribute('weight', 'existing_edge', 'both')).toThrow(/already exists/);
		} finally {
			net.dispose();
		}
	});

	test('validates node-to-edge dense copy types and dimensions', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			net.addNodes(2);
			net.addEdges([{ from: 0, to: 1 }]);

			net.defineNodeAttribute('label', AttributeType.String, 1);
			net.defineEdgeAttribute('label_edge', AttributeType.Float, 2);
			expect(() => net.defineNodeToEdgeAttribute('label', 'label_edge')).toThrow(/numeric/);

			net.defineNodeAttribute('float_attr', AttributeType.Float, 1);
			net.defineEdgeAttribute('int_edge', AttributeType.Integer, 2);
			expect(() => net.defineNodeToEdgeAttribute('float_attr', 'int_edge')).toThrow(/already exists/);

			net.defineEdgeAttribute('too_small_edge', AttributeType.Float, 1);
			expect(() => net.defineNodeToEdgeAttribute('float_attr', 'too_small_edge')).toThrow(/already exists/);

			net.defineEdgeAttribute('weird_edge', AttributeType.Float, 3);
			expect(() => net.defineNodeToEdgeAttribute('float_attr', 'weird_edge', 'source')).toThrow(/already exists/);
		} finally {
			net.dispose();
		}
	});

	test('packs dense attribute and index buffers with versioning', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(4);
			net.defineNodeAttribute('weight', AttributeType.Float, 1);
			const weights = net.getNodeAttributeBuffer('weight').view;
			for (let i = 0; i < nodes.length; i++) {
				weights[nodes[i]] = (i + 1) * 10;
			}

			net.addDenseNodeAttributeBuffer('weight');
			const reverseOrder = Uint32Array.from([...nodes].reverse());
			net.setDenseNodeOrder(reverseOrder);
			net.updateDenseNodeAttributeBuffer('weight');
			let dense = net.getDenseNodeAttributeView('weight');
			expect(dense.count).toBe(reverseOrder.length);
			expect(dense.stride).toBe(Float32Array.BYTES_PER_ELEMENT);
			expect(dense.validStart).toBe(0);
			expect(dense.validEnd).toBe(nodes[nodes.length - 1] + 1);
			const denseFloats = dense.view;
			expect(Array.from(denseFloats.slice(0, reverseOrder.length))).toEqual([40, 30, 20, 10]);
			const firstDenseVersion = dense.version;

			weights[nodes[0]] = 99;
			net.bumpNodeAttributeVersion('weight');
			net.updateDenseNodeAttributeBuffer('weight');
			dense = net.getDenseNodeAttributeView('weight');
			const refreshedFloats = dense.view;
			expect(refreshedFloats[0]).toBeCloseTo(40);
			expect(refreshedFloats[reverseOrder.length - 1]).toBeCloseTo(99);
			expect(dense.version).toBeGreaterThan(firstDenseVersion);

			net.updateDenseNodeIndexBuffer();
			const indexDense = net.getDenseNodeIndexView();
			expect(indexDense.count).toBe(net.nodeCount);
			const indexView = indexDense.view;
			expect(Array.from(indexView.slice(0, net.nodeCount))).toEqual(expect.arrayContaining(Array.from(nodes)));
			expect(indexDense.topologyVersion).toBeGreaterThan(0);

			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[2], to: nodes[3] },
			]);
			net.defineEdgeAttribute('capacity', AttributeType.Float, 1);
			const capView = net.getEdgeAttributeBuffer('capacity').view;
			capView[edges[0]] = 1.5;
			capView[edges[1]] = 2.5;

			net.addDenseEdgeAttributeBuffer('capacity');
			net.updateDenseEdgeAttributeBuffer('capacity');
			const denseEdge = net.getDenseEdgeAttributeView('capacity');
			const denseEdgeFloats = denseEdge.view;
			expect(Array.from(denseEdgeFloats.slice(0, denseEdge.count))).toEqual(expect.arrayContaining([1.5, 2.5]));
			net.updateDenseEdgeIndexBuffer();
			const denseEdgeIndex = net.getDenseEdgeIndexView();
			expect(denseEdgeIndex.count).toBe(net.edgeCount);
		} finally {
			net.dispose();
		}
	});

	test('aliases dense attribute buffers to sparse slices when contiguous and unordered', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(4);
			net.defineNodeAttribute('weight', AttributeType.Float, 1);
			const weights = net.getNodeAttributeBuffer('weight').view;
			for (let i = 0; i < nodes.length; i++) {
				weights[nodes[i]] = i + 1;
			}

			net.addDenseNodeAttributeBuffer('weight');
			net.updateDenseNodeAttributeBuffer('weight');
			let dense = net.getDenseNodeAttributeView('weight');
			expect(dense.count).toBe(net.nodeCount);

			weights[nodes[0]] = 123;
			dense = net.getDenseNodeAttributeView('weight');
			expect(dense.view[0]).toBeCloseTo(123);

			net.removeNodes([nodes[1]]);
			net.updateDenseNodeAttributeBuffer('weight');
			dense = net.getDenseNodeAttributeView('weight');
			const packedBefore = dense.view[0];

			weights[nodes[0]] = 777;
			dense = net.getDenseNodeAttributeView('weight');
			expect(dense.view[0]).toBeCloseTo(packedBefore);
		} finally {
			net.dispose();
		}
	});

	test('virtualizes dense index buffers when contiguous and unordered', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(4);

			net.updateDenseNodeIndexBuffer();
			let dense = net.getDenseNodeIndexView();
			expect(Array.from(dense.view)).toEqual([0, 1, 2, 3]);

			net.removeNodes([nodes[0]]);
			net.updateDenseNodeIndexBuffer();
			dense = net.getDenseNodeIndexView();
			expect(Array.from(dense.view)).toEqual([1, 2, 3]);

			net.removeNodes([nodes[2]]);
			net.updateDenseNodeIndexBuffer();
			dense = net.getDenseNodeIndexView();
			expect(Array.from(dense.view)).toEqual([1, 3]);

			const fresh = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
			try {
				const freshNodes = fresh.addNodes(4);
				fresh.removeNodes([freshNodes[1]]);
				fresh.updateDenseNodeIndexBuffer();
				const fallbackDense = fresh.getDenseNodeIndexView();
				expect(Array.from(fallbackDense.view)).toEqual([0, 2, 3]);
			} finally {
				fresh.dispose();
			}
		} finally {
			net.dispose();
		}
	});

	test('builds color-encoded dense buffers for nodes and edges', async () => {
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		const decode32 = (view, logicalIndex) => {
			const base = logicalIndex * 4;
			return (
				view[base]
				| (view[base + 1] << 8)
				| (view[base + 2] << 16)
				| (view[base + 3] << 24)
			) >>> 0;
		};
		try {
			const nodes = net.addNodes(3);
			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[1], to: nodes[2] },
			]);

			net.defineNodeAttribute('node_id', AttributeType.UnsignedInteger, 1);
			net.defineEdgeAttribute('edge_tag', AttributeType.UnsignedInteger, 1);
			const nodeIds = net.getNodeAttributeBuffer('node_id').view;
			const edgeTags = net.getEdgeAttributeBuffer('edge_tag').view;
			nodeIds[nodes[0]] = 10;
			nodeIds[nodes[1]] = 20;
			nodeIds[nodes[2]] = 30;
			edgeTags[edges[0]] = 100;
			edgeTags[edges[1]] = 200;

			net.defineDenseColorEncodedNodeAttribute('node_id', 'node_color', { format: DenseColorEncodingFormat.Uint8x4 });
			net.defineDenseColorEncodedEdgeAttribute('edge_tag', 'edge_color', { format: DenseColorEncodingFormat.Uint8x4 });
			net.defineDenseColorEncodedNodeAttribute('$index', 'node_index_color', { format: DenseColorEncodingFormat.Uint8x4 });
			net.defineDenseColorEncodedEdgeAttribute('$index', 'edge_index_color', { format: DenseColorEncodingFormat.Uint8x4 });

			const nodeColor = net.updateDenseColorEncodedNodeAttribute('node_color');
			expect(nodeColor.count).toBe(nodes.length);
			expect(nodeColor.dimension).toBe(4);
			expect(nodeColor.view).toBeInstanceOf(Uint8Array);
			expect(decode32(nodeColor.view, 0)).toBe(11);
			expect(decode32(nodeColor.view, 1)).toBe(21);
			expect(decode32(nodeColor.view, 2)).toBe(31);

			const edgeColor = net.updateDenseColorEncodedEdgeAttribute('edge_color');
			expect(edgeColor.count).toBe(edges.length);
			expect(edgeColor.view).toBeInstanceOf(Uint8Array);
			expect(decode32(edgeColor.view, 0)).toBe(101);
			expect(decode32(edgeColor.view, 1)).toBe(201);

			nodeIds[nodes[1]] = 500;
			net.bumpNodeAttributeVersion('node_id');
			const updatedNodeColor = net.updateDenseColorEncodedNodeAttribute('node_color');
			expect(decode32(updatedNodeColor.view, 1)).toBe(501);

			const nodeOrder = Uint32Array.from(nodes).reverse();
			const edgeOrder = Uint32Array.from(edges).reverse();
			net.setDenseNodeOrder(nodeOrder);
			net.setDenseEdgeOrder(edgeOrder);

			const nodeIndexColor = net.updateDenseColorEncodedNodeAttribute('node_index_color');
			expect(Array.from({ length: nodeIndexColor.count }, (_, i) => decode32(nodeIndexColor.view, i))).toEqual(
				Array.from(nodeOrder, (id) => id + 1)
			);
			const edgeIndexColor = net.updateDenseColorEncodedEdgeAttribute('edge_index_color');
			expect(Array.from({ length: edgeIndexColor.count }, (_, i) => decode32(edgeIndexColor.view, i))).toEqual(
				Array.from(edgeOrder, (id) => id + 1)
			);
		} finally {
			net.dispose();
		}
	});

	test('buffer access guards block allocation-prone calls and expose typed dense views', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			net.defineNodeAttribute('size', AttributeType.Float, 1);
			const sizeBuf = net.getNodeAttributeBuffer('size').view;
			sizeBuf[nodes[0]] = 1.5;
			sizeBuf[nodes[1]] = 2.5;
			net.addDenseNodeAttributeBuffer('size');
			net.updateDenseNodeAttributeBuffer('size');
			net.updateDenseNodeIndexBuffer();

			const buffers = net.withDenseBufferViews([['node', 'size'], ['node', 'index']], (views) => {
				const sizeView = views.node.size.view;
				const indexView = views.node.index.view;
				expect(sizeView instanceof Float32Array).toBe(true);
				expect(indexView instanceof Uint32Array).toBe(true);
				expect(Array.from(sizeView.slice(0, 2))).toEqual([1.5, 2.5]);
				expect(Array.from(indexView.slice(0, 2))).toEqual(expect.arrayContaining(Array.from(nodes)));
				expect(() => net.updateDenseNodeAttributeBuffer('size')).toThrow(/buffer access/);
				expect(() => net.addNodes(1)).toThrow(/buffer access/);
				return views;
			});
			expect(buffers.node.size.pointer).toBeGreaterThan(0);
		} finally {
			net.dispose();
		}
	});

	test('batch update and fetch dense buffers returns typed views', async () => {
		const net = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(3);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }, { from: nodes[1], to: nodes[2] }]);
			net.defineNodeAttribute('pos', AttributeType.Float, 2);
			net.defineEdgeAttribute('weight', AttributeType.Float, 1);
			const pos = net.getNodeAttributeBuffer('pos').view;
			pos[nodes[0] * 2 + 0] = 1;
			pos[nodes[0] * 2 + 1] = 2;
			pos[nodes[1] * 2 + 0] = 3;
			pos[nodes[1] * 2 + 1] = 4;
			pos[nodes[2] * 2 + 0] = 5;
			pos[nodes[2] * 2 + 1] = 6;
			const weight = net.getEdgeAttributeBuffer('weight').view;
			weight[edges[0]] = 7;
			weight[edges[1]] = 8;
			net.addDenseNodeAttributeBuffer('pos');
			net.addDenseEdgeAttributeBuffer('weight');

			const views = net.updateAndGetDenseBufferViews([
				['node', 'pos'],
				['edge', 'weight'],
				['node', 'index'],
				['edge', 'index'],
			]);

			expect(views.node.pos.view instanceof Float32Array).toBe(true);
			expect(views.edge.weight.view instanceof Float32Array).toBe(true);
			expect(views.node.index.view instanceof Uint32Array).toBe(true);
			expect(views.edge.index.view instanceof Uint32Array).toBe(true);
			const densePos = views.node.pos.view;
			const denseNodeIds = views.node.index.view;
			for (let i = 0; i < denseNodeIds.length; i++) {
				const nodeId = denseNodeIds[i];
				const base = nodeId * 2;
				const denseBase = i * 2;
				expect(densePos[denseBase + 0]).toBeCloseTo(pos[base + 0]);
				expect(densePos[denseBase + 1]).toBeCloseTo(pos[base + 1]);
			}
			const denseEdgeWeights = views.edge.weight.view;
			const denseEdgeIds = views.edge.index.view;
			for (let i = 0; i < denseEdgeIds.length; i++) {
				const edgeId = denseEdgeIds[i];
				expect(denseEdgeWeights[i]).toBeCloseTo(weight[edgeId]);
			}
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
			const valView = net.getNodeAttributeBuffer('value').view;
			valView[nodes[0]] = 1;
			valView[nodes[1]] = 2;
			valView[nodes[2]] = 3;

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

			networkInstance.getNodeAttributeBuffer('score').view[nodes[0]] = 1.25;
			networkInstance.getNodeAttributeBuffer('score').view[nodes[1]] = 2.5;
			networkInstance.getNodeAttributeBuffer('score').view[nodes[2]] = 3.75;
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
				expect(restored.getNodeAttributeBuffer('score').view[1]).toBeCloseTo(2.5);
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

			networkInstance.getNodeAttributeBuffer('score').view[0] = 1.5;
			networkInstance.getNodeAttributeBuffer('hidden').view[0] = 99;
			networkInstance.setNodeStringAttribute('label', 0, 'keep?');
			networkInstance.getEdgeAttributeBuffer('weight').view[edges[0]] = 2.25;
			networkInstance.setEdgeStringAttribute('status', edges[0], 'ok');
			networkInstance.setNetworkStringAttribute('title', 'Filtered XNET');
			networkInstance.getNetworkAttributeBuffer('version').view[0] = 7;

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
				expect(restored.getNodeAttributeBuffer('score').view[0]).toBeCloseTo(1.5);
				expect(restored.getEdgeAttributeBuffer('weight').view[edges[0]]).toBeCloseTo(2.25);
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

			networkInstance.getNodeAttributeBuffer('score').view[0] = 2.5;
			networkInstance.getNodeAttributeBuffer('rank').view[0] = 4;
			networkInstance.getEdgeAttributeBuffer('weight').view[edges[0]] = 3.5;
			networkInstance.getEdgeAttributeBuffer('flag').view[edges[0]] = 1;
			networkInstance.getNetworkAttributeBuffer('version').view[0] = 42;

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
				expect(restored.getNodeAttributeBuffer('score').view[0]).toBeCloseTo(2.5);
				expect(restored.getEdgeAttributeBuffer('weight').view[edges[0]]).toBeCloseTo(3.5);
				expect(restored.getNetworkAttributeBuffer('version').view[0]).toBe(42);
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

			const meta = net.getNodeAttributeBuffer('jsMeta');
			meta.set(nodes[1], { name: 'one' });
			meta.set(nodes[3], { name: 'three' });

			net.setNodeStringAttribute('label', nodes[1], 'one');
			net.setNodeStringAttribute('label', nodes[3], 'three');

			const scores = net.getNodeAttributeBuffer('score').view;
			scores[nodes[1]] = 1.25;
			scores[nodes[3]] = 3.5;

			const edges = net.addEdges([
				{ from: nodes[1], to: nodes[3] },
				{ from: nodes[3], to: nodes[4] },
			]);
			net.getEdgeAttributeBuffer('flag').view[edges[0]] = 1;

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
		} finally {
			net.dispose();
		}
	});
});
