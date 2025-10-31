const DIST_ENTRY = '../../../../dist/helios-network.js';
const SRC_ENTRY = '../../../../src/helios-network.js';

/**
 * Load the Helios module, falling back to the source entry when the built
 * bundle is unavailable (e.g. during `npm run dev`).
 */
export async function loadHelios() {
	try {
		return await import(DIST_ENTRY);
	} catch (error) {
		console.warn(
			'[examples] Falling back to src/helios-network.js – build dist/ with `npm run build` to use the bundled artefact.',
			error?.message ?? error,
		);
		return import(SRC_ENTRY);
	}
}
