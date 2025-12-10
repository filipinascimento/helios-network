// tests/browser.spec.js
import { test, expect } from '@playwright/test';

const BASE_URL = 'http://localhost:5173';

const browserExamples = [
	{ slug: 'basic-usage', heading: /Browser Basic Usage/i },
	{ slug: 'attributes', heading: /Attribute Playground/i },
	{ slug: 'iteration', heading: /Iteration Demo/i },
	{ slug: 'modifying-graphs', heading: /Graph Mutations/i },
	{ slug: 'saving-loading', heading: /Saving & Loading/i },
];

for (const { slug, heading } of browserExamples) {
	test(`browser example \'${slug}\' renders output`, async ({ page }) => {
		await page.goto(`${BASE_URL}/examples/browser/${slug}/index.html`);
		await expect(page.locator('h1')).toContainText(heading);
		await expect(page.locator('#output')).toContainText(/Network disposed/i);
	});
}

test('browser attribute API exposes names, info, and presence', async ({ page }) => {
	await page.goto(`${BASE_URL}/examples/browser/attributes/index.html`);

	const result = await page.evaluate(async () => {
		const { loadHelios } = await import('/examples/browser/utils/load-helios.js');
		const { default: HeliosNetwork, AttributeType } = await loadHelios();
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			net.defineNodeAttribute('mass', AttributeType.Double, 2);
			net.defineEdgeAttribute('flag', AttributeType.Boolean, 1);
			net.defineNetworkAttribute('version', AttributeType.UnsignedInteger, 1);

			const nodeNames = net.getNodeAttributeNames();
			const edgeNames = net.getEdgeAttributeNames();
			const networkNames = net.getNetworkAttributeNames();

			const nodeInfo = net.getNodeAttributeInfo('mass');
			const edgeInfo = net.getEdgeAttributeInfo('flag');
			const networkInfo = net.getNetworkAttributeInfo('version');

			const selector = net.createNodeSelector();

			return {
				nodeNames,
				edgeNames,
				networkNames,
				nodeInfo,
				edgeInfo,
				networkInfo,
				selectorHasMass: selector.hasAttribute('mass'),
				netHasMissing: net.hasNodeAttribute('missing'),
			};
		} finally {
			net.dispose();
		}
	});

	expect(result.nodeNames).toEqual(expect.arrayContaining(['mass']));
	expect(result.edgeNames).toEqual(expect.arrayContaining(['flag']));
	expect(result.networkNames).toEqual(expect.arrayContaining(['version']));

	expect(result.nodeInfo).toEqual({ type: 5, dimension: 2, complex: false });
	expect(result.edgeInfo).toEqual({ type: 1, dimension: 1, complex: false });
	expect(result.networkInfo).toEqual({ type: 4, dimension: 1, complex: false });

	expect(result.selectorHasMass).toBe(true);
	expect(result.netHasMissing).toBe(false);
});

test('browser round-trips BXNet serialization', async ({ page }) => {
	await page.goto(`${BASE_URL}/examples/browser/basic-usage/index.html`);

	const result = await page.evaluate(async () => {
		const { loadHelios } = await import('/examples/browser/utils/load-helios.js');
		const { default: HeliosNetwork, AttributeType } = await loadHelios();
		const net = await HeliosNetwork.create({ directed: true, initialNodes: 0, initialEdges: 0 });
		try {
			const nodes = net.addNodes(2);
			const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);
			net.defineNodeAttribute('mass', AttributeType.Float, 1);
			net.defineEdgeAttribute('capacity', AttributeType.Double, 1);
			net.defineNetworkAttribute('title', AttributeType.String, 1);

			net.getNodeAttributeBuffer('mass').view[nodes[0]] = 3.25;
			net.getEdgeAttributeBuffer('capacity').view[edges[0]] = 7.5;
			net.setNetworkStringAttribute('title', 'browser-bxnet');

			const payload = await net.saveBXNet();
			const restored = await HeliosNetwork.fromBXNet(payload);
			try {
				return {
					directed: restored.directed,
					nodeCount: restored.nodeCount,
					edgeCount: restored.edgeCount,
					mass0: restored.getNodeAttributeBuffer('mass').view[0],
					capacity0: restored.getEdgeAttributeBuffer('capacity').view[0],
					title: restored.getNetworkStringAttribute('title'),
				};
			} finally {
				restored.dispose();
			}
		} finally {
			net.dispose();
		}
	});

	expect(result.directed).toBe(true);
	expect(result.nodeCount).toBe(2);
	expect(result.edgeCount).toBe(1);
	expect(result.mass0).toBeCloseTo(3.25);
	expect(result.capacity0).toBeCloseTo(7.5);
	expect(result.title).toBe('browser-bxnet');
});
