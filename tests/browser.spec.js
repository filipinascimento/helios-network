// tests/browser.spec.js
import { test, expect } from '@playwright/test';

test('browser test page loads and works', async ({ page }) => {
  await page.goto('http://localhost:5173/test/browser-test.html');
  await expect(page.locator('h1')).toHaveText('Helios Network Browser Test');
  await expect(page.locator('#output')).not.toHaveText('');
});
