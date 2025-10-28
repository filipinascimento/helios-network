// vite.config.js
import { execSync } from 'child_process'
import { viteStaticCopy } from 'vite-plugin-static-copy'

const isUMD = process.env.FORMAT === 'umd';

function compileEmscripten() {
	return {
		name: 'compile-emscripten',
		configResolved(config) {
			if (config.command === 'serve' || config.command === 'build') {
				execSync('make compile', { stdio: 'inherit' });
			}
		},
	}
}

export default {
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
			entry: 'src/helios-network.js',
			name: 'helios-network',
			// the proper extensions will be added
			fileName: 'helios-network',
			formats: isUMD ? ['umd'] : ['es'],
		},
		minify: "esbuild",
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
