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
});
