// vite.config.js
import { execSync } from 'child_process'
import { promises as fs } from 'node:fs'
import path from 'node:path'
import { viteStaticCopy } from 'vite-plugin-static-copy'

const format = process.env.FORMAT ?? 'es';
const isUMD = format === 'umd';
const isInline = format === 'inline';

const debugSymbols = process.env.DEBUG_SYMBOLS === '1';

function compileEmscripten() {
	return {
		name: 'compile-emscripten',
		configResolved(config) {
			if (config.command === 'serve' || config.command === 'build') {
				// You can also pass MODE=debug/release to make here if you want
				const mode = debugSymbols ? 'debug' : 'release';
				execSync(`make compile MODE=${mode}`, { stdio: 'inherit' });
			}
		},
	}
}

function stableWorkerBundlePlugin() {
	const workerFileRe = /^HeliosSessionWorker\.browser-[A-Za-z0-9_-]+\.js$/;
	const stableWorkerRel = 'workers/HeliosSessionWorker.browser.js';
	const stableWorkerMapRel = 'workers/HeliosSessionWorker.browser.js.map';
	let outDir = 'dist';

	return {
		name: 'stable-worker-bundle',
		apply: 'build',
		configResolved(config) {
			outDir = config.build.outDir ?? 'dist';
		},
		async closeBundle() {
			// Only normalize the default ESM build.
			if (isUMD || isInline) return;

			const absOutDir = path.resolve(process.cwd(), outDir);
			const absAssetsDir = path.join(absOutDir, 'assets');
			const absWorkersDir = path.join(absOutDir, 'workers');
			const absMainEsm = path.join(absOutDir, 'helios-network.js');
			const absMainUmd = path.join(absOutDir, 'helios-network.umd.cjs');
			const absMainEsmMap = `${absMainEsm}.map`;
			const absMainUmdMap = `${absMainUmd}.map`;

			let assetNames;
			try {
				assetNames = await fs.readdir(absAssetsDir);
			} catch {
				return;
			}

			const workerAsset = assetNames.find((n) => workerFileRe.test(n));
			if (!workerAsset) return;

			await fs.mkdir(absWorkersDir, { recursive: true });

			const absWorkerAsset = path.join(absAssetsDir, workerAsset);
			const absWorkerAssetMap = path.join(absAssetsDir, `${workerAsset}.map`);
			const absStableWorker = path.join(absOutDir, stableWorkerRel);
			const absStableWorkerMap = path.join(absOutDir, stableWorkerMapRel);

			// Copy worker to a stable location/name.
			let workerCode = await fs.readFile(absWorkerAsset, 'utf8');
			// Normalize any existing sourcemap reference to the stable map filename.
			// Vite outputs `//# sourceMappingURL=...` (no extra `#`/`@`).
			workerCode = workerCode
				.replace(/\/\/[@#]\s*sourceMappingURL=.*$/gm, '')
				.replace(/\s+$/u, '');
			workerCode += `\n//# sourceMappingURL=HeliosSessionWorker.browser.js.map\n`;
			await fs.writeFile(absStableWorker, workerCode, 'utf8');

			// Copy (and normalize) worker sourcemap if present.
			try {
				const mapText = await fs.readFile(absWorkerAssetMap, 'utf8');
				try {
					const parsed = JSON.parse(mapText);
					parsed.file = 'HeliosSessionWorker.browser.js';
					await fs.writeFile(absStableWorkerMap, JSON.stringify(parsed), 'utf8');
				} catch {
					await fs.writeFile(absStableWorkerMap, mapText, 'utf8');
				}
			} catch {
				// no map
			}

			const patchBundleFile = async (filePath) => {
				let code;
				try {
					code = await fs.readFile(filePath, 'utf8');
				} catch {
					return;
				}
				const escapedWorker = workerAsset.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
				code = code
					.replace(new RegExp(`"/assets/${escapedWorker}"`, 'g'), `"./${stableWorkerRel}"`)
					.replace(new RegExp(`'/assets/${escapedWorker}'`, 'g'), `'./${stableWorkerRel}'`);
				await fs.writeFile(filePath, code, 'utf8');
			};

			await patchBundleFile(absMainEsm);
			await patchBundleFile(absMainUmd);
			await patchBundleFile(absMainEsmMap);
			await patchBundleFile(absMainUmdMap);

			// Keep the hashed worker outputs in dist/assets.
			// Some tooling (including Vite dev logs) will attempt to load the original
			// sourcemap for any discovered worker chunks; deleting the `*.map` can
			// cause noisy ENOENT warnings even though runtime is fine.
		},
	};
}

export default {
	publicDir: 'docs',
	appType: 'mpa',
	server: {
		fs: {
			allow: ['.', 'docs'],
		},
	},
	plugins: [
    viteStaticCopy({
      targets: [
        {
          src: 'compiled/*',
          dest: 'compiled'
        },
      ]
    }),
		stableWorkerBundlePlugin(),
		// compileEmscripten()
	],
	build: {
		target: "esnext",
		sourcemap: true,
		lib: {
			entry: isInline ? 'src/helios-network-inline.js' : 'src/helios-network.js',
			name: isInline ? 'helios-network-inline' : 'helios-network',
			// the proper extensions will be added
			fileName: isInline ? 'helios-network.inline' : 'helios-network',
			formats: isUMD ? ['umd'] : ['es'],
		},
		minify: debugSymbols ? false : "esbuild",
		rollupOptions: {
			external: ['module', 'node:module'],
		},
		assetsInlineLimit: 0,
	},
	define: {
		isUMD: isUMD,
	},
	// server: {
    // open: '/tests/browser-test.html',
  // },
	// resolve: {
  //   alias: {
  //     'CXNetwork': './compiled/CXNetwork.mjs',
  //   },
  // },


}
