// playwright.config.js
// Basic Playwright config for running browser tests on Vite dev server
import { defineConfig } from '@playwright/test';

export default defineConfig({
  webServer: {
    command: 'vite',
    url: 'http://localhost:5173',
    reuseExistingServer: !process.env.CI,
  },
  use: {
    headless: true, // <--- Force headless mode
  },
  testDir: './tests',
  testMatch: /browser\.spec\.js/,
  timeout: 30000,
});
