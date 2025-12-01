import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { beforeAll, afterAll, describe, expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';

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
		expect(network.nodeActivityView[createdNodes[3]]).toBe(0);
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
		} finally {
			net.dispose();
		}
	});

	test('packs dense attribute and index buffers with dirty tracking', async () => {
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
			let dense = net.updateDenseNodeAttributeBuffer('weight', reverseOrder);
			expect(dense.count).toBe(reverseOrder.length);
			expect(dense.stride).toBe(Float32Array.BYTES_PER_ELEMENT);
			expect(dense.validStart).toBe(0);
			expect(dense.validEnd).toBe(nodes[nodes.length - 1] + 1);
			const denseFloats = new Float32Array(dense.view.buffer, dense.pointer, dense.view.byteLength / Float32Array.BYTES_PER_ELEMENT);
			expect(Array.from(denseFloats.slice(0, reverseOrder.length))).toEqual([40, 30, 20, 10]);

			weights[nodes[0]] = 99;
			net.markDenseNodeAttributeDirty('weight');
			dense = net.updateDenseNodeAttributeBuffer('weight', reverseOrder);
			const refreshedFloats = new Float32Array(dense.view.buffer, dense.pointer, dense.view.byteLength / Float32Array.BYTES_PER_ELEMENT);
			expect(refreshedFloats[0]).toBeCloseTo(40);
			expect(refreshedFloats[reverseOrder.length - 1]).toBeCloseTo(99);

			const indexDense = net.updateDenseNodeIndexBuffer();
			expect(indexDense.count).toBe(net.nodeCount);
			const indexView = new Uint32Array(indexDense.view.buffer, indexDense.pointer, indexDense.count);
			expect(Array.from(indexView.slice(0, net.nodeCount))).toEqual(expect.arrayContaining(Array.from(nodes)));

			const edges = net.addEdges([
				{ from: nodes[0], to: nodes[1] },
				{ from: nodes[2], to: nodes[3] },
			]);
			net.defineEdgeAttribute('capacity', AttributeType.Float, 1);
			const capView = net.getEdgeAttributeBuffer('capacity').view;
			capView[edges[0]] = 1.5;
			capView[edges[1]] = 2.5;

			net.addDenseEdgeAttributeBuffer('capacity');
			const denseEdge = net.updateDenseEdgeAttributeBuffer('capacity');
			const denseEdgeFloats = new Float32Array(denseEdge.view.buffer, denseEdge.pointer, denseEdge.view.byteLength / Float32Array.BYTES_PER_ELEMENT);
			expect(Array.from(denseEdgeFloats.slice(0, denseEdge.count))).toEqual(expect.arrayContaining([1.5, 2.5]));
			const denseEdgeIndex = net.updateDenseEdgeIndexBuffer();
			expect(denseEdgeIndex.count).toBe(net.edgeCount);
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
