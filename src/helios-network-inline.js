import HeliosNetwork, {
	AttributeType,
	getHeliosModule,
	NodeSelector,
	EdgeSelector,
} from './js/HeliosNetwork.js';
import {
	setHeliosModuleFactory,
	getDefaultHeliosModuleFactory,
} from './js/moduleFactory.js';
import { getInlineWasmBinary } from './js/inline/wasm-inline.js';

const defaultFactory = getDefaultHeliosModuleFactory();
let cachedBinary = null;

function ensureInlineBinary() {
	if (!cachedBinary) {
		cachedBinary = getInlineWasmBinary();
	}
	return cachedBinary;
}

const isNode = typeof process !== 'undefined' && !!process.versions?.node && process.type !== 'renderer';

setHeliosModuleFactory((options = {}) => {
	if (isNode && !options.wasmBinary) {
		return defaultFactory(options);
	}
	const wasmBinary = options.wasmBinary ?? ensureInlineBinary();
	return defaultFactory({ ...options, wasmBinary });
});

export { AttributeType, NodeSelector, EdgeSelector, getHeliosModule };
export default HeliosNetwork;
