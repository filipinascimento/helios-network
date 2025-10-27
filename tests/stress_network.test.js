import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';

const BATCH_NODES = 2000;
const EDGE_REPETITIONS = 3;
const EDGE_BATCH_SIZE = BATCH_NODES * EDGE_REPETITIONS;

/**
 * Stress-oriented regression that exercises heavy allocation, attribute writes,
 * selector usage, and structural mutations. The goal is to detect regressions
 * that only surface during larger workloads (e.g. incorrect capacity growth or
 * stale views into WASM memory).
 */
test('handles batched high-volume network operations', async () => {
	const network = await HeliosNetwork.create({
		directed: true,
		initialNodes: 64,
		initialEdges: 0,
	});

	try {
		const newNodes = network.addNodes(BATCH_NODES);
		expect(newNodes.length).toBe(BATCH_NODES);
		expect(network.nodeCount).toBeGreaterThanOrEqual(BATCH_NODES);

		// Create a dense batch of edges; each node connects to the next few nodes.
		const edgePayload = [];
		for (let i = 0; i < BATCH_NODES; i += 1) {
			for (let j = 1; j <= EDGE_REPETITIONS; j += 1) {
				const to = (i + j) % BATCH_NODES;
				edgePayload.push({ from: newNodes[i], to: newNodes[to] });
			}
		}

		const createdEdges = network.addEdges(edgePayload);
		expect(createdEdges.length).toBe(EDGE_BATCH_SIZE);
		expect(network.edgeCount).toBeGreaterThanOrEqual(EDGE_BATCH_SIZE);

		// Attribute buffers should scale with node count.
		network.defineNodeAttribute('load', AttributeType.Float, 1);
		const loadBuffer = network.getNodeAttributeBuffer('load');
		expect(loadBuffer.view.length).toBeGreaterThanOrEqual(network.nodeCount);

		for (let i = 0; i < newNodes.length; i += 1) {
			loadBuffer.view[newNodes[i]] = Math.log1p(i);
		}

		// Validate a handful of computed values to ensure numeric stability.
		expect(loadBuffer.view[newNodes[0]]).toBeCloseTo(0);
		expect(loadBuffer.view[newNodes[1000]]).toBeGreaterThan(loadBuffer.view[newNodes[10]]);

		// Exercise selector population on a large subset.
		const sampledNodes = newNodes.filter((_, idx) => idx % 5 === 0);
		const selector = network.createNodeSelector(sampledNodes);
		expect(selector.count).toBe(sampledNodes.length);
		expect(Array.from(selector.toTypedArray())).toEqual(Array.from(sampledNodes));
		selector.dispose();

		// Remove a slice of edges and nodes to confirm capacity recycling remains functional.
		const edgesToRemove = Array.from(createdEdges).filter((_, idx) => idx % 7 === 0);
		network.removeEdges(edgesToRemove);
		expect(network.edgeCount).toBeGreaterThanOrEqual(EDGE_BATCH_SIZE - edgesToRemove.length);

		const nodesToRemove = sampledNodes.slice(0, 50);
		network.removeNodes(nodesToRemove);
		for (const nodeId of nodesToRemove) {
			expect(network.nodeActivityView[nodeId]).toBe(0);
		}

		// Query neighbours from a mid-stream node after removals to ensure adjacency is consistent.
		const midpoint = newNodes[Math.floor(newNodes.length / 2)];
		const { nodes, edges } = network.getOutNeighbors(midpoint);
		expect(nodes.length).toBeGreaterThan(0);
		expect(edges.length).toBe(nodes.length);
	} finally {
		network.dispose();
	}
}, 20000);
