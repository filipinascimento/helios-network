import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';

test('HeliosNetwork emits topology events and supports on()/unsubscribe()', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	try {
		let calls = 0;
		let lastDetail;
		const unsubscribe = network.on(HeliosNetwork.EVENTS.nodesAdded, (event) => {
			calls += 1;
			lastDetail = event.detail;
		});

		network.addNodes(1);
		expect(calls).toBe(1);
		expect(lastDetail).toMatchObject({
			count: 1,
			oldNodeCount: 0,
			nodeCount: 1,
		});

		unsubscribe();
		network.addNodes(1);
		expect(calls).toBe(1);
	} finally {
		network.dispose();
	}
});

test('HeliosNetwork on() supports AbortSignal auto-unsubscribe', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	try {
		const controller = new AbortController();
		let calls = 0;
		network.on(
			HeliosNetwork.EVENTS.nodesAdded,
			() => {
				calls += 1;
			},
			{ signal: controller.signal }
		);

		network.addNodes(1);
		expect(calls).toBe(1);

		controller.abort();
		network.addNodes(1);
		expect(calls).toBe(1);
	} finally {
		network.dispose();
	}
});

test('HeliosNetwork supports onAny() and listen(type.namespace) replacement', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	try {
		const seenTypes = new Set();
		network.onAny(({ type }) => {
			seenTypes.add(type);
		});

		network.addNodes(1);
		expect(seenTypes.has(HeliosNetwork.EVENTS.nodesAdded)).toBe(true);
		expect(seenTypes.has(HeliosNetwork.EVENTS.topologyChanged)).toBe(true);

		let aCalls = 0;
		let bCalls = 0;
		network.listen(`${HeliosNetwork.EVENTS.nodesAdded}.panel`, () => {
			aCalls += 1;
		});
		network.listen(`${HeliosNetwork.EVENTS.nodesAdded}.panel`, () => {
			bCalls += 1;
		});

		network.addNodes(1);
		expect(aCalls).toBe(0);
		expect(bCalls).toBe(1);

		network.listen(`${HeliosNetwork.EVENTS.nodesAdded}.panel`, null);
		network.addNodes(1);
		expect(bCalls).toBe(1);
	} finally {
		network.dispose();
	}
});

test('HeliosNetwork emits attribute events for define/set/remove', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 0, initialEdges: 0 });
	try {
		let defined;
		const definedOff = network.on(HeliosNetwork.EVENTS.attributeDefined, (event) => {
			defined = event.detail;
		});

		network.defineNodeAttribute('label', AttributeType.String, 1);
		definedOff();
		expect(defined).toMatchObject({
			scope: 'node',
			name: 'label',
			type: AttributeType.String,
			dimension: 1,
		});

		const nodes = network.addNodes(1);
		let changed;
		network.on(HeliosNetwork.EVENTS.attributeChanged, (event) => {
			if (event.detail?.scope === 'node' && event.detail?.name === 'label') {
				changed = event.detail;
			}
		});
		network.setNodeStringAttribute('label', nodes[0], 'alpha');
		expect(changed).toMatchObject({
			scope: 'node',
			name: 'label',
			op: 'set',
			index: nodes[0],
		});

		let removed;
		network.on(HeliosNetwork.EVENTS.attributeRemoved, (event) => {
			removed = event.detail;
		});
		network.removeNodeAttribute('label');
		expect(removed).toMatchObject({ scope: 'node', name: 'label' });
	} finally {
		network.dispose();
	}
});
