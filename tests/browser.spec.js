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
		await page.goto(`${BASE_URL}/docs/examples/browser/${slug}/`);
		await expect(page.locator('h1')).toContainText(heading);
		await expect(page.locator('#output')).toContainText(/Network disposed/i);
	});
}
