import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';

test('can create network and add nodes/edges', async () => {
	const network = await HeliosNetwork.create({ directed: true, initialNodes: 2 });
	expect(network.nodeCount).toBe(2);

	const newNodes = network.addNodes(3);
	expect(newNodes.length).toBe(3);
	expect(network.nodeCount).toBe(5);

	const edges = network.addEdges([
		{ from: newNodes[0], to: newNodes[1] },
		{ from: newNodes[1], to: newNodes[2] },
	]);
	expect(edges.length).toBe(2);
	expect(network.edgeCount).toBe(2);

	network.defineNodeAttribute('weight', AttributeType.Float, 1);
	const weightBuffer = network.getNodeAttributeBuffer('weight');
	weightBuffer.view[ newNodes[0] ] = 42.5;
	expect(weightBuffer.view[newNodes[0]]).toBeCloseTo(42.5);

	network.dispose();
});

test('can select nodes and edges with query expressions', async () => {
	const network = await HeliosNetwork.create({ directed: false });
	const nodes = network.addNodes(3);

	network.defineNodeAttribute('score', AttributeType.Float, 1);
	const scoreBuffer = network.getNodeAttributeBuffer('score');
	scoreBuffer.view[nodes[0]] = 0.5;
	scoreBuffer.view[nodes[1]] = 2.0;
	scoreBuffer.view[nodes[2]] = 3.5;

	const selectedNodes = network.selectNodes('score > 1.0');
	expect(Array.from(selectedNodes)).toEqual([nodes[1], nodes[2]]);

	network.defineNodeAttribute('flag', AttributeType.Integer, 1);
	const flagBuffer = network.getNodeAttributeBuffer('flag');
	flagBuffer.view[nodes[0]] = 1;
	flagBuffer.view[nodes[1]] = 0;
	flagBuffer.view[nodes[2]] = 1;

	const edges = network.addEdges([
		{ from: nodes[0], to: nodes[1] },
		{ from: nodes[1], to: nodes[2] },
	]);

	const selectedEdges = network.selectEdges('$src.flag == 1');
	expect(Array.from(selectedEdges)).toEqual([edges[0]]);

	network.defineNodeAttribute('label', AttributeType.String, 1);
	network.setNodeStringAttribute('label', nodes[0], 'alpha');
	network.setNodeStringAttribute('label', nodes[1], 'beta');
	network.setNodeStringAttribute('label', nodes[2], 'gamma');

	const inNodes = network.selectNodes('label IN (\"alpha\", \"gamma\")');
	expect(Array.from(inNodes)).toEqual([nodes[0], nodes[2]]);

	const regexNodes = network.selectNodes('label =~ \"^g\"');
	expect(Array.from(regexNodes)).toEqual([nodes[2]]);

	network.defineNodeAttribute('vec2', AttributeType.Float, 2);
	const vecBuffer = network.getNodeAttributeBuffer('vec2');
	const dim = 2;
	vecBuffer.view[nodes[0] * dim] = 0.2;
	vecBuffer.view[nodes[0] * dim + 1] = 0.4;
	vecBuffer.view[nodes[1] * dim] = 1.5;
	vecBuffer.view[nodes[1] * dim + 1] = 0.1;
	vecBuffer.view[nodes[2] * dim] = 0.3;
	vecBuffer.view[nodes[2] * dim + 1] = 2.2;

	const vectorNodes = network.selectNodes('vec2 > 2.0');
	expect(Array.from(vectorNodes)).toEqual([nodes[2]]);

	const vectorMax = network.selectNodes('vec2.max > 2.0');
	expect(Array.from(vectorMax)).toEqual([nodes[2]]);

	const vectorIndex = network.selectNodes('vec2[0] > 1.0');
	expect(Array.from(vectorIndex)).toEqual([nodes[1]]);

	network.defineNodeAttribute('vec2b', AttributeType.Float, 2);
	const vecB = network.getNodeAttributeBuffer('vec2b');
	vecB.view[nodes[0] * dim] = 0.1;
	vecB.view[nodes[0] * dim + 1] = 0.2;
	vecB.view[nodes[1] * dim] = 1.0;
	vecB.view[nodes[1] * dim + 1] = 1.0;
	vecB.view[nodes[2] * dim] = 0.1;
	vecB.view[nodes[2] * dim + 1] = 0.1;

	const vectorDot = network.selectNodes('vec2.dot(vec2b) > 1.0');
	expect(Array.from(vectorDot)).toEqual([nodes[1]]);

	const vectorAny = network.selectNodes('vec2.any > 2.0');
	expect(Array.from(vectorAny)).toEqual([nodes[2]]);

	const vectorAll = network.selectNodes('vec2.all > 0.11');
	expect(Array.from(vectorAll)).toEqual([nodes[0], nodes[2]]);

	const vectorDotConst = network.selectNodes('vec2.dot([1, 1]) > 2.0');
	expect(Array.from(vectorDotConst)).toEqual([nodes[2]]);

	network.dispose();
});

test('can apply text batch with relative ids', async () => {
	const network = await HeliosNetwork.create({ directed: false });
	network.defineNodeAttribute('weight', AttributeType.Float, 1);
	network.defineNodeAttribute('label', AttributeType.String, 1);

	const batch = `
newIDs = ADD_NODES n=4
ADD_EDGES pairs=[(0,1),(1,2),(2,3)] ! relative newIDs
SET_ATTR_VALUES scope=node name=weight ids=[0,2] values=[0.5,2.0] ! relative newIDs
SET_ATTR_VALUES scope=node name=label ids=[1,3] values=["a","b"] ! relative newIDs
`;

	const { variables, results } = network.applyTextBatch(batch);
	expect(results.every((entry) => entry.ok)).toBe(true);
	expect(variables.newIDs.length).toBe(4);
	expect(network.edgeCount).toBe(3);

	network.dispose();
});

test('can apply binary batch with result slots', async () => {
	const network = await HeliosNetwork.create({ directed: false });
	network.defineNodeAttribute('weight', AttributeType.Float, 1);

	const encoder = new TextEncoder();
	const nameBytes = encoder.encode('weight');

	const records = [];

	// ADD_NODES n=3, resultSlot=1
	{
		const payload = new ArrayBuffer(4);
		new DataView(payload).setUint32(0, 3, true);
		records.push({ op: 1, flags: 0, slot: 1, payload: new Uint8Array(payload) });
	}

	// ADD_EDGES pairs=[(0,1),(1,2)] relative slot=1, resultSlot=2
	{
		const payload = new ArrayBuffer(4 + 4 + 16);
		const view = new DataView(payload);
		let off = 0;
		view.setUint32(off, 2, true); off += 4;
		view.setUint32(off, 1, true); off += 4; // baseSlot
		view.setUint32(off, 0, true); off += 4;
		view.setUint32(off, 1, true); off += 4;
		view.setUint32(off, 1, true); off += 4;
		view.setUint32(off, 2, true);
		records.push({ op: 2, flags: 1, slot: 2, payload: new Uint8Array(payload) });
	}

	// SET_ATTR_VALUES scope=node name=weight ids=[0,2] values=[0.5,2.0] relative slot=1
	{
		const payloadLen = 1 + 1 + 2 + 4 + 4 + 4 + 4 + nameBytes.length + 8 + 16;
		const payload = new ArrayBuffer(payloadLen);
		const view = new DataView(payload);
		let off = 0;
		view.setUint8(off, 0); off += 1; // scope node
		view.setUint8(off, 0); off += 1; // valueType f64
		view.setUint16(off, 0, true); off += 2;
		view.setUint32(off, 2, true); off += 4; // idCount
		view.setUint32(off, 2, true); off += 4; // valueCount
		view.setUint32(off, 1, true); off += 4; // baseSlot
		view.setUint32(off, nameBytes.length, true); off += 4;
		new Uint8Array(payload, off, nameBytes.length).set(nameBytes); off += nameBytes.length;
		view.setUint32(off, 0, true); off += 4;
		view.setUint32(off, 2, true); off += 4;
		view.setFloat64(off, 0.5, true); off += 8;
		view.setFloat64(off, 2.0, true); off += 8;
		records.push({ op: 3, flags: 1, slot: 0, payload: new Uint8Array(payload) });
	}

	const headerLen = 4 + 1 + 1 + 2 + 4;
	const totalLen = records.reduce((sum, rec) => sum + 1 + 1 + 2 + 4 + 4 + rec.payload.length, headerLen);
	const buffer = new ArrayBuffer(totalLen);
	const bytes = new Uint8Array(buffer);
	const view = new DataView(buffer);
	let offset = 0;
	bytes.set([72, 78, 80, 66], offset); offset += 4; // HNPB
	view.setUint8(offset++, 1); // version
	view.setUint8(offset++, 0); // flags
	view.setUint16(offset, 0, true); offset += 2;
	view.setUint32(offset, records.length, true); offset += 4;

	for (const rec of records) {
		view.setUint8(offset++, rec.op);
		view.setUint8(offset++, rec.flags);
		view.setUint16(offset, 0, true); offset += 2;
		view.setUint32(offset, rec.slot, true); offset += 4;
		view.setUint32(offset, rec.payload.length, true); offset += 4;
		bytes.set(rec.payload, offset); offset += rec.payload.length;
	}

	const result = network.applyBinaryBatch(buffer);
	expect(result.results.every((entry) => entry.ok)).toBe(true);
	expect(network.nodeCount).toBe(3);
	expect(network.edgeCount).toBe(2);

	network.dispose();
});
