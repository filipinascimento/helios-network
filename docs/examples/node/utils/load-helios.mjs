const DIST_ENTRY = '../../../../dist/helios-network.js';
const SRC_ENTRY = '../../../../src/helios-network.js';

/**
 * Loads the Helios module. During development, the compiled bundle under
 * `dist/` might be missing, so we fall back to the source entry.
 *
 * @param {object} [options]
 * @param {boolean} [options.preferSource=false] Force reading from the source entry even if the bundle exists.
 */
export async function loadHelios(options = {}) {
	const { preferSource = false } = options;

	if (preferSource) {
		return import(SRC_ENTRY);
	}

	try {
		return await import(DIST_ENTRY);
	} catch (error) {
		console.warn(
			'[examples] Falling back to src/helios-network.js – run `npm run build` to refresh dist/ before shipping.',
			error?.message ?? error,
		);
		return import(SRC_ENTRY);
	}
}
