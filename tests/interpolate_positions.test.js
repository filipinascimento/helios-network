import { expect, test } from 'vitest';
import HeliosNetwork, { AttributeType } from '../src/helios-network.js';
import { withNodeBuffer } from './helpers/bufferAccess.js';

test('interpolates node positions using the WASM helper', async () => {
	const network = await HeliosNetwork.create({ directed: false, initialNodes: 2 });
	network.defineNodeAttribute('position', AttributeType.Float, 3);
	withNodeBuffer(network, 'position', ({ view: positions }) => {
		positions.set([0, 0, 0, 10, 0, 0]);
	});

	const target = new Float32Array([10, 0, 0, 20, 0, 0]);
	const step = network.interpolateNodeAttribute('position', target, {
		elapsedMs: 16,
		layoutElapsedMs: 32,
		smoothing: 6,
		minDisplacementRatio: 0,
	});
	expect(step).toBe(true);
	withNodeBuffer(network, 'position', ({ view: positions }) => {
		expect(positions[0]).toBeGreaterThan(0);
		expect(positions[0]).toBeLessThan(10);
	});

	for (let i = 0; i < 60; i += 1) {
		network.interpolateNodeAttribute('position', target, {
			elapsedMs: 16,
			layoutElapsedMs: 32,
			smoothing: 6,
			minDisplacementRatio: 0,
		});
	}
	withNodeBuffer(network, 'position', ({ view: positions }) => {
		expect(positions[0]).toBeCloseTo(10, 3);
	});
	network.dispose();
});
