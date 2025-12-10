// vite.config.js
import { execSync } from 'child_process'
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
