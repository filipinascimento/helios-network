import defaultFactory from '../../compiled/CXNetwork.mjs';
import { getInlineWasmBinary } from './inline/wasm-inline.js';

let activeFactory = defaultFactory;
let cachedInlineBinary = null;

function ensureInlineBinary() {
	if (!cachedInlineBinary) {
		cachedInlineBinary = getInlineWasmBinary();
	}
	return cachedInlineBinary;
}

function isNodeRuntime() {
	try {
		return typeof process !== 'undefined' && !!process.versions?.node && process.type !== 'renderer';
	} catch {
		return false;
	}
}

/**
 * Replaces the active WASM module factory used by Helios.
 *
 * @param {Function} factory - New factory that returns a promise resolving to the WASM module.
 */
export function setHeliosModuleFactory(factory) {
	activeFactory = typeof factory === 'function' ? factory : defaultFactory;
}

/**
 * Returns the factory currently used to instantiate the WASM module.
 *
 * @returns {Function} Active factory function.
 */
export function getHeliosModuleFactory() {
	return activeFactory;
}

/**
 * Provides the original factory exported by Emscripten before any overrides.
 *
 * @returns {Function} Default factory wired to the real WASM artefacts.
 */
export function getDefaultHeliosModuleFactory() {
	return defaultFactory;
}

/**
 * Creates a new Helios WASM module using the active factory.
 *
 * @param {object} [options] - Options forwarded to the Emscripten loader.
 * @returns {Promise<object>} Resolves with the initialized module.
 */
export default function createHeliosModule(options) {
	if (activeFactory === defaultFactory) {
		if (isNodeRuntime() && !options?.wasmBinary) {
			return defaultFactory(options);
		}
		const wasmBinary = options?.wasmBinary ?? ensureInlineBinary();
		return defaultFactory({ ...options, wasmBinary });
	}
	return activeFactory(options);
}
