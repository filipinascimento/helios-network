import createHeliosModule from './moduleFactory.js';
import { WasmSteppableSession } from './sessions/WasmSteppableSession.js';
import { PACKAGE_VERSION } from './version.js';

/**
 * Enumeration of attribute types supported by the WASM network core.
 * Values align with the constants defined in the native implementation.
 * @enum {number}
 */
const AttributeType = Object.freeze({
	String: 0,
	Boolean: 1,
	Float: 2,
	Integer: 3,
	UnsignedInteger: 4,
	Double: 5,
	Category: 6,
	Data: 7,
	Javascript: 8,
	BigInteger: 9,
	UnsignedBigInteger: 10,
	MultiCategory: 11,
	Unknown: 255,
});

const CategorySortOrder = Object.freeze({
	None: 0,
	Frequency: 1,
	Alphabetical: 2,
	Natural: 3,
});

const DimensionDifferenceMethod = Object.freeze({
	Forward: 0,
	Backward: 1,
	Central: 2,
	LeastSquares: 3,
});

const NeighborDirection = Object.freeze({
	Out: 0,
	In: 1,
	Both: 2,
});

const StrengthMeasure = Object.freeze({
	Sum: 0,
	Average: 1,
	Maximum: 2,
	Minimum: 3,
});

const ClusteringCoefficientVariant = Object.freeze({
	Unweighted: 0,
	Onnela: 1,
	Newman: 2,
});

const MeasurementExecutionMode = Object.freeze({
	Auto: 0,
	SingleThread: 1,
	Parallel: 2,
});

const ConnectedComponentsMode = Object.freeze({
	Weak: 0,
	Strong: 1,
});

const DIMENSION_FORWARD_MAX_ORDER = 6;
const DIMENSION_BACKWARD_MAX_ORDER = 6;
const DIMENSION_CENTRAL_MAX_ORDER = 4;

const DIMENSION_CENTRAL_COEFFICIENTS = Object.freeze([
	Object.freeze([0.5, 0, 0, 0]),
	Object.freeze([2 / 3, -1 / 12, 0, 0]),
	Object.freeze([3 / 4, -3 / 20, 1 / 60, 0]),
	Object.freeze([4 / 5, -1 / 5, 4 / 105, -1 / 280]),
]);

const DIMENSION_FORWARD_COEFFICIENTS = Object.freeze([
	Object.freeze([-1, 1, 0, 0, 0, 0, 0]),
	Object.freeze([-3 / 2, 2, -1 / 2, 0, 0, 0, 0]),
	Object.freeze([-11 / 6, 3, -3 / 2, 1 / 3, 0, 0, 0]),
	Object.freeze([-25 / 12, 4, -3, 4 / 3, -1 / 4, 0, 0]),
	Object.freeze([-137 / 60, 5, -5, 10 / 3, -5 / 4, 1 / 5, 0]),
	Object.freeze([-49 / 20, 6, -15 / 2, 20 / 3, -15 / 4, 6 / 5, -1 / 6]),
]);

/**
 * Options for categorizing string attributes.
 * @typedef {object} CategorizeOptions
 * @property {(CategorySortOrder|string)=} sortOrder - Ordering of categories.
 * @property {string=} missingLabel - Label used for missing values.
 */

/**
 * Options for decategorizing categorical attributes.
 * @typedef {object} DecategorizeOptions
 * @property {string=} missingLabel - Label used for missing values.
 */

/**
 * Buffers returned for multi-category attributes.
 * @typedef {object} MultiCategoryBuffers
 * @property {Uint32Array} offsets
 * @property {Uint32Array} ids
 * @property {Float32Array|null} weights
 * @property {number} offsetCount
 * @property {number} entryCount
 * @property {boolean} hasWeights
 * @property {number} version
 */

/**
 * Endpoint selection values for edge operations.
 * @typedef {number|'source'|'destination'|'both'} EndpointSelection
 */

/**
 * Selector options for index selectors.
 * @typedef {object} SelectorOptions
 * @property {'node'|'edge'=} scope - Selector scope.
 * @property {boolean=} fullCoverage - Whether selector covers all active items.
 */

/**
 * Event handler invoked by `on`/`off`.
 * @callback EventHandler
 * @param {any} event
 * @returns {void}
 */

/**
 * Event payload for `onAny` listeners.
 * @typedef {object} AnyEventPayload
 * @property {string} type
 * @property {any} detail
 * @property {any} event
 * @property {HeliosNetwork} target
 */

/**
 * Handler for `onAny` listeners.
 * @callback AnyEventHandler
 * @param {AnyEventPayload} payload
 * @returns {void}
 */

/**
 * Unsubscribe callback returned by event helpers.
 * @callback UnsubscribeFn
 * @returns {void}
 */

/**
 * Options supporting abort signals for listener helpers.
 * @typedef {object} SignalOptions
 * @property {AbortSignal=} signal
 */

/**
 * Options for buffer memory usage reports.
 * @typedef {object} BufferMemoryUsageOptions
 */

/**
 * Options for buffer version queries.
 * @typedef {object} BufferVersionOptions
 */

/**
 * Node-to-edge passthrough descriptor.
 * @typedef {object} NodeToEdgePassthrough
 * @property {string} edgeName
 * @property {string} sourceName
 * @property {EndpointSelection} endpoints
 * @property {boolean} doubleWidth
 */

/**
 * Attribute filter lists per scope.
 * @typedef {object} AttributeFilterMap
 * @property {string[]} [node]
 * @property {string[]} [edge]
 * @property {string[]} [network]
 * @property {string[]} [graph]
 */

/**
 * Options for .bxnet/.xnet serialization.
 * @typedef {object} SaveSerializedOptions
 * @property {string=} path
 * @property {('uint8array'|'arraybuffer'|'base64'|'blob')=} format
 * @property {AttributeFilterMap=} allowAttributes
 * @property {AttributeFilterMap=} ignoreAttributes
 */

/**
 * Options for .zxnet serialization.
 * @typedef {object} SaveZXNetOptions
 * @property {string=} path
 * @property {number=} compressionLevel
 * @property {('uint8array'|'arraybuffer'|'base64'|'blob')=} format
 * @property {AttributeFilterMap=} allowAttributes
 * @property {AttributeFilterMap=} ignoreAttributes
 */

/**
 * Optional ordering descriptor for filtered node/edge outputs.
 * @typedef {object} FilterOrderSpec
 * @property {'id'|'attribute'=} type
 * @property {string=} by
 * @property {string=} attribute
 * @property {string=} name
 * @property {'asc'|'desc'=} direction
 * @property {number=} component
 */

/**
 * Options for filtered subgraph extraction.
 * @typedef {object} FilterSubgraphOptions
 * @property {string=} nodeQuery
 * @property {string=} edgeQuery
 * @property {NodeSelector|Iterable<number>|Uint32Array=} nodeSelector
 * @property {EdgeSelector|Iterable<number>|Uint32Array=} edgeSelector
 * @property {NodeSelector|Iterable<number>|Uint32Array=} nodeSelection
 * @property {EdgeSelector|Iterable<number>|Uint32Array=} edgeSelection
 * @property {FilterOrderSpec|string=} orderNodesBy
 * @property {FilterOrderSpec|string=} orderEdgesBy
 * @property {boolean=} asSelector
 */

/**
 * Cached WASM module promise and resolved instance.
 * @type {Promise<object>|null}
 */
let modulePromise = null;
/**
 * Resolved Helios WASM module instance once initialization completes.
 * @type {object|null}
 */
let moduleInstance = null;

/**
 * Lazily instantiates the Helios WASM module and reuses the instance.
 *
 * @param {Record<string, unknown>} [options] - Optional configuration forwarded
 *   to the Emscripten loader (for example `locateFile`).
 * @returns {Promise<object>} Resolves with the initialized WASM module.
 */
async function getModule(options = {}) {
	if (!modulePromise) {
		modulePromise = (async () => {
			const moduleOptions = { ...options };
			if (typeof moduleOptions.locateFile !== 'function') {
				moduleOptions.locateFile = (path, scriptDirectory = '') => {
					if (typeof path === 'string' && path.endsWith('.wasm')) {
						if (scriptDirectory && scriptDirectory.includes('compiled')) {
							return `${scriptDirectory}${path}`;
						}
						return new URL('./compiled/' + path, import.meta.url).href;
					}
					return `${scriptDirectory ?? ''}${path ?? ''}`;
				};
			}
			const instance = await createHeliosModule(moduleOptions);
			if (instance.ready) {
				await instance.ready;
			}
			moduleInstance = instance;
			return instance;
		})();
	}
	return modulePromise;
}

/**
 * Attribute types that require storing handles to JavaScript-managed values.
 * @const {Set<number>}
 */
const COMPLEX_ATTRIBUTE_TYPES = new Set([AttributeType.Data, AttributeType.Javascript]);
/**
 * Attribute types that store pointers within the WASM heap.
 * @const {Set<number>}
 */
const POINTER_ATTRIBUTE_TYPES = new Set([AttributeType.String, AttributeType.Data, AttributeType.Javascript]);

/**
 * Map of attribute type -> typed array constructor used by the WASM heap views.
 * Complex and pointer-based attribute types are handled separately.
 * @const {Record<number, Function>}
 */
const TypedArrayForType = {
	[AttributeType.Boolean]: Uint8Array,
	[AttributeType.Float]: Float32Array,
	[AttributeType.Double]: Float64Array,
	[AttributeType.Integer]: Int32Array,
	[AttributeType.UnsignedInteger]: Uint32Array,
	[AttributeType.Category]: Int32Array,
	[AttributeType.BigInteger]: BigInt64Array,
	[AttributeType.UnsignedBigInteger]: BigUint64Array,
};

/**
 * Size in bytes of each attribute type element.
 * @const {Record<number, number>}
 */
const TYPE_ELEMENT_SIZE = {
	[AttributeType.Boolean]: 1,
	[AttributeType.Float]: 4,
	[AttributeType.Double]: 8,
	[AttributeType.Integer]: 4,
	[AttributeType.UnsignedInteger]: 4,
	[AttributeType.Category]: 4,
	[AttributeType.String]: 4,
	[AttributeType.Data]: 4,
	[AttributeType.Javascript]: 4,
	[AttributeType.BigInteger]: 8,
	[AttributeType.UnsignedBigInteger]: 8,
	[AttributeType.MultiCategory]: 0,
};

/**
 * Materializes a `Set<number>` into a compact `Uint32Array`.
 *
 * @param {Set<number>} values - Unique numeric values to convert.
 * @returns {Uint32Array} Frozen copy of the provided values.
 */
function setToUint32Array(values) {
	if (!values.size) {
		return new Uint32Array();
	}
	const result = new Uint32Array(values.size);
	let idx = 0;
	for (const value of values) {
		result[idx] = value;
		idx += 1;
	}
	return result;
}

const EMPTY_UINT32 = new Uint32Array(0);

const BaseEventTarget = typeof globalThis.EventTarget === 'function'
	? globalThis.EventTarget
	: class {
		constructor() {
			this.__listeners = new Map();
		}
		addEventListener(type, handler, options = undefined) {
			if (!type || typeof handler !== 'function') {
				return;
			}
			const once = Boolean(options && typeof options === 'object' && options.once);
			if (!this.__listeners.has(type)) {
				this.__listeners.set(type, []);
			}
			const bucket = this.__listeners.get(type);
			const entry = {
				original: handler,
				listener: handler,
				once,
			};
			bucket.push(entry);
		}
		removeEventListener(type, handler) {
			const bucket = this.__listeners.get(type);
			if (!bucket || !bucket.length) {
				return;
			}
			for (let i = bucket.length - 1; i >= 0; i -= 1) {
				if (bucket[i].original === handler || bucket[i].listener === handler) {
					bucket.splice(i, 1);
				}
			}
			if (bucket.length === 0) {
				this.__listeners.delete(type);
			}
		}
		dispatchEvent(event) {
			const type = event?.type;
			if (!type) {
				return true;
			}
			const bucket = this.__listeners.get(type);
			if (!bucket || !bucket.length) {
				return true;
			}
			const snapshot = bucket.slice();
			for (const entry of snapshot) {
				try {
					event.target = event.target ?? this;
					event.currentTarget = event.currentTarget ?? this;
					entry.listener.call(this, event);
				} catch (_) {
					/* no-op */
				}
				if (entry.once) {
					this.removeEventListener(type, entry.original);
				}
			}
			return true;
		}
	};

function createDetailEvent(type, detail) {
	if (typeof globalThis.CustomEvent === 'function') {
		return new CustomEvent(type, { detail });
	}
	if (typeof globalThis.Event === 'function') {
		const event = new Event(type);
		try {
			Object.defineProperty(event, 'detail', { value: detail });
		} catch (_) {
			try {
				event.detail = detail;
			} catch (_) {
				/* no-op */
			}
		}
		return event;
	}
	return { type, detail };
}

function parseNamespacedType(typeWithNamespace) {
	if (typeof typeWithNamespace !== 'string' || typeWithNamespace.length === 0) {
		throw new TypeError('Event type must be a non-empty string');
	}
	const splitAt = typeWithNamespace.lastIndexOf('.');
	if (splitAt <= 0 || splitAt === typeWithNamespace.length - 1) {
		throw new Error('Namespaced event types must be in the form "type.namespace"');
	}
	return {
		type: typeWithNamespace.slice(0, splitAt),
		namespace: typeWithNamespace.slice(splitAt + 1),
	};
}

const HELIOS_NETWORK_EVENTS = Object.freeze({
	topologyChanged: 'topology:changed',
	nodesAdded: 'nodes:added',
	nodesRemoved: 'nodes:removed',
	edgesAdded: 'edges:added',
	edgesRemoved: 'edges:removed',

	attributeDefined: 'attribute:defined',
	attributeRemoved: 'attribute:removed',
	attributeChanged: 'attribute:changed',
});

const NODE_RUNTIME = typeof process !== 'undefined' && !!process.versions?.node;
const VIRTUAL_TEMP_DIR = '/tmp/helios';

let nodeFsModulePromise = null;
let nodePathModulePromise = null;
let nodeOsModulePromise = null;

function isNodeRuntime() {
	return NODE_RUNTIME;
}

async function getNodeFsModule() {
	if (!isNodeRuntime()) {
		throw new Error('Node filesystem is unavailable in this environment');
	}
	if (!nodeFsModulePromise) {
		nodeFsModulePromise = import('node:fs/promises').catch(() => import('fs/promises'));
	}
	const mod = await nodeFsModulePromise;
	return mod.default ?? mod;
}

async function getNodeOsModule() {
	if (!isNodeRuntime()) {
		throw new Error('OS helpers are only available in Node runtimes');
	}
	if (!nodeOsModulePromise) {
		nodeOsModulePromise = import('node:os').catch(() => import('os'));
	}
	const mod = await nodeOsModulePromise;
	return mod.default ?? mod;
}

async function getNodePathModule() {
	if (!isNodeRuntime()) {
		throw new Error('Path resolution is only supported in Node runtimes');
	}
	if (!nodePathModulePromise) {
		nodePathModulePromise = import('node:path').catch(() => import('path'));
	}
	const mod = await nodePathModulePromise;
	return mod.default ?? mod;
}

async function resolveNodePath(targetPath) {
	const mod = await getNodePathModule();
	const resolver = typeof mod.resolve === 'function' ? mod.resolve.bind(mod) : mod.default?.resolve?.bind(mod.default);
	if (!resolver) {
		throw new Error('Unable to resolve file path in Node runtime');
	}
	return resolver(targetPath);
}

async function createNodeTempPath(extension) {
	const os = await getNodeOsModule();
	const pathMod = await getNodePathModule();
	const baseDir = typeof os.tmpdir === 'function' ? os.tmpdir() : '/tmp';
	const name = `helios-${Date.now().toString(16)}-${Math.random().toString(16).slice(2)}.${extension}`;
	const joiner = typeof pathMod.join === 'function'
		? pathMod.join.bind(pathMod)
		: pathMod.default?.join?.bind(pathMod.default);
	return joiner ? joiner(baseDir, name) : `${baseDir}/${name}`;
}

function getModuleFS(module) {
	try {
		const candidate = module?.FS;
		if (candidate && typeof candidate.mkdirTree === 'function') {
			return candidate;
		}
	} catch (_) {
		// FS not exported
	}
	return null;
}

function ensureVirtualTempDir(fs) {
	if (!fs || typeof fs.mkdirTree !== 'function') {
		throw new Error('Emscripten FS support is required for serialization features');
	}
	fs.mkdirTree(VIRTUAL_TEMP_DIR);
}

function createVirtualPath(fs, extension) {
	ensureVirtualTempDir(fs);
	const timestamp = Date.now().toString(16);
	const random = Math.random().toString(16).slice(2);
	return `${VIRTUAL_TEMP_DIR}/${timestamp}-${random}.${extension}`;
}

function clampCompressionLevel(level) {
	if (!Number.isFinite(level)) {
		return 6;
	}
	const value = Math.round(level);
	if (value < 0) {
		return 0;
	}
	if (value > 9) {
		return 9;
	}
	return value;
}

const ATTRIBUTE_FILTER_SCOPE_KEYS = {
	node: ['node', 'nodes'],
	edge: ['edge', 'edges'],
	network: ['network', 'graph', 'graphs'],
};

function extractAttributeFilterList(source, keys, label) {
	for (const key of keys) {
		if (!Object.prototype.hasOwnProperty.call(source, key)) {
			continue;
		}
		const value = source[key];
		if (value == null) {
			return [];
		}
		if (!Array.isArray(value)) {
			throw new Error(`${label} must be an array of strings`);
		}
		const result = [];
		for (const entry of value) {
			if (typeof entry !== 'string') {
				throw new Error(`${label} must only contain strings`);
			}
			result.push(entry);
		}
		return result;
	}
	return [];
}

function normalizeAttributeFilter(source, label) {
	if (!source) {
		return null;
	}
	if (typeof source !== 'object' || Array.isArray(source)) {
		throw new Error(`${label} must be an object with node/edge/network arrays`);
	}
	const filter = {
		node: extractAttributeFilterList(source, ATTRIBUTE_FILTER_SCOPE_KEYS.node, `${label}.node`),
		edge: extractAttributeFilterList(source, ATTRIBUTE_FILTER_SCOPE_KEYS.edge, `${label}.edge`),
		network: extractAttributeFilterList(source, ATTRIBUTE_FILTER_SCOPE_KEYS.network, `${label}.network`),
	};
	return (filter.node.length || filter.edge.length || filter.network.length) ? filter : null;
}

async function resolveInputBytes(source) {
	if (source == null) {
		throw new TypeError('No source provided for deserialization');
	}
	if (typeof source === 'string') {
		if (!isNodeRuntime()) {
			throw new TypeError('String paths are only supported in Node runtimes');
		}
		const fs = await getNodeFsModule();
		const resolved = await resolveNodePath(source);
		const buffer = await fs.readFile(resolved);
		return buffer instanceof Uint8Array ? new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength) : new Uint8Array(buffer);
	}
	if (source instanceof Uint8Array) {
		return source;
	}
	if (ArrayBuffer.isView(source)) {
		return new Uint8Array(source.buffer, source.byteOffset, source.byteLength);
	}
	if (source instanceof ArrayBuffer) {
		return new Uint8Array(source);
	}
	if (typeof source === 'object' && typeof source.arrayBuffer === 'function') {
		const buffer = await source.arrayBuffer();
		return new Uint8Array(buffer);
	}
	throw new TypeError('Unsupported source type for deserialization');
}

function bytesToFormat(bytes, format = 'uint8array') {
	const normalized = typeof format === 'string' ? format.toLowerCase() : 'uint8array';
	switch (normalized) {
		case 'uint8array':
			return bytes;
		case 'arraybuffer':
			return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
		case 'base64':
			return uint8ToBase64(bytes);
		case 'blob':
			if (typeof Blob === 'undefined') {
				throw new Error('Blob is not available in this environment');
			}
			return new Blob([bytes], { type: 'application/octet-stream' });
		default:
			throw new Error(`Unsupported output format "${format}"`);
	}
}

function uint8ToBase64(bytes) {
	if (isNodeRuntime()) {
		return Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength).toString('base64');
	}
	if (typeof btoa !== 'function') {
		throw new Error('Base64 encoding is not supported in this environment');
	}
	let binary = '';
	const chunkSize = 0x8000;
	for (let i = 0; i < bytes.length; i += chunkSize) {
		const slice = bytes.subarray(i, i + chunkSize);
		let chunk = '';
		for (let j = 0; j < slice.length; j += 1) {
			chunk += String.fromCharCode(slice[j]);
		}
		binary += chunk;
	}
	return btoa(binary);
}

/**
 * Projects attribute values for the provided selection scope into JavaScript data structures.
 *
 * @param {HeliosNetwork} network - Owning network instance.
 * @param {'node'|'edge'} scope - Attribute scope to read.
 * @param {string} name - Attribute identifier.
 * @param {Uint32Array} indices - Selection indices.
 * @param {Object} [options] - Projection options.
 * @param {boolean} [options.raw=false] - When true, returns the raw typed buffer.
 * @returns {Array|TypedArray} Resolved attribute payload.
 */
function projectAttributeValues(network, scope, name, indices, options = {}) {
	const { raw = false } = options;
	return network.withBufferAccess(() => {
		const buffer = scope === 'node'
			? network.getNodeAttributeBuffer(name)
			: network.getEdgeAttributeBuffer(name);

		if ('get' in buffer && typeof buffer.get === 'function') {
			const result = new Array(indices.length);
			for (let i = 0; i < indices.length; i += 1) {
				const index = indices[i];
				result[i] = buffer.get(index) ?? null;
			}
			return result;
		}

		if (buffer.type === AttributeType.String) {
			const reader = scope === 'node'
				? network.getNodeStringAttribute.bind(network)
				: network.getEdgeStringAttribute.bind(network);
			if (!indices.length) {
				return [];
			}
			const result = new Array(indices.length);
			for (let i = 0; i < indices.length; i += 1) {
				result[i] = reader(name, indices[i]);
			}
			return result;
		}

		if (!('view' in buffer) || !buffer.view) {
			throw new Error(`Attribute "${name}" on ${scope} is not a numeric attribute`);
		}

		const { view, dimension } = buffer;
		const TypedCtor = view.constructor;

		if (!indices.length) {
			return raw && dimension === 1 ? new TypedCtor(0) : [];
		}

		if (dimension === 1) {
			if (raw) {
				const result = new TypedCtor(indices.length);
				for (let i = 0; i < indices.length; i += 1) {
					result[i] = view[indices[i]];
				}
				return result;
			}
			const result = new Array(indices.length);
			for (let i = 0; i < indices.length; i += 1) {
				result[i] = view[indices[i]];
			}
			return result;
		}

		const result = new Array(indices.length);
		for (let i = 0; i < indices.length; i += 1) {
			const index = indices[i];
			const start = index * dimension;
			const slice = view.slice(start, start + dimension);
			result[i] = raw ? slice : Array.from(slice);
		}
		return result;
	});
}

/**
 * RAII-style helper that marshals temporary UTF-8 strings into WASM memory.
 * @private
 */
class CString {
	/**
	 * @param {object} module - Emscripten module exposing `_malloc` and UTF-8 helpers.
	 * @param {string} value - Text to encode into WASM memory.
	 * @throws {Error} When memory allocation fails.
	 */
	constructor(module, value) {
		this.module = module;
		this.ptr = module._malloc(module.lengthBytesUTF8(value) + 1);
		if (!this.ptr) {
			throw new Error('Failed to allocate string memory');
		}
		module.stringToUTF8(value, this.ptr, module.lengthBytesUTF8(value) + 1);
	}

	/**
	 * Releases the allocated UTF-8 buffer. Subsequent calls become no-ops.
	 */
	dispose() {
		if (this.ptr) {
			this.module._free(this.ptr);
			this.ptr = 0;
		}
	}
}

/**
 * Helper that marshals a list of UTF-8 strings into a WASM pointer array.
 * @private
 */
class CStringArray {
	/**
	 * @param {object} module - Emscripten module exposing `_malloc` and UTF-8 helpers.
	 * @param {string[]} values - Strings to encode into WASM memory.
	 */
	constructor(module, values) {
		this.module = module;
		this.ptr = 0;
		this.count = 0;
		this._strings = [];
		if (!Array.isArray(values) || values.length === 0) {
			return;
		}
		this.count = values.length;
		try {
			for (let i = 0; i < values.length; i += 1) {
				const value = values[i];
				if (typeof value !== 'string') {
					throw new Error('Attribute filter names must be strings');
				}
				const cstr = new CString(module, value);
				this._strings.push(cstr);
			}
			this.ptr = module._malloc(values.length * 4);
			if (!this.ptr) {
				throw new Error('Failed to allocate string pointer array');
			}
			const heap = module.HEAPU32;
			const baseIndex = this.ptr >>> 2;
			for (let i = 0; i < this._strings.length; i += 1) {
				heap[baseIndex + i] = this._strings[i].ptr;
			}
		} catch (err) {
			this.dispose();
			throw err;
		}
	}

	/**
	 * Releases allocated memory for the string list.
	 */
	dispose() {
		for (const cstr of this._strings) {
			cstr.dispose();
		}
		this._strings = [];
		if (this.ptr) {
			this.module._free(this.ptr);
			this.ptr = 0;
		}
		this.count = 0;
	}
}

/**
 * Minimal wrapper around a native selector handle.
 * Provides shared behavior for node and edge selectors.
 * @private
 */
class Selector {
	/**
	 * @param {object} module - Active Helios WASM module.
	 * @param {HeliosNetwork} network - Owning Helios network wrapper.
	 * @param {number} ptr - Pointer to the native selector.
	 * @param {{destroyFn:function(number):void,countFn:function(number):number,dataFn:function(number):number}} fns - Selector function table.
	 * @param {SelectorOptions} [options] - Selector options.
	 */
	constructor(module, network, ptr, fns, options = {}) {
		this.module = module;
		this.network = network;
		this.ptr = ptr;
		this._destroyFn = fns.destroyFn;
		this._countFn = fns.countFn;
		this._dataFn = fns.dataFn;
		this._scope = options.scope || null;
		this._fullCoverage = Boolean(options.fullCoverage);
	}

	/**
	 * @returns {number} Number of stored indices.
	 */
	get count() {
		if (this._fullCoverage) {
			return this._fullCount();
		}
		return this._countFn(this.ptr);
	}

	/**
	 * @returns {number} Pointer to the selector data buffer.
	 */
	get dataPointer() {
		if (this._fullCoverage) {
			return 0;
		}
		return this._dataFn(this.ptr);
	}

	/**
	 * Copies the selector's indices into a new `Uint32Array`.
	 *
	 * @returns {Uint32Array} Copied selection.
	 */
	toTypedArray() {
		if (this._fullCoverage) {
			return this._materializeFullIndexArray();
		}
		const count = this.count;
		const ptr = this.dataPointer;
		if (!ptr || count === 0) {
			return new Uint32Array();
		}
		return new Uint32Array(this.module.HEAPU32.buffer, ptr, count).slice();
	}

	/**
	 * Iterator over the selector indices.
	 *
	 * @returns {IterableIterator<number>} Generator yielding each index.
	 */
	*[Symbol.iterator]() {
		if (this._fullCoverage) {
			const indices = this._materializeFullIndexArray();
			for (let i = 0; i < indices.length; i += 1) {
				yield indices[i];
			}
			return;
		}
		const count = this.count;
		const ptr = this.dataPointer;
		if (!ptr || count === 0) {
			return;
		}
		const base = ptr >>> 2;
		for (let i = 0; i < count; i += 1) {
			yield this.module.HEAPU32[base + i];
		}
	}

	/**
	 * Convenience wrapper that materializes the selector as a plain array.
	 *
	 * @returns {number[]} Copied index array.
	 */
	toArray() {
		return Array.from(this);
	}

	/**
	 * Applies the provided callback to every index in the selector.
	 *
	 * @param {function(number, number):void} callback - Invoked with (index, position).
	 */
	forEach(callback) {
		let position = 0;
		for (const index of this) {
			callback(index, position);
			position += 1;
		}
	}

	/**
	 * Maps the indices into a new array using the provided transformer.
	 *
	 * @template T
	 * @param {function(number, number):T} mapper - Invoked with (index, position).
	 * @returns {T[]} Array of mapped values.
	 */
	map(mapper) {
		const results = [];
		let position = 0;
		for (const index of this) {
			results.push(mapper(index, position));
			position += 1;
		}
		return results;
	}

	/**
	 * Provides a standalone iterator instance over the selector indices.
	 *
	 * @returns {IterableIterator<number>} Iterator yielding selector indices.
	 */
	values() {
		return this[Symbol.iterator]();
	}

	/**
	 * Releases the native selector.
	 */
	dispose() {
		if (this.ptr && this._destroyFn) {
			this._destroyFn(this.ptr);
			this.ptr = 0;
		}
	}

	_fullCount() {
		if (!this.network) {
			return 0;
		}
		if (this._scope === 'node') {
			return this.network.nodeCount;
		}
		if (this._scope === 'edge') {
			return this.network.edgeCount;
		}
		return 0;
	}

	_materializeFullIndexArray() {
		if (!this.network) {
			return new Uint32Array();
		}
		if (this._scope === 'node') {
			return this.network.withBufferAccess(() => this.network.nodeIndices.slice(), { nodeIndices: true });
		}
		if (this._scope === 'edge') {
			return this.network.withBufferAccess(() => this.network.edgeIndices.slice(), { edgeIndices: true });
		}
		return new Uint32Array();
	}

	_setFullCoverage(enabled) {
		this._fullCoverage = Boolean(enabled);
	}
}

/**
 * Selector specialized for node indices.
 */
class NodeSelector extends Selector {
	/**
	 * Allocates a node selector within the WASM module.
	 *
	 * @param {object} module - Active Helios WASM module.
	 * @param {HeliosNetwork} network - Owning network instance.
	 * @returns {NodeSelector} Newly allocated selector wrapper.
	 * @throws {Error} When allocation fails.
	 */
	static create(module, network) {
		const ptr = module._CXNodeSelectorCreate(0);
		if (!ptr) {
			throw new Error('Failed to create node selector');
		}
		return new NodeSelector(module, network, ptr);
	}

	/**
	 * Allocates a selector representing all nodes without materializing the ids in WASM.
	 *
	 * @param {HeliosNetwork} network - Owning network instance.
	 * @returns {NodeSelector} Proxy-backed selector.
	 */
	static createAll(network) {
		const selector = NodeSelector.create(network.module, network);
		selector._setFullCoverage(true);
		return selector._asProxy();
	}

	/**
	 * Internal helper that instantiates a selector from explicit indices.
	 *
	 * @param {HeliosNetwork} network - Owning network instance.
	 * @param {Uint32Array} indices - Indices to populate.
	 * @returns {NodeSelector} Populated selector proxy.
	 */
	static fromIndices(network, indices) {
		const selector = NodeSelector.create(network.module, network);
		selector.fillFromArray(network, indices);
		return selector._asProxy();
	}

	/**
	 * @param {object} module - Active Helios WASM module.
	 * @param {number} ptr - Pointer to the native selector.
	 */
	constructor(module, network, ptr) {
		super(module, network, ptr, {
			destroyFn: module._CXNodeSelectorDestroy,
			countFn: module._CXNodeSelectorCount,
			dataFn: module._CXNodeSelectorData,
		}, { scope: 'node' });
		this._proxy = null;
		this._attributeProxy = null;
	}

	/**
	 * Fills the selector with every node in the provided network.
	 *
	 * @param {HeliosNetwork} network - Network to source node indices from.
	 * @returns {NodeSelector} This selector for chaining.
	 */
	fillAll(network) {
		this.module._CXNodeSelectorFillAll(this.ptr, network.ptr);
		return this;
	}

	/**
	 * Fills the selector with the given node indices.
	 *
	 * @param {HeliosNetwork} network - Network that owns the indices.
	 * @param {Iterable<number>|Uint32Array} indices - Node indices to select.
	 * @returns {NodeSelector} This selector for chaining.
	 * @throws {Error} When memory allocation fails.
	 */
	fillFromArray(network, indices) {
		network?._assertCanAllocate?.('node selector materialization');
		const array = Array.isArray(indices) ? Uint32Array.from(indices) : new Uint32Array(indices);
		if (array.length === 0) {
			if (typeof this.module._CXNodeSelectorClear === 'function') {
				this.module._CXNodeSelectorClear(this.ptr);
			} else {
				this.module._CXNodeSelectorFillFromArray(this.ptr, 0, 0);
			}
			return this;
		}
		const size = array.length * 4;
		const ptr = this.module._malloc(size);
		if (!ptr) {
			throw new Error('Failed to allocate selector buffer');
		}
		this.module.HEAPU32.set(array, ptr / 4);
		this.module._CXNodeSelectorFillFromArray(this.ptr, ptr, array.length);
		this.module._free(ptr);
		return this;
	}

	/**
	 * @returns {boolean} Whether the network defines the specified attribute.
	 */
	hasAttribute(name) {
		return this.network?.hasNodeAttribute(name) ?? false;
	}
	/**
	 * Resolves attribute values for the selected nodes.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {Object} [options] - Projection options.
	 * @param {boolean} [options.raw=false] - When true, returns the raw typed buffer.
	 * @returns {Array|TypedArray} Attribute values aligned with the selector indices.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.defineNodeAttribute('weight', AttributeType.Float);
	 * const nodes = net.addNodes(2);
	 * const selector = net.createNodeSelector(nodes);
	 * const weights = selector.attribute('weight');
	 * console.log(weights[0]); // weight for the first node in the selector
	 */
	attribute(name, options = {}) {
		if (!this.hasAttribute(name)) {
			throw new Error(`Unknown node attribute "${name}"`);
		}
		return projectAttributeValues(this.network, 'node', name, this.toTypedArray(), options);
	}

	/**
	 * Provides a proxy that lazily exposes attributes via property access.
	 *
	 * @returns {NodeSelector} Proxy-backed selector.
	 */
	_asProxy() {
		if (!this._proxy) {
			const attributeProxy = () => {
				if (!this._attributeProxy) {
					this._attributeProxy = new Proxy({}, {
						get: (_, prop) => {
							if (typeof prop !== 'string') {
								return undefined;
							}
							if (!this.hasAttribute(prop)) {
								return undefined;
							}
							return this.attribute(prop);
						},
					});
				}
				return this._attributeProxy;
			};

			this._proxy = new Proxy(this, {
				get: (target, prop) => {
					if (typeof prop === 'symbol') {
						return Reflect.get(target, prop, target);
					}
					if (prop === 'attributes') {
						return attributeProxy();
					}
					if (prop in target) {
						const value = Reflect.get(target, prop, target);
						return typeof value === 'function' ? value.bind(target) : value;
					}
					if (typeof prop === 'string' && target.hasAttribute(prop)) {
						return target.attribute(prop);
					}
					return Reflect.get(target, prop, target);
				},
			});
		}
		return this._proxy;
	}

	/**
	 * Computes neighbour sets for the selected nodes.
	 *
	 * @param {Object} [options] - Query options.
	 * @param {'out'|'in'|'both'|'all'} [options.mode='out'] - Determines which neighbors to include.
	 * @param {boolean} [options.includeEdges=false] - When true, also collects traversed edges.
	 * @param {boolean} [options.asSelector=false] - When true, returns selector proxies.
	 * @returns {(Uint32Array|{nodes:(Uint32Array|NodeSelector), edges:(Uint32Array|EdgeSelector)})} Neighbor information.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(3);
	 * net.addEdges([[nodes[0], nodes[1]], [nodes[2], nodes[0]]]);
	 *
	 * const selector = net.createNodeSelector([nodes[0]]);
	 * const { nodes: neighbours } = selector.neighbors({ mode: 'both' });
	 * console.log([...neighbours]); // neighbouring node indices for nodes[0]
	 */
	neighbors(options = {}) {
		const {
			mode = 'out',
			includeEdges = false,
			asSelector = false,
		} = options;
		return this.network.getNeighborsForNodes(this.toTypedArray(), {
			direction: mode,
			includeEdges,
			asSelector,
			includeSourceNodes: true,
		});
	}

	/**
	 * Computes neighbors at exactly the requested hop distance from this selector.
	 *
	 * @param {number} level - Target concentric level (0 = sources).
	 * @param {Object} [options] - Query options.
	 * @param {'out'|'in'|'both'|'all'} [options.mode='out'] - Traversal direction.
	 * @param {boolean} [options.includeEdges=false] - When true, include traversed edge ids.
	 * @param {boolean} [options.includeSources=false] - Whether source nodes can appear in results.
	 * @param {boolean} [options.asSelector=false] - When true, returns selector proxies.
	 * @returns {(Uint32Array|{nodes:(Uint32Array|NodeSelector), edges:(Uint32Array|EdgeSelector)})}
	 */
	neighborsAtLevel(level, options = {}) {
		const {
			mode = 'out',
			includeEdges = false,
			includeSources = false,
			asSelector = false,
		} = options;
		return this.network.getNeighborsAtLevel(this.toTypedArray(), level, {
			direction: mode,
			includeEdges,
			includeSourceNodes: includeSources,
			asSelector,
		});
	}

	/**
	 * Computes neighbors up to the requested hop distance from this selector.
	 *
	 * @param {number} maxLevel - Maximum concentric level (inclusive).
	 * @param {Object} [options] - Query options.
	 * @param {'out'|'in'|'both'|'all'} [options.mode='out'] - Traversal direction.
	 * @param {boolean} [options.includeEdges=false] - When true, include traversed edge ids.
	 * @param {boolean} [options.includeSources=false] - Whether source nodes can appear in results.
	 * @param {boolean} [options.asSelector=false] - When true, returns selector proxies.
	 * @returns {(Uint32Array|{nodes:(Uint32Array|NodeSelector), edges:(Uint32Array|EdgeSelector)})}
	 */
	neighborsUpToLevel(maxLevel, options = {}) {
		const {
			mode = 'out',
			includeEdges = false,
			includeSources = false,
			asSelector = false,
		} = options;
		return this.network.getNeighborsUpToLevel(this.toTypedArray(), maxLevel, {
			direction: mode,
			includeEdges,
			includeSourceNodes: includeSources,
			asSelector,
		});
	}

	/**
	 * Derives degree values for the selected nodes.
	 *
	 * @param {Object} [options] - Degree configuration.
	 * @param {'out'|'in'|'both'|'all'} [options.mode='out'] - Determines which edge directions to count.
	 * @returns {number[]} Degree values aligned with the selector order.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(2);
	 * net.addEdges([[nodes[0], nodes[1]], [nodes[0], nodes[1]]]);
	 *
	 * const selector = net.createNodeSelector(nodes);
	 * console.log(selector.degree({ mode: 'out' })); // [2, 0]
	 */
	degree(options = {}) {
		const { mode = 'out' } = options;
		const collectOut = mode === 'out' || mode === 'both' || mode === 'all';
		const collectIn = mode === 'in' || mode === 'both' || mode === 'all';

		return this.map((node) => {
			let count = 0;
			if (collectOut) {
				count += this.network.getOutNeighbors(node).nodes.length;
			}
			if (collectIn) {
				count += this.network.getInNeighbors(node).nodes.length;
			}
			return count;
		});
	}

	/**
	 * Retrieves incident edges for the selected nodes.
	 *
	 * @param {Object} [options] - Incident edge options.
	 * @param {'out'|'in'|'both'|'all'} [options.mode='out'] - Determines which edges to collect.
	 * @param {boolean} [options.asSelector=false] - When true, returns a selector proxy.
	 * @returns {Uint32Array|EdgeSelector} Edge indices or selector proxy.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(2);
	 * const edges = net.addEdges([[nodes[0], nodes[1]]]);
	 *
	 * const selector = net.createNodeSelector([nodes[1]]);
	 * const touching = selector.incidentEdges({ mode: 'both' });
	 * console.log([...touching]); // [edges[0]]
	 */
	incidentEdges(options = {}) {
		const { mode = 'out', asSelector = false } = options;
		const edgeSet = new Set();
		const collectOut = mode === 'out' || mode === 'both' || mode === 'all';
		const collectIn = mode === 'in' || mode === 'both' || mode === 'all';

		for (const node of this) {
			if (collectOut) {
				for (const edge of this.network.getOutNeighbors(node).edges) {
					edgeSet.add(edge);
				}
			}
			if (collectIn) {
				for (const edge of this.network.getInNeighbors(node).edges) {
					edgeSet.add(edge);
				}
			}
		}

		const edgesArray = setToUint32Array(edgeSet);
		return asSelector ? EdgeSelector.fromIndices(this.network, edgesArray) : edgesArray;
	}
}

/**
 * Selector specialized for edge indices.
 */
class EdgeSelector extends Selector {
	/**
	 * Allocates an edge selector within the WASM module.
	 *
	 * @param {object} module - Active Helios WASM module.
	 * @param {HeliosNetwork} network - Owning network instance.
	 * @returns {EdgeSelector} Newly allocated selector wrapper.
	 * @throws {Error} When allocation fails.
	 */
	static create(module, network) {
		const ptr = module._CXEdgeSelectorCreate(0);
		if (!ptr) {
			throw new Error('Failed to create edge selector');
		}
		return new EdgeSelector(module, network, ptr);
	}

	/**
	 * Allocates a selector representing all edges without materializing the ids in WASM.
	 *
	 * @param {HeliosNetwork} network - Owning network instance.
	 * @returns {EdgeSelector} Proxy-backed selector.
	 */
	static createAll(network) {
		const selector = EdgeSelector.create(network.module, network);
		selector._setFullCoverage(true);
		return selector._asProxy();
	}

	/**
	 * Instantiates a selector populated with the provided edge indices.
	 *
	 * @param {HeliosNetwork} network - Owning network instance.
	 * @param {Uint32Array} indices - Edge indices to populate.
	 * @returns {EdgeSelector} Populated selector proxy.
	 */
	static fromIndices(network, indices) {
		const selector = EdgeSelector.create(network.module, network);
		selector.fillFromArray(network, indices);
		return selector._asProxy();
	}

	/**
	 * @param {object} module - Active Helios WASM module.
	 * @param {number} ptr - Pointer to the native selector.
	 */
	constructor(module, network, ptr) {
		super(module, network, ptr, {
			destroyFn: module._CXEdgeSelectorDestroy,
			countFn: module._CXEdgeSelectorCount,
			dataFn: module._CXEdgeSelectorData,
		}, { scope: 'edge' });
		this._proxy = null;
		this._attributeProxy = null;
	}

	/**
	 * Fills the selector with every edge in the provided network.
	 *
	 * @param {HeliosNetwork} network - Network to source edge indices from.
	 * @returns {EdgeSelector} This selector for chaining.
	 */
	fillAll(network) {
		this.module._CXEdgeSelectorFillAll(this.ptr, network.ptr);
		return this;
	}

	/**
	 * Fills the selector with the given edge indices.
	 *
	 * @param {HeliosNetwork} network - Network that owns the indices.
	 * @param {Iterable<number>|Uint32Array} indices - Edge indices to select.
	 * @returns {EdgeSelector} This selector for chaining.
	 * @throws {Error} When memory allocation fails.
	 */
	fillFromArray(network, indices) {
		network?._assertCanAllocate?.('edge selector materialization');
		const array = Array.isArray(indices) ? Uint32Array.from(indices) : new Uint32Array(indices);
		if (array.length === 0) {
			if (typeof this.module._CXEdgeSelectorClear === 'function') {
				this.module._CXEdgeSelectorClear(this.ptr);
			} else {
				this.module._CXEdgeSelectorFillFromArray(this.ptr, 0, 0);
			}
			return this;
		}
		const size = array.length * 4;
		const ptr = this.module._malloc(size);
		if (!ptr) {
			throw new Error('Failed to allocate selector buffer');
		}
		this.module.HEAPU32.set(array, ptr / 4);
		this.module._CXEdgeSelectorFillFromArray(this.ptr, ptr, array.length);
		this.module._free(ptr);
		return this;
	}

	_setFullCoverage(enabled) {
		super._setFullCoverage(enabled);
		return this;
	}

	/**
	 * @returns {boolean} Whether the specified attribute is registered for edges.
	 */
	hasAttribute(name) {
		return Boolean(this.network?._edgeAttributes?.has(name));
	}

	/**
	 * Resolves attribute values for the selected edges.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {Object} [options] - Projection options.
	 * @param {boolean} [options.raw=false] - When true, returns the raw typed buffer.
	 * @returns {Array|TypedArray} Attribute values aligned with the selector indices.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * net.defineEdgeAttribute('capacity', AttributeType.Float);
	 * const edges = net.addEdges([[0, 1]]);
	 * const selector = net.createEdgeSelector(edges);
	 * const capacities = selector.attribute('capacity');
	 * console.log(capacities[0]); // capacity for the first edge in the selector
	 */
	attribute(name, options = {}) {
		if (!this.hasAttribute(name)) {
			throw new Error(`Unknown edge attribute "${name}"`);
		}
		return projectAttributeValues(this.network, 'edge', name, this.toTypedArray(), options);
	}

	/**
	 * Provides a proxy that exposes attributes through property access.
	 *
	 * @returns {EdgeSelector} Proxy-backed selector.
	 */
	_asProxy() {
		if (!this._proxy) {
			const attributeProxy = () => {
				if (!this._attributeProxy) {
					this._attributeProxy = new Proxy({}, {
						get: (_, prop) => {
							if (typeof prop !== 'string') {
								return undefined;
							}
							if (!this.hasAttribute(prop)) {
								return undefined;
							}
							return this.attribute(prop);
						},
					});
				}
				return this._attributeProxy;
			};

			this._proxy = new Proxy(this, {
				get: (target, prop) => {
					if (typeof prop === 'symbol') {
						return Reflect.get(target, prop, target);
					}
					if (prop === 'attributes') {
						return attributeProxy();
					}
					if (prop in target) {
						const value = Reflect.get(target, prop, target);
						return typeof value === 'function' ? value.bind(target) : value;
					}
					if (typeof prop === 'string' && target.hasAttribute(prop)) {
						return target.attribute(prop);
					}
					return Reflect.get(target, prop, target);
				},
			});
		}
		return this._proxy;
	}

	/**
	 * Retrieves the source node for each selected edge.
	 *
	 * @param {Object} [options] - Projection options.
	 * @param {boolean} [options.asSelector=false] - When true, returns a selector proxy.
	 * @param {boolean} [options.unique=false] - When true, collapses duplicates.
	 * @returns {Uint32Array|NodeSelector} Source node indices or selector proxy.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * const edges = net.addEdges([[0, 1]]);
	 * const selector = net.createEdgeSelector(edges);
	 * console.log([...selector.sources()]); // [0]
	 */
	sources(options = {}) {
		const { asSelector = false, unique = false } = options;
		return this._projectEndpoint(0, { asSelector, unique });
	}

	/**
	 * Retrieves the target node for each selected edge.
	 *
	 * @param {Object} [options] - Projection options.
	 * @param {boolean} [options.asSelector=false] - When true, returns a selector proxy.
	 * @param {boolean} [options.unique=false] - When true, collapses duplicates.
	 * @returns {Uint32Array|NodeSelector} Target node indices or selector proxy.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * const edges = net.addEdges([[0, 1]]);
	 * const selector = net.createEdgeSelector(edges);
	 * console.log([...selector.targets()]); // [1]
	 */
	targets(options = {}) {
		const { asSelector = false, unique = false } = options;
		return this._projectEndpoint(1, { asSelector, unique });
	}

	/**
	 * Returns the [source, target] pairs for the selection.
	 *
	 * @returns {Array<Array<number>>} Edge endpoint tuples.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * const edges = net.addEdges([[0, 1]]);
	 * const selector = net.createEdgeSelector(edges);
	 * console.log(selector.endpoints()); // [[0, 1]]
	 */
	endpoints() {
		return this.network.withBufferAccess(() => {
			const pairs = [];
			const edgesView = this.network.edgesView;
			for (const edge of this) {
				const base = edge * 2;
				pairs.push([edgesView[base], edgesView[base + 1]]);
			}
			return pairs;
		}, { edgesView: true });
	}

	/**
	 * Collects all nodes touched by the selected edges.
	 *
	 * @param {Object} [options] - Projection options.
	 * @param {boolean} [options.asSelector=false] - When true, returns a selector proxy.
	 * @returns {Uint32Array|NodeSelector} Node indices or selector proxy.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(3);
	 * const edges = net.addEdges([[0, 1], [1, 2]]);
	 * const selector = net.createEdgeSelector(edges);
	 *
	 * const touched = selector.nodes();
	 * console.log([...touched]); // [0, 1, 2]
	 *
	 * const nodeSelector = selector.nodes({ asSelector: true });
	 * console.log(nodeSelector.count); // 3
	 */
	nodes(options = {}) {
		const { asSelector = false } = options;
		const indices = this.network.withBufferAccess(() => {
			const nodeSet = new Set();
			const edgesView = this.network.edgesView;
			for (const edge of this) {
				const base = edge * 2;
				nodeSet.add(edgesView[base]);
				nodeSet.add(edgesView[base + 1]);
			}
			return setToUint32Array(nodeSet);
		}, { edgesView: true });
		return asSelector ? NodeSelector.fromIndices(this.network, indices) : indices;
	}

	/**
	 * Internal helper that projects either edge endpoint.
	 * @private
	 *
	 * @param {0|1} offset - 0 for source, 1 for target.
	 * @param {Object} options - Projection options.
	 * @param {boolean} options.asSelector - Whether to return a selector proxy.
	 * @param {boolean} options.unique - Whether to deduplicate node indices.
	 * @returns {Uint32Array|NodeSelector} Projected nodes.
	 */
	_projectEndpoint(offset, options) {
		const { asSelector, unique } = options;
		const indices = this.network.withBufferAccess(() => {
			const edgesView = this.network.edgesView;
			if (unique) {
				const nodeSet = new Set();
				for (const edge of this) {
					nodeSet.add(edgesView[edge * 2 + offset]);
				}
				return setToUint32Array(nodeSet);
			}

			const indices = new Uint32Array(this.count);
			let position = 0;
			for (const edge of this) {
				indices[position] = edgesView[edge * 2 + offset];
				position += 1;
			}
			return indices;
		}, { edgesView: true });
		return asSelector ? NodeSelector.fromIndices(this.network, indices) : indices;
	}
	}

	class DimensionSession {
		constructor(network, options = {}) {
			this.network = network;
			this.options = options ?? {};
			this._disposed = false;
			this._finalized = false;
			this._finalResult = null;
			this._phase = 0;
			this._canceledReason = null;

			const maxLevelInput = this.options.maxLevel ?? 8;
			this._maxLevel = Math.max(0, (maxLevelInput | 0));
			this._levels = this._maxLevel + 1;
			this._method = network._normalizeDimensionMethod(this.options.method ?? 'leastsquares');
			const orderInput = this.options.order ?? 2;
			this._order = network._normalizeDimensionOrder(this._method, orderInput);
			this._extraPadding = network._dimensionExtraPadding(this._method, this._order);
			this._capacityMaxLevel = this._maxLevel + this._extraPadding;
			this._capacityLevels = this._capacityMaxLevel + 1;

			this._selectedNodes = DimensionSession._collectSelectedNodes(network, this.options.nodes ?? null);
			this._progressTotal = this._selectedNodes.length;
			this._progressCurrent = 0;
			if (!this._progressTotal) {
				throw new Error('No active nodes selected for dimension session');
			}

			this._sumCapacity = new Float64Array(this._capacityLevels);
			this._sumNodeDimension = new Float64Array(this._levels);
			this._sumNodeDimensionSq = new Float64Array(this._levels);

			this._nodeMaxDimension = new Float32Array(this._progressTotal);
			this._captureNodeProfiles = this.options.captureNodeDimensionProfiles === true
				|| Boolean(this.options.outNodeDimensionLevelsAttribute);
			this._nodeDimensionLevels = this._captureNodeProfiles
				? new Float32Array(this._progressTotal * this._levels)
				: null;

			this._topologyBaseline = network.getTopologyVersions();
		}

		static _collectSelectedNodes(network, nodes) {
			if (!nodes) {
				return network.withBufferAccess(() => network.nodeIndices.slice(), { nodeIndices: true });
			}

			const source = ArrayBuffer.isView(nodes) ? nodes : Array.from(nodes);
			const out = [];
			for (let i = 0; i < source.length; i += 1) {
				const raw = Number(source[i]);
				if (!Number.isFinite(raw) || raw < 0) {
					continue;
				}
				const node = raw >>> 0;
				if (network.hasNodeIndex(node)) {
					out.push(node);
				}
			}
			return Uint32Array.from(out);
		}

		static _estimateDimensionFromSeries(series, capacityMaxLevel, radius, method, order) {
			if (!series || radius > capacityMaxLevel || !(series[radius] > 0)) {
				return 0;
			}

			if (method === DimensionDifferenceMethod.LeastSquares) {
				let sumXY = 0;
				let sumX = 0;
				let sumY = 0;
				let sumXX = 0;
				let count = 0;
				if (radius > order) {
					for (let offset = -order; offset <= order; offset += 1) {
						const h = radius + offset;
						if (h <= 0 || h > capacityMaxLevel) {
							continue;
						}
						const v = series[h];
						if (!(v > 0)) {
							continue;
						}
						const x = Math.log(h);
						const y = Math.log(v);
						sumXY += y * x;
						sumX += x;
						sumY += y;
						sumXX += x * x;
						count += 1;
					}
				}
				const denom = count * sumXX - sumX * sumX;
				if (!(denom !== 0 && Number.isFinite(denom))) {
					return 0;
				}
				const slope = (count * sumXY - sumX * sumY) / denom;
				return Number.isFinite(slope) ? slope : 0;
			}

			if (method === DimensionDifferenceMethod.Forward) {
				if (radius + order > capacityMaxLevel) {
					return 0;
				}
				const coefficients = DIMENSION_FORWARD_COEFFICIENTS[order - 1];
				let derivative = 0;
				for (let offset = 0; offset <= order; offset += 1) {
					const r = radius + offset;
					if (r <= 0) {
						continue;
					}
					derivative += coefficients[offset] * series[r];
				}
				const value = (derivative * radius) / series[radius];
				return Number.isFinite(value) ? value : 0;
			}

			if (method === DimensionDifferenceMethod.Backward) {
				const coefficients = DIMENSION_FORWARD_COEFFICIENTS[order - 1];
				let derivative = 0;
				for (let offset = 0; offset <= order; offset += 1) {
					if (offset > radius) {
						continue;
					}
					const r = radius - offset;
					if (r <= 0) {
						continue;
					}
					derivative += (-coefficients[offset]) * series[r];
				}
				const value = (derivative * radius) / series[radius];
				return Number.isFinite(value) ? value : 0;
			}

			if (radius + order > capacityMaxLevel) {
				return 0;
			}
			const coefficients = DIMENSION_CENTRAL_COEFFICIENTS[order - 1];
			let derivative = 0;
			for (let offset = 1; offset <= order; offset += 1) {
				if (offset <= radius) {
					const rb = radius - offset;
					if (rb > 0) {
						derivative += (-coefficients[offset - 1]) * series[rb];
					}
				}
				const rf = radius + offset;
				if (rf > 0 && rf <= capacityMaxLevel) {
					derivative += coefficients[offset - 1] * series[rf];
				}
			}
			const value = (derivative * radius) / series[radius];
			return Number.isFinite(value) ? value : 0;
		}

		static _normalizeLevelsEncoding(value) {
			if (typeof value !== 'string') {
				return 'vector';
			}
			const normalized = value.trim().toLowerCase();
			if (normalized === 'string' || normalized === 'json' || normalized === 'csv') {
				return 'string';
			}
			if (normalized === 'vector' || normalized === 'array' || normalized === 'numeric') {
				return 'vector';
			}
			return 'vector';
		}

		_ensureActive() {
			if (this._canceledReason) {
				throw new Error(`Session canceled: ${this._canceledReason}`);
			}
			if (this._disposed) {
				throw new Error('Session has been disposed');
			}
			this.network._ensureActive();
		}

		_checkCancellation() {
			const baseline = this._topologyBaseline;
			if (!baseline) {
				return;
			}
			const current = this.network.getTopologyVersions();
			if (current.node !== baseline.node || current.edge !== baseline.edge) {
				this.cancel('network topology changed');
				throw new Error('Session canceled: network topology changed');
			}
		}

		_progressObject() {
			return {
				phase: this._phase,
				progressCurrent: this._progressCurrent,
				progressTotal: this._progressTotal,
				processedNodes: this._progressCurrent,
				nodeCount: this._progressTotal,
				maxLevel: this._maxLevel,
				method: this._method,
				order: this._order,
			};
		}

		_runChunk(budget) {
			const remaining = this._progressTotal - this._progressCurrent;
			const count = Math.min(Math.max(1, budget >>> 0), remaining);
			for (let i = 0; i < count; i += 1) {
				const idx = this._progressCurrent;
				const node = this._selectedNodes[idx];
				const local = this.network.measureNodeDimension(node, {
					maxLevel: this._capacityMaxLevel,
					method: this._method,
					order: this._order,
				});
				const { capacity, dimension } = local;
				for (let r = 0; r <= this._capacityMaxLevel; r += 1) {
					const c = Number(capacity[r] ?? 0);
					this._sumCapacity[r] += c;
				}
				let localMax = Number.NEGATIVE_INFINITY;
				const profileBase = this._nodeDimensionLevels ? (idx * this._levels) : 0;
				for (let r = 0; r <= this._maxLevel; r += 1) {
					const d = Number(dimension[r] ?? 0);
					this._sumNodeDimension[r] += d;
					this._sumNodeDimensionSq[r] += d * d;
					if (d > localMax) {
						localMax = d;
					}
					if (this._nodeDimensionLevels) {
						this._nodeDimensionLevels[profileBase + r] = d;
					}
				}
				this._nodeMaxDimension[idx] = Number.isFinite(localMax) ? localMax : 0;
				this._progressCurrent += 1;
			}
			if (this._progressCurrent >= this._progressTotal) {
				this._phase = 5;
			} else if (this._phase === 0) {
				this._phase = 1;
			}
		}

		cancel(reason = 'canceled') {
			if (this._canceledReason) {
				return;
			}
			this._canceledReason = String(reason || 'canceled');
			this._phase = 6;
			this.dispose();
		}

		dispose() {
			this._disposed = true;
		}

		getProgress() {
			this._ensureActive();
			this._checkCancellation();
			return this._progressObject();
		}

		step(options = {}) {
			this._ensureActive();
			this._checkCancellation();
			if (this._phase === 5) {
				return this._progressObject();
			}

			const hasTimeout = Object.prototype.hasOwnProperty.call(options, 'timeoutMs');
			const budget = options.budget == null ? 8 : (options.budget >>> 0);
			const timeoutMs = hasTimeout ? options.timeoutMs : 4;
			const chunkBudget = options.chunkBudget == null ? 8 : (options.chunkBudget >>> 0);
			const now = typeof globalThis !== 'undefined' && globalThis.performance && typeof globalThis.performance.now === 'function'
				? globalThis.performance.now.bind(globalThis.performance)
				: Date.now;

			try {
				if (timeoutMs == null) {
					this._runChunk(Math.max(1, budget));
					return this._progressObject();
				}
				const deadline = now() + Math.max(0, Number(timeoutMs) || 0);
				const perChunk = Math.max(1, chunkBudget);
				do {
					this._runChunk(perChunk);
					if (this._phase === 5) {
						break;
					}
				} while (now() < deadline);
				return this._progressObject();
			} catch (error) {
				this._phase = 6;
				throw error;
			}
		}

		async run(options = {}) {
			this._ensureActive();
			const {
				stepOptions = {},
				yieldMs = 0,
				yield: yieldFn = null,
				onProgress = null,
				signal = null,
				maxIterations = Infinity,
			} = options;
			const defer = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

			let last = this.getProgress();
			for (let iteration = 0; iteration < maxIterations; iteration += 1) {
				if (signal?.aborted) {
					this.cancel('aborted');
					throw new Error('Session canceled: aborted');
				}
				if (this._phase === 5) {
					return last;
				}
				last = this.step(stepOptions);
				if (typeof onProgress === 'function') {
					onProgress(last);
				}
				if (this._phase === 5) {
					return last;
				}
				if (typeof yieldFn === 'function') {
					await yieldFn(last);
				} else {
					await defer(Math.max(0, Number(yieldMs) || 0));
				}
			}
			return last;
		}

		isComplete() {
			return this._phase === 5;
		}

		isFailed() {
			return this._phase === 6;
		}

		isFinalized() {
			return this._finalized;
		}

		_ensureNodeFloatAttribute(name, dimension) {
			if (!this.network.hasNodeAttribute(name)) {
				this.network.defineNodeAttribute(name, AttributeType.Float, dimension);
				return;
			}
			const info = this.network.getNodeAttributeInfo(name);
			if (!info || info.type !== AttributeType.Float || info.dimension !== dimension) {
				throw new Error(`Node attribute "${name}" must be Float (dimension ${dimension})`);
			}
		}

		_ensureNodeStringAttribute(name) {
			if (!this.network.hasNodeAttribute(name)) {
				this.network.defineNodeAttribute(name, AttributeType.String, 1);
				return;
			}
			const info = this.network.getNodeAttributeInfo(name);
			if (!info || info.type !== AttributeType.String || info.dimension !== 1) {
				throw new Error(`Node attribute "${name}" must be String (dimension 1)`);
			}
		}

		_writeNodeMaxDimensionAttribute(name) {
			this._ensureNodeFloatAttribute(name, 1);
			this.network.withBufferAccess(() => {
				const { view, bumpVersion } = this.network.getNodeAttributeBuffer(name);
				for (let i = 0; i < this._selectedNodes.length; i += 1) {
					view[this._selectedNodes[i]] = this._nodeMaxDimension[i];
				}
				bumpVersion();
			});
		}

		_writeNodeDimensionLevelsVectorAttribute(name) {
			if (!this._nodeDimensionLevels) {
				throw new Error('Session did not capture per-level dimensions. Set captureNodeDimensionProfiles: true when creating the session.');
			}
			this._ensureNodeFloatAttribute(name, this._levels);
			this.network.withBufferAccess(() => {
				const { view, bumpVersion } = this.network.getNodeAttributeBuffer(name);
				for (let i = 0; i < this._selectedNodes.length; i += 1) {
					const nodeBase = this._selectedNodes[i] * this._levels;
					const localBase = i * this._levels;
					for (let r = 0; r < this._levels; r += 1) {
						view[nodeBase + r] = this._nodeDimensionLevels[localBase + r];
					}
				}
				bumpVersion();
			});
		}

		_writeNodeDimensionLevelsStringAttribute(name, precision) {
			if (!this._nodeDimensionLevels) {
				throw new Error('Session did not capture per-level dimensions. Set captureNodeDimensionProfiles: true when creating the session.');
			}
			this._ensureNodeStringAttribute(name);
			for (let i = 0; i < this._selectedNodes.length; i += 1) {
				const localBase = i * this._levels;
				const values = new Array(this._levels);
				for (let r = 0; r < this._levels; r += 1) {
					const v = this._nodeDimensionLevels[localBase + r];
					values[r] = Number.isFinite(v) ? Number(v.toFixed(precision)) : 0;
				}
				this.network.setNodeStringAttribute(name, this._selectedNodes[i], JSON.stringify(values));
			}
		}

		_computeFinalResult() {
			const selectedCount = this._progressTotal;
			const averageCapacity = new Float32Array(this._levels);
			const globalDimension = new Float32Array(this._levels);
			const averageNodeDimension = new Float32Array(this._levels);
			const nodeDimensionStddev = new Float32Array(this._levels);
			const averageCapacitySeries = new Float64Array(this._capacityLevels);

			for (let r = 0; r <= this._capacityMaxLevel; r += 1) {
				const avgCapacity = this._sumCapacity[r] / selectedCount;
				averageCapacitySeries[r] = avgCapacity;
				if (r <= this._maxLevel) {
					averageCapacity[r] = avgCapacity;
				}
			}

			for (let r = 0; r <= this._maxLevel; r += 1) {
				const avgNode = this._sumNodeDimension[r] / selectedCount;
				const avgSqNode = this._sumNodeDimensionSq[r] / selectedCount;
				const variance = Math.max(0, avgSqNode - avgNode * avgNode);

				averageNodeDimension[r] = avgNode;
				nodeDimensionStddev[r] = Math.sqrt(variance);
			}

			globalDimension[0] = 0;
			for (let r = 1; r <= this._maxLevel; r += 1) {
				globalDimension[r] = DimensionSession._estimateDimensionFromSeries(
					averageCapacitySeries,
					this._capacityMaxLevel,
					r,
					this._method,
					this._order
				);
			}

			return {
				selectedCount,
				averageCapacity,
				globalDimension,
				averageNodeDimension,
				nodeDimensionStddev,
				maxLevel: this._maxLevel,
				method: this._method,
				order: this._order,
			};
		}

		finalize(options = {}) {
			this._ensureActive();
			this._checkCancellation();
			if (this._phase !== 5) {
				throw new Error('Dimension session is not ready to finalize (run step() until done)');
			}
			if (this._finalized) {
				return this._finalResult;
			}

			const outNodeMaxDimensionAttribute = options.outNodeMaxDimensionAttribute
				?? this.options.outNodeMaxDimensionAttribute
				?? null;
			const outNodeDimensionLevelsAttribute = options.outNodeDimensionLevelsAttribute
				?? this.options.outNodeDimensionLevelsAttribute
				?? null;
			const dimensionLevelsEncoding = DimensionSession._normalizeLevelsEncoding(
				options.dimensionLevelsEncoding
				?? this.options.dimensionLevelsEncoding
				?? 'vector'
			);
			const precisionRaw = options.dimensionLevelsStringPrecision
				?? this.options.dimensionLevelsStringPrecision
				?? 6;
			const precision = Math.max(0, Math.min(12, precisionRaw | 0));

			if (outNodeMaxDimensionAttribute) {
				this._writeNodeMaxDimensionAttribute(String(outNodeMaxDimensionAttribute));
			}
			if (outNodeDimensionLevelsAttribute) {
				if (dimensionLevelsEncoding === 'string') {
					this._writeNodeDimensionLevelsStringAttribute(String(outNodeDimensionLevelsAttribute), precision);
				} else {
					this._writeNodeDimensionLevelsVectorAttribute(String(outNodeDimensionLevelsAttribute));
				}
			}

			this._finalResult = this._computeFinalResult();
			this._finalized = true;
			return this._finalResult;
		}
	}

class LeidenSession {
		constructor(module, network, ptr, options) {
			this.options = options;
			this._finalized = false;
			const cancelOn = {
				topology: 'both',
				attributes: {
					edge: options.edgeWeightAttribute ? [options.edgeWeightAttribute] : [],
				},
			};
			const workerSpec = {
				kind: 'leiden',
				buildSnapshot: () => {
					const sessionOptions = this.options ?? {};
					return network.withBufferAccess(() => {
						const activeNodes = network.nodeIndices;
						const nodeToCompact = new Map();
						for (let i = 0; i < activeNodes.length; i += 1) {
							nodeToCompact.set(activeNodes[i], i);
						}

						const edgeIds = network.edgeIndices;
						const edgePairs = new Uint32Array(edgeIds.length * 2);
						const edgeWeightAttribute = sessionOptions.edgeWeightAttribute ?? null;
						const edgeWeights = edgeWeightAttribute ? new Float64Array(edgeIds.length) : null;
						const weightView = edgeWeightAttribute ? network.getEdgeAttributeBuffer(edgeWeightAttribute).view : null;
						const edgesView = network.edgesView;
						for (let i = 0; i < edgeIds.length; i += 1) {
							const edgeId = edgeIds[i];
							const base = edgeId * 2;
							const u = edgesView[base];
							const v = edgesView[base + 1];
							const cu = nodeToCompact.get(u);
							const cv = nodeToCompact.get(v);
							if (cu == null || cv == null) {
								throw new Error('Encountered edge endpoint for inactive node during worker snapshot');
							}
							edgePairs[i * 2] = cu >>> 0;
							edgePairs[i * 2 + 1] = cv >>> 0;
							if (edgeWeights && weightView) {
								const raw = weightView[edgeId];
								edgeWeights[i] = typeof raw === 'bigint' ? Number(raw) : Number(raw ?? 0);
							}
						}
						return {
							directed: Boolean(network.directed),
							activeNodes: activeNodes.slice(),
							edgePairs,
							edgeWeights,
							edgeWeightAttribute,
							resolution: sessionOptions.resolution ?? 1,
							seed: sessionOptions.seed ?? 0,
							maxLevels: sessionOptions.maxLevels ?? 32,
							maxPasses: sessionOptions.passes ?? sessionOptions.maxPasses ?? 8,
							outNodeCommunityAttribute: sessionOptions.outNodeCommunityAttribute ?? 'community',
							categoricalCommunities: sessionOptions.categoricalCommunities !== false,
						};
					}, { nodeIndices: true, edgeIndices: true, edgesView: true });
				},
				buildPayload: (snapshot) => {
					const payload = {
						directed: snapshot.directed,
						nodeCount: snapshot.activeNodes.length >>> 0,
						edgePairsBuffer: snapshot.edgePairs.buffer,
						edgeWeightsBuffer: snapshot.edgeWeights ? snapshot.edgeWeights.buffer : null,
						edgeWeightAttribute: snapshot.edgeWeightAttribute,
						resolution: snapshot.resolution,
						seed: snapshot.seed,
						maxLevels: snapshot.maxLevels,
						maxPasses: snapshot.maxPasses,
						outNodeCommunityAttribute: snapshot.outNodeCommunityAttribute,
					};
					const transfer = [snapshot.edgePairs.buffer];
					if (snapshot.edgeWeights) transfer.push(snapshot.edgeWeights.buffer);
					return { payload, transfer };
				},
				applyResult: (result, snapshot) => {
					const outName = snapshot.outNodeCommunityAttribute;
					const categorical = snapshot.categoricalCommunities !== false;
					if (!outName) {
						throw new Error('outNodeCommunityAttribute is required');
					}
					if (!network._nodeAttributes.has(outName)) {
						network.defineNodeAttribute(outName, categorical ? AttributeType.Category : AttributeType.UnsignedInteger, 1);
					} else {
						const meta = network._nodeAttributes.get(outName);
						if (meta) {
							if (categorical && meta.type !== AttributeType.Category) {
								throw new Error(`Node attribute "${outName}" must be Category`);
							}
							if (!categorical && meta.type !== AttributeType.UnsignedInteger) {
								throw new Error(`Node attribute "${outName}" must be UnsignedInteger`);
							}
						}
					}

					const communities = new Uint32Array(result.communitiesBuffer);
					if (communities.length !== snapshot.activeNodes.length) {
						throw new Error('Worker community payload does not match snapshot node count');
					}
					network.withBufferAccess(() => {
						const { view, bumpVersion } = network.getNodeAttributeBuffer(outName);
						view.fill(0);
						for (let i = 0; i < snapshot.activeNodes.length; i += 1) {
							view[snapshot.activeNodes[i]] = communities[i];
						}
						bumpVersion();
					});
					this._finalized = true;
				},
			};
			const handlers = {
				scratchBytes: 80,
				destroy: typeof module._CXLeidenSessionDestroy === 'function' ? module._CXLeidenSessionDestroy.bind(module) : null,
				step: typeof module._CXLeidenSessionStep === 'function' ? module._CXLeidenSessionStep.bind(module) : null,
				isTerminalPhase: (phase) => phase === 5 || phase === 6,
				isDonePhase: (phase) => phase === 5,
				isFailedPhase: (phase) => phase === 6,
				cancelOn,
				workerSpec,
				getProgress: (sessionPtr, scratchPtr) => {
					if (typeof module._CXLeidenSessionGetProgress !== 'function') {
						throw new Error('CXLeidenSessionGetProgress is not available in this WASM build.');
					}
					const base = scratchPtr;
					const progressCurrentPtr = base + 0; // f64
					const progressTotalPtr = base + 8; // f64
					const phasePtr = base + 16; // u32
					const levelPtr = base + 20; // u32
					const maxLevelsPtr = base + 24; // u32
					const passPtr = base + 28; // u32
					const maxPassesPtr = base + 32; // u32
					const visitedPtr = base + 36; // u32
					const nodeCountPtr = base + 40; // u32
					const communityCountPtr = base + 44; // u32

					module._CXLeidenSessionGetProgress(
						sessionPtr,
						progressCurrentPtr,
						progressTotalPtr,
						phasePtr,
						levelPtr,
						maxLevelsPtr,
						passPtr,
						maxPassesPtr,
					visitedPtr,
						nodeCountPtr,
						communityCountPtr
					);

					const progressCurrent = module.HEAPF64[progressCurrentPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
					const progressTotal = module.HEAPF64[progressTotalPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
					const phase = module.HEAPU32[phasePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
					const level = module.HEAPU32[levelPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
					const maxLevels = module.HEAPU32[maxLevelsPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
					const pass = module.HEAPU32[passPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
				const maxPasses = module.HEAPU32[maxPassesPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
				const visitedThisPass = module.HEAPU32[visitedPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
				const nodeCount = module.HEAPU32[nodeCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
					const communityCount = module.HEAPU32[communityCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;

						return {
							progressCurrent,
							progressTotal,
							phase,
							level,
							maxLevels,
							pass,
							maxPasses,
						visitedThisPass,
						nodeCount,
						communityCount,
					};
				},
			};

			this._base = new WasmSteppableSession(module, network, ptr, handlers);
		}

	_ensureActive() {
		this._base._ensureActive();
	}

	_ensureScratch() {
		return this._base._ensureScratch();
	}

	dispose() {
		this._base.dispose();
	}

		getProgress() {
			return this._base.getProgress();
		}

		isComplete() {
			const { phase } = this.getProgress();
			return phase === 5;
		}

		isFinalized() {
			return this._finalized;
		}

		step(options = {}) {
			return this._base.step(options);
		}

		run(options = {}) {
			return this._base.run(options);
		}

		async runWorker(options = {}) {
			const { outNodeCommunityAttribute, categoricalCommunities, ...runnerOptions } = options ?? {};
			if (outNodeCommunityAttribute) {
				this.options = { ...this.options, outNodeCommunityAttribute: String(outNodeCommunityAttribute) };
			}
			if (categoricalCommunities != null) {
				this.options = { ...this.options, categoricalCommunities: Boolean(categoricalCommunities) };
			}
			try {
				return await this._base.runWorker(runnerOptions);
			} finally {
				this.dispose();
			}
		}

		finalize(options = {}) {
			this._ensureActive();
			const module = this._base.module;
			const network = this._base.network;
			const ptr = this._base.ptr;
		network._assertCanAllocate('Leiden community detection finalization');
		if (typeof module._CXLeidenSessionFinalize !== 'function') {
			throw new Error('CXLeidenSessionFinalize is not available in this WASM build.');
		}

		const outNodeCommunityAttribute = options.outNodeCommunityAttribute ?? this.options?.outNodeCommunityAttribute ?? 'community';
		const categoricalCommunities = options.categoricalCommunities ?? this.options?.categoricalCommunities ?? true;
		if (!outNodeCommunityAttribute) {
			throw new Error('outNodeCommunityAttribute is required');
		}

		const outName = new CString(module, outNodeCommunityAttribute);
		const modularityPtr = module._malloc(Float64Array.BYTES_PER_ELEMENT);
		const communityCountPtr = module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		if (!modularityPtr || !communityCountPtr) {
			outName.dispose();
			if (modularityPtr) module._free(modularityPtr);
			if (communityCountPtr) module._free(communityCountPtr);
			throw new Error('Failed to allocate Leiden finalize buffers');
		}
		module.HEAPF64[modularityPtr / Float64Array.BYTES_PER_ELEMENT] = 0;
		module.HEAPU32[communityCountPtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

		let ok = 0;
		let modularity = 0;
		let communityCount = 0;
		try {
			ok = module._CXLeidenSessionFinalize(ptr, outName.ptr, modularityPtr, communityCountPtr);
			modularity = module.HEAPF64[modularityPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
			communityCount = module.HEAPU32[communityCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		} finally {
			outName.dispose();
			module._free(modularityPtr);
			module._free(communityCountPtr);
		}
			if (!ok) {
				throw new Error('Leiden session is not ready to finalize (run step() until done)');
			}
			this._finalized = true;

			if (!network._nodeAttributes.has(outNodeCommunityAttribute)) {
				network._nodeAttributes.set(outNodeCommunityAttribute, {
					type: categoricalCommunities ? AttributeType.Category : AttributeType.UnsignedInteger,
					dimension: 1,
					complex: false,
					jsStore: new Map(),
					stringPointers: new Map(),
					nextHandle: 1,
				});
			} else {
				const meta = network._nodeAttributes.get(outNodeCommunityAttribute);
				if (meta) {
					if (categoricalCommunities && meta.type !== AttributeType.Category) {
						throw new Error(`Node attribute "${outNodeCommunityAttribute}" must be Category`);
					}
					if (!categoricalCommunities && meta.type !== AttributeType.UnsignedInteger) {
						throw new Error(`Node attribute "${outNodeCommunityAttribute}" must be UnsignedInteger`);
					}
				}
			}

		return { communityCount, modularity };
	}
}

class ConnectedComponentsSession {
	constructor(module, network, ptr, options = {}) {
		this.options = options;
		this._finalized = false;
		const handlers = {
			scratchBytes: 48,
			destroy: typeof module._CXConnectedComponentsSessionDestroy === 'function'
				? module._CXConnectedComponentsSessionDestroy.bind(module)
				: null,
			step: typeof module._CXConnectedComponentsSessionStep === 'function'
				? module._CXConnectedComponentsSessionStep.bind(module)
				: null,
			isTerminalPhase: (phase) => phase === 3 || phase === 4,
			isDonePhase: (phase) => phase === 3,
			isFailedPhase: (phase) => phase === 4,
			cancelOn: { topology: 'both' },
			getProgress: (sessionPtr, scratchPtr) => {
				if (typeof module._CXConnectedComponentsSessionGetProgress !== 'function') {
					throw new Error('CXConnectedComponentsSessionGetProgress is not available in this WASM build.');
				}
				const base = scratchPtr;
				const progressCurrentPtr = base + 0; // f64
				const progressTotalPtr = base + 8; // f64
				const phasePtr = base + 16; // u32
				const visitedNodesPtr = base + 20; // u32
				const activeNodesPtr = base + 24; // u32
				const componentCountPtr = base + 28; // u32
				const largestComponentSizePtr = base + 32; // u32

				module._CXConnectedComponentsSessionGetProgress(
					sessionPtr,
					progressCurrentPtr,
					progressTotalPtr,
					phasePtr,
					visitedNodesPtr,
					activeNodesPtr,
					componentCountPtr,
					largestComponentSizePtr
				);

				return {
					progressCurrent: module.HEAPF64[progressCurrentPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0,
					progressTotal: module.HEAPF64[progressTotalPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0,
					phase: module.HEAPU32[phasePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					visitedNodes: module.HEAPU32[visitedNodesPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					activeNodes: module.HEAPU32[activeNodesPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					componentCount: module.HEAPU32[componentCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					largestComponentSize: module.HEAPU32[largestComponentSizePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
				};
			},
		};

		this._base = new WasmSteppableSession(module, network, ptr, handlers);
	}

	_ensureActive() {
		this._base._ensureActive();
	}

	dispose() {
		this._base.dispose();
	}

	getProgress() {
		return this._base.getProgress();
	}

	isComplete() {
		const { phase } = this.getProgress();
		return phase === 3;
	}

	isFinalized() {
		return this._finalized;
	}

	step(options = {}) {
		return this._base.step(options);
	}

	run(options = {}) {
		return this._base.run(options);
	}

	finalize(options = {}) {
		this._ensureActive();
		const module = this._base.module;
		const network = this._base.network;
		const ptr = this._base.ptr;
		network._assertCanAllocate('connected components finalization');
		if (typeof module._CXConnectedComponentsSessionFinalize !== 'function') {
			throw new Error('CXConnectedComponentsSessionFinalize is not available in this WASM build.');
		}

		const outNodeComponentAttribute = options.outNodeComponentAttribute
			?? this.options?.outNodeComponentAttribute
			?? null;
		const output = network._resolveNodeMetricOutputAttribute(
			outNodeComponentAttribute,
			AttributeType.UnsignedInteger,
			'UnsignedInteger'
		);
		const outPtr = output?.pointer ?? module._malloc(network.nodeCapacity * Uint32Array.BYTES_PER_ELEMENT);
		const componentCountPtr = module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		const largestComponentSizePtr = module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		if (!outPtr || !componentCountPtr || !largestComponentSizePtr) {
			if (!output && outPtr) module._free(outPtr);
			if (componentCountPtr) module._free(componentCountPtr);
			if (largestComponentSizePtr) module._free(largestComponentSizePtr);
			throw new Error('Failed to allocate connected components finalize buffers');
		}
		module.HEAPU32[componentCountPtr / Uint32Array.BYTES_PER_ELEMENT] = 0;
		module.HEAPU32[largestComponentSizePtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

		let ok = 0;
		let componentCount = 0;
		let largestComponentSize = 0;
		let valuesByNode;
		try {
			ok = module._CXConnectedComponentsSessionFinalize(
				ptr,
				outPtr,
				network.nodeCapacity >>> 0,
				componentCountPtr,
				largestComponentSizePtr
			);
			componentCount = module.HEAPU32[componentCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
			largestComponentSize = module.HEAPU32[largestComponentSizePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
			valuesByNode = network._copyUint32NodeValuesFromPointer(outPtr);
		} finally {
			if (!output) {
				module._free(outPtr);
			}
			module._free(componentCountPtr);
			module._free(largestComponentSizePtr);
		}
		if (!ok) {
			throw new Error('Connected-components session is not ready to finalize (run step() until done)');
		}

		if (output) {
			network._bumpAttributeVersion('node', output.name, { op: 'set' });
		}

		this._finalized = true;
		const result = network._collectNodeMetricResultUint32(valuesByNode, options.nodes ?? this.options?.nodes ?? null);
		return { ...result, componentCount, largestComponentSize };
	}
}

class CorenessSession {
	constructor(module, network, ptr, options = {}) {
		this.options = options;
		this._finalized = false;
		const handlers = {
			scratchBytes: 40,
			destroy: typeof module._CXCorenessSessionDestroy === 'function'
				? module._CXCorenessSessionDestroy.bind(module)
				: null,
			step: typeof module._CXCorenessSessionStep === 'function'
				? module._CXCorenessSessionStep.bind(module)
				: null,
			isTerminalPhase: (phase) => phase === 3 || phase === 4,
			isDonePhase: (phase) => phase === 3,
			isFailedPhase: (phase) => phase === 4,
			cancelOn: { topology: 'both' },
			getProgress: (sessionPtr, scratchPtr) => {
				if (typeof module._CXCorenessSessionGetProgress !== 'function') {
					throw new Error('CXCorenessSessionGetProgress is not available in this WASM build.');
				}
				const base = scratchPtr;
				const progressCurrentPtr = base + 0; // f64
				const progressTotalPtr = base + 8; // f64
				const phasePtr = base + 16; // u32
				const peeledNodesPtr = base + 20; // u32
				const activeNodesPtr = base + 24; // u32
				const currentCorePtr = base + 28; // u32
				const maxCorePtr = base + 32; // u32

				module._CXCorenessSessionGetProgress(
					sessionPtr,
					progressCurrentPtr,
					progressTotalPtr,
					phasePtr,
					peeledNodesPtr,
					activeNodesPtr,
					currentCorePtr,
					maxCorePtr
				);

				return {
					progressCurrent: module.HEAPF64[progressCurrentPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0,
					progressTotal: module.HEAPF64[progressTotalPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0,
					phase: module.HEAPU32[phasePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					peeledNodes: module.HEAPU32[peeledNodesPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					activeNodes: module.HEAPU32[activeNodesPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					currentCore: module.HEAPU32[currentCorePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
					maxCore: module.HEAPU32[maxCorePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0,
				};
			},
		};

		this._base = new WasmSteppableSession(module, network, ptr, handlers);
	}

	_ensureActive() {
		this._base._ensureActive();
	}

	dispose() {
		this._base.dispose();
	}

	getProgress() {
		return this._base.getProgress();
	}

	isComplete() {
		const { phase } = this.getProgress();
		return phase === 3;
	}

	isFinalized() {
		return this._finalized;
	}

	step(options = {}) {
		return this._base.step(options);
	}

	run(options = {}) {
		return this._base.run(options);
	}

	finalize(options = {}) {
		this._ensureActive();
		const module = this._base.module;
		const network = this._base.network;
		const ptr = this._base.ptr;
		network._assertCanAllocate('coreness finalization');
		if (typeof module._CXCorenessSessionFinalize !== 'function') {
			throw new Error('CXCorenessSessionFinalize is not available in this WASM build.');
		}

		const outNodeCorenessAttribute = options.outNodeCorenessAttribute
			?? this.options?.outNodeCorenessAttribute
			?? null;
		const output = network._resolveNodeMetricOutputAttribute(
			outNodeCorenessAttribute,
			AttributeType.UnsignedInteger,
			'UnsignedInteger'
		);
		const outPtr = output?.pointer ?? module._malloc(network.nodeCapacity * Uint32Array.BYTES_PER_ELEMENT);
		const maxCorePtr = module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		if (!outPtr || !maxCorePtr) {
			if (!output && outPtr) module._free(outPtr);
			if (maxCorePtr) module._free(maxCorePtr);
			throw new Error('Failed to allocate coreness finalize buffers');
		}
		module.HEAPU32[maxCorePtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

		let ok = 0;
		let maxCore = 0;
		let valuesByNode;
		try {
			ok = module._CXCorenessSessionFinalize(
				ptr,
				outPtr,
				network.nodeCapacity >>> 0,
				maxCorePtr
			);
			maxCore = module.HEAPU32[maxCorePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
			valuesByNode = network._copyUint32NodeValuesFromPointer(outPtr);
		} finally {
			if (!output) {
				module._free(outPtr);
			}
			module._free(maxCorePtr);
		}
		if (!ok) {
			throw new Error('Coreness session is not ready to finalize (run step() until done)');
		}

		if (output) {
			network._bumpAttributeVersion('node', output.name, { op: 'set' });
		}

		this._finalized = true;
		const result = network._collectNodeMetricResultUint32(valuesByNode, options.nodes ?? this.options?.nodes ?? null);
		return {
			...result,
			direction: this.options?.direction ?? NeighborDirection.Both,
			executionMode: this.options?.executionMode ?? MeasurementExecutionMode.SingleThread,
			maxCore,
		};
	}
}

/**
 * High-level JavaScript wrapper around the Helios WASM network implementation.
 * Manages lifetime, attribute registration, and buffer views.
 */
export class HeliosNetwork extends BaseEventTarget {
	/**
	 * Asynchronously constructs and initializes a network instance.
	 *
	 * @param {object} [options]
	 * @param {boolean} [options.directed=false] - Whether the graph enforces edge direction.
	 * @param {number} [options.initialNodes=0] - Nodes to pre-allocate and populate.
	 * @param {number} [options.initialEdges=0] - Edge capacity to pre-allocate.
	 * @param {object} [options.module] - Pre-initialized WASM module to reuse.
	 * @returns {Promise<HeliosNetwork>} Ready-to-use network.
	 * @example
	 * import HeliosNetwork from 'helios-network';
	 *
	 * const network = await HeliosNetwork.create({ initialNodes: 2 });
	 * console.log(network.nodeCount); // 2
	 * network.dispose();
	 */
	static async create(options = {}) {
		const {
			directed = false,
			initialNodes = 0,
			initialEdges = 0,
			module: providedModule,
		} = options;
		const module = providedModule || await getModule();
		moduleInstance = module;
		return HeliosNetwork._createWithModule(module, directed, initialNodes, initialEdges);
	}

	/**
	 * Synchronously constructs a network instance using an already-initialized module.
	 *
	 * Either provide the `module` option explicitly or ensure that the module
	 * was previously initialized through `await HeliosNetwork.create(...)` or
	 * `await getHeliosModule(...)`.
	 *
	 * @param {object} [options]
	 * @param {boolean} [options.directed=false] - Whether the graph enforces edge direction.
	 * @param {number} [options.initialNodes=0] - Nodes to pre-allocate and populate.
	 * @param {number} [options.initialEdges=0] - Edge capacity to pre-allocate.
	 * @param {object} [options.module] - Pre-initialized WASM module to reuse.
	 * @returns {HeliosNetwork} Ready-to-use network.
	 * @throws {Error} When the module has not been initialized yet.
	 * @example
	 * import HeliosNetwork from 'helios-network';
	 *
	 * const boot = await HeliosNetwork.create();
	 * boot.dispose(); // primes the WASM module once
	 *
	 * const network = HeliosNetwork.createSync({ initialNodes: 1 });
	 * console.log(network.nodeCount); // 1
	 * network.dispose();
	 */
	static createSync(options = {}) {
		const {
			directed = false,
			initialNodes = 0,
			initialEdges = 0,
			module: providedModule,
		} = options;
		const module = providedModule || moduleInstance;
		if (!module) {
			throw new Error('Helios WASM module is not initialized. Await HeliosNetwork.create() or getHeliosModule() first, or pass the module explicitly.');
		}
		moduleInstance = module;
		return HeliosNetwork._createWithModule(module, directed, initialNodes, initialEdges);
	}

	/**
	 * Returns the package version string for the helios-network module.
	 *
	 * @returns {string}
	 */
	static getPackageVersion() {
		return PACKAGE_VERSION;
	}

	/**
	 * Returns the package version string for the helios-network module.
	 *
	 * @returns {string}
	 */
	getPackageVersion() {
		return HeliosNetwork.getPackageVersion();
	}

	/**
	 * Hydrates a network instance from a `.bxnet` container.
	 *
	 * @param {Uint8Array|ArrayBuffer|string|Blob|Response} source - Serialized payload or Node file path.
	 * @param {object} [options]
	 * @param {object} [options.module] - Optional WASM module to reuse.
	 * @returns {Promise<HeliosNetwork>} Newly constructed network.
	 */
	static async fromBXNet(source, options = {}) {
		return HeliosNetwork._fromSerialized(source, 'bxnet', options);
	}

	/**
	 * Hydrates a network instance from a BGZF-compressed `.zxnet` container.
	 *
	 * @param {Uint8Array|ArrayBuffer|string|Blob|Response} source - Serialized payload or Node file path.
	 * @param {object} [options]
	 * @param {object} [options.module] - Optional WASM module to reuse.
	 * @returns {Promise<HeliosNetwork>} Newly constructed network.
	 */
	static async fromZXNet(source, options = {}) {
		return HeliosNetwork._fromSerialized(source, 'zxnet', options);
	}

	/**
	 * Hydrates a network instance from a human-readable `.xnet` container.
	 *
	 * @param {Uint8Array|ArrayBuffer|string|Blob|Response} source - Serialized payload or Node file path.
	 * @param {object} [options]
	 * @param {object} [options.module] - Optional WASM module to reuse.
	 * @returns {Promise<HeliosNetwork>} Newly constructed network.
	 */
	static async fromXNet(source, options = {}) {
		return HeliosNetwork._fromSerialized(source, 'xnet', options);
	}

	/**
	 * Wraps an existing native network pointer.
	 *
	 * @param {object} module - Active Helios WASM module.
	 * @param {number} ptr - Pointer to the native network structure.
	 * @param {boolean} directed - Indicates whether the network is directed.
	 */
	constructor(module, ptr, directed) {
		super();
		this.module = module;
		this.ptr = ptr;
		this.directed = directed;
		this._disposed = false;

		this._anyEventListeners = new Set();
		this._namespacedEventBindings = new Map();

		this._nodeAttributes = new Map();
		this._edgeAttributes = new Map();
		this._networkAttributes = new Map();
		this._bufferSessionDepth = 0;
		this._nodeToEdgePassthrough = new Map();
		this._nodeAttributeDependents = new Map();
		this._nodeTopologyVersion = 0;
		this._edgeTopologyVersion = 0;
		this._activeNodeIndexBuffer = { ptr: 0, capacity: 0, count: 0, version: 0, dirty: true };
		this._activeEdgeIndexBuffer = { ptr: 0, capacity: 0, count: 0, version: 0, dirty: true };
		this._localVersions = new Map();

		this._nodeValidRangeCache = null;
		this._edgeValidRangeCache = null;
		this._validRangeScratchPtr = 0;
	}

	/**
	 * Registers an event listener. Wrapper over `addEventListener`.
	 *
	 * If `options.signal` is provided, the listener is automatically removed on abort.
	 *
	 * @param {string} type
	 * @param {EventHandler} handler
	 * @param {AddEventListenerOptions} [options]
	 * @returns {UnsubscribeFn} Unsubscribe function.
	 */
	on(type, handler, options = undefined) {
		this.addEventListener(type, handler, options);
		const signal = options && typeof options === 'object' ? options.signal : undefined;
		if (signal && typeof signal.addEventListener === 'function') {
			if (signal.aborted) {
				this.removeEventListener(type, handler, options);
			} else {
				const abortHandler = () => {
					try {
						this.removeEventListener(type, handler, options);
					} catch (_) {
						/* no-op */
					}
				};
				signal.addEventListener('abort', abortHandler, { once: true });
			}
		}
		return () => this.removeEventListener(type, handler, options);
	}

	/**
	 * Removes a listener previously added with `on`/`addEventListener`.
	 * @param {string} type
	 * @param {EventHandler} handler
	 * @param {EventListenerOptions} [options]
	 */
	off(type, handler, options = undefined) {
		this.removeEventListener(type, handler, options);
	}

	/**
	 * Registers a handler that runs for every emitted event.
	 *
	 * The handler is invoked as `handler({ type, detail, event, target })`.
	 *
	 * @param {AnyEventHandler} handler
	 * @param {SignalOptions} [options]
	 * @returns {UnsubscribeFn} Unsubscribe function.
	 */
	onAny(handler, options = {}) {
		this._anyEventListeners.add(handler);
		const signal = options?.signal;
		if (signal && typeof signal.addEventListener === 'function') {
			if (signal.aborted) {
				this._anyEventListeners.delete(handler);
			} else {
				const abortHandler = () => {
					this._anyEventListeners.delete(handler);
				};
				signal.addEventListener('abort', abortHandler, { once: true });
			}
		}
		return () => {
			this._anyEventListeners.delete(handler);
		};
	}

	/**
	 * Emits an event using a `CustomEvent` (or `Event` fallback) with the payload in `event.detail`.
	 * Also forwards the emission to any `onAny` subscribers.
	 *
	 * @param {string} type
	 * @param {any} [detail]
	 * @returns {any} The dispatched event instance.
	 */
	emit(type, detail = undefined) {
		const event = createDetailEvent(type, detail);
		this.dispatchEvent(event);
		if (this._anyEventListeners && this._anyEventListeners.size) {
			for (const handler of this._anyEventListeners) {
				try {
					handler({ type, detail, event, target: this });
				} catch (_) {
					/* no-op */
				}
			}
		}
		return event;
	}

	/**
	 * Namespaced binding helper.
	 *
	 * `listen("type.namespace", handler)` guarantees there is only one active handler
	 * per (type, namespace) pair. Re-calling replaces the previous handler.
	 * Pass `handler = null` to remove the binding.
	 *
	 * @param {string} typeWithNamespace
	 * @param {EventHandler|null} handler
	 * @param {AddEventListenerOptions} [options]
	 * @returns {UnsubscribeFn|undefined}
	 */
	listen(typeWithNamespace, handler, options = undefined) {
		const { type, namespace } = parseNamespacedType(typeWithNamespace);
		const key = `${type}::${namespace}`;
		const existing = this._namespacedEventBindings.get(key);
		if (existing && typeof existing.unsubscribe === 'function') {
			try {
				existing.unsubscribe();
			} catch (_) {
				/* no-op */
			}
			this._namespacedEventBindings.delete(key);
		}

		if (!handler) {
			return undefined;
		}

		const unsubscribe = this.on(type, handler, options);
		this._namespacedEventBindings.set(key, { unsubscribe });
		const signal = options && typeof options === 'object' ? options.signal : undefined;
		if (signal && typeof signal.addEventListener === 'function') {
			if (signal.aborted) {
				this._namespacedEventBindings.delete(key);
			} else {
				const abortHandler = () => {
					this._namespacedEventBindings.delete(key);
				};
				signal.addEventListener('abort', abortHandler, { once: true });
			}
		}
		return unsubscribe;
	}

	_emitTopologyChanged(detail) {
		try {
			this.emit(HELIOS_NETWORK_EVENTS.topologyChanged, detail);
		} catch (_) {
			/* no-op */
		}
	}

	_emitAttributeEvent(type, detail) {
		try {
			this.emit(type, detail);
		} catch (_) {
			/* no-op */
		}
	}

	/**
	 * Disposes of the underlying native resources.
	 * Safe to call multiple times.
	 */
	dispose() {
		if (!this._disposed && this.ptr) {
			this._releaseStringAttributes(this._nodeAttributes);
			this._releaseStringAttributes(this._edgeAttributes);
			this._releaseStringAttributes(this._networkAttributes);
			if (this._validRangeScratchPtr) {
				this.module._free(this._validRangeScratchPtr);
				this._validRangeScratchPtr = 0;
			}
			if (this._allNodesSelector) {
				this._allNodesSelector.dispose();
				this._allNodesSelector = null;
			}
			if (this._allEdgesSelector) {
				this._allEdgesSelector.dispose();
				this._allEdgesSelector = null;
			}
			this._disposeActiveIndexBuffer('node');
			this._disposeActiveIndexBuffer('edge');
			this.module._CXFreeNetwork(this.ptr);
			this.ptr = 0;
			this._disposed = true;
		}
	}

	/**
	 * Guards against operations after disposal.
	 * @private
	 * @throws {Error} When the network has been disposed.
	 */
	_ensureActive() {
		if (this._disposed || !this.ptr) {
			throw new Error('HeliosNetwork has been disposed');
		}
	}

	_normalizeVersion(value) {
		const numeric = typeof value === 'bigint' ? Number(value) : value;
		return Number.isFinite(numeric) ? numeric : null;
	}

	/**
	 * Begins a buffer access session during which allocation-prone calls are forbidden.
	 * This helps keep WASM views stable while user code manipulates them.
	 */
	startBufferAccess() {
		this._ensureActive();
		this._bufferSessionDepth += 1;
	}

	/**
	 * Ends a buffer access session started with {@link startBufferAccess}.
	 */
	endBufferAccess() {
		if (this._bufferSessionDepth <= 0) {
			throw new Error('No buffer access session to end');
		}
		this._bufferSessionDepth -= 1;
	}

	/**
	 * Runs a callback inside a buffer access session, ensuring cleanup even on throw.
	 *
	 * @template T
	 * @param {function(): T} fn - Callback to execute.
	 * @returns {T} Callback result.
	 */
	withBufferAccess(fn, options = null) {
		this.startBufferAccess();
		try {
			if (options) {
				this._prepareBufferAccess(options);
			}
			return fn();
		} finally {
			this.endBufferAccess();
		}
	}

	/**
	 * Throws when an allocation-prone method is invoked inside a buffer access session.
	 * @param {string} operation - Description of the attempted operation.
	 * @private
	 */
	_assertCanAllocate(operation) {
		if (this._bufferSessionDepth > 0) {
			throw new Error(`Cannot perform ${operation} during buffer access (run it before calling startBufferAccess)`);
		}
	}

	_assertInsideBufferAccess(operation) {
		if (this._bufferSessionDepth <= 0) {
			throw new Error(`Cannot access ${operation} outside buffer access (wrap it in withBufferAccess(...))`);
		}
	}

	_prepareBufferAccess(options = {}) {
		if (options.nodeIndices) {
			this._ensureActiveIndexBuffer('node');
		}
		if (options.edgeIndices) {
			this._ensureActiveIndexBuffer('edge');
		}
		if (options.edgesView) {
			this._ensureActive();
			this.module._CXNetworkEdgesBuffer(this.ptr);
		}
	}

	_ensureValidRangeScratch() {
		if (this._validRangeScratchPtr) {
			return this._validRangeScratchPtr;
		}
		this._assertCanAllocate('valid range scratch allocation');
		this._ensureActive();
		const ptr = this.module._malloc(16);
		if (!ptr) {
			throw new Error('Failed to allocate valid-range scratch memory');
		}
		this._validRangeScratchPtr = ptr;
		return ptr;
	}

	_invalidateAttributePointerCache() {
		for (const meta of this._nodeAttributes.values()) {
			meta.attributePtr = 0;
			meta.stride = 0;
		}
		for (const meta of this._edgeAttributes.values()) {
			meta.attributePtr = 0;
			meta.stride = 0;
		}
		for (const meta of this._networkAttributes.values()) {
			meta.attributePtr = 0;
			meta.stride = 0;
		}
	}

	_refreshTopologyVersions(scope = null) {
		let touched = false;
		if (scope === null || scope === 'node') {
			const fn = this.module._CXNetworkNodeTopologyVersion;
			if (typeof fn === 'function') {
				const value = this._normalizeVersion(fn.call(this.module, this.ptr));
				if (value !== null) {
					this._nodeTopologyVersion = value;
					touched = true;
				}
			}
		}
		if (scope === null || scope === 'edge') {
			const fn = this.module._CXNetworkEdgeTopologyVersion;
			if (typeof fn === 'function') {
				const value = this._normalizeVersion(fn.call(this.module, this.ptr));
				if (value !== null) {
					this._edgeTopologyVersion = value;
					touched = true;
				}
			}
		}
		return touched;
	}

	_bumpTopology(scope, cascadeEdges = false) {
		const refreshed = this._refreshTopologyVersions(scope === 'node' && cascadeEdges ? null : scope);
		if (!refreshed) {
			if (scope === 'node') {
				this._nodeTopologyVersion += 1;
				if (cascadeEdges) {
					this._edgeTopologyVersion += 1;
				}
			} else if (scope === 'edge') {
				this._edgeTopologyVersion += 1;
			}
		}
		if (scope === 'node') {
			this._activeNodeIndexBuffer.dirty = true;
			if (cascadeEdges) {
				this._activeEdgeIndexBuffer.dirty = true;
				this._refreshTopologyVersions('edge');
			}
		} else if (scope === 'edge') {
			this._activeEdgeIndexBuffer.dirty = true;
		}
	}

	_getTopologyVersion(scope) {
		this._refreshTopologyVersions(scope);
		return scope === 'node' ? this._nodeTopologyVersion : this._edgeTopologyVersion;
	}

	_nextLocalVersion(key) {
		const current = this._localVersions.get(key) || 0;
		const next = current >= Number.MAX_SAFE_INTEGER ? 1 : current + 1;
		this._localVersions.set(key, next);
		return next;
	}

	/**
	 * @returns {number} Total number of active nodes.
	 */
	get nodeCount() {
		this._ensureActive();
		return this.module._CXNetworkNodeCount(this.ptr);
	}

	/**
	 * Returns whether the provided node index is active.
	 *
	 * @param {number} index - Node index to check.
	 * @returns {boolean}
	 */
	hasNodeIndex(index) {
		this._ensureActive();
		if (typeof index !== 'number' || index < 0) {
			return false;
		}
		return !!this.module._CXNetworkIsNodeActive(this.ptr, index);
	}

	/**
	 * Batch version of {@link hasNodeIndex}. Returns booleans aligned with the input order.
	 *
	 * @param {Iterable<number>|Uint32Array} indices - Node indices to check.
	 * @returns {boolean[]}
	 */
	hasNodeIndices(indices) {
		this._ensureActive();
		const list = Array.from(indices || []);
		const result = new Array(list.length);
		for (let i = 0; i < list.length; i += 1) {
			result[i] = this.hasNodeIndex(list[i]);
		}
		return result;
	}

	/**
	 * Selector representing all active nodes (no indices stored).
	 */
	get nodes() {
		this._ensureActive();
		if (!this._allNodesSelector) {
			this._allNodesSelector = NodeSelector.createAll(this);
		}
		return this._allNodesSelector;
	}

	/**
	 * @returns {number} Total number of active edges.
	 */
	get edgeCount() {
		this._ensureActive();
		return this.module._CXNetworkEdgeCount(this.ptr);
	}

	/**
	 * Returns whether the provided edge index is active.
	 *
	 * @param {number} index - Edge index to check.
	 * @returns {boolean}
	 */
	hasEdgeIndex(index) {
		this._ensureActive();
		if (typeof index !== 'number' || index < 0) {
			return false;
		}
		return !!this.module._CXNetworkIsEdgeActive(this.ptr, index);
	}

	/**
	 * Batch version of {@link hasEdgeIndex}. Returns booleans aligned with the input order.
	 *
	 * @param {Iterable<number>|Uint32Array} indices - Edge indices to check.
	 * @returns {boolean[]}
	 */
	hasEdgeIndices(indices) {
		this._ensureActive();
		const list = Array.from(indices || []);
		const result = new Array(list.length);
		for (let i = 0; i < list.length; i += 1) {
			result[i] = this.hasEdgeIndex(list[i]);
		}
		return result;
	}

	/**
	 * Returns a stable WASM-backed view of active node indices in native order.
	 *
	 * @returns {Uint32Array}
	 */
	get nodeIndices() {
		this._assertInsideBufferAccess('active node indices');
		return this._getActiveIndexView('node');
	}

	/**
	 * Selector representing all active edges (no indices stored).
	 */
	get edges() {
		this._ensureActive();
		if (!this._allEdgesSelector) {
			this._allEdgesSelector = EdgeSelector.createAll(this);
		}
		return this._allEdgesSelector;
	}

	/**
	 * @returns {number} Allocated node capacity.
	 */
	get nodeCapacity() {
		this._ensureActive();
		return this.module._CXNetworkNodeCapacity(this.ptr);
	}

	/**
	 * @returns {number} Allocated edge capacity.
	 */
	get edgeCapacity() {
		this._ensureActive();
		return this.module._CXNetworkEdgeCapacity(this.ptr);
	}

	/**
	 * Returns a stable WASM-backed view of active edge indices in native order.
	 *
	 * @returns {Uint32Array}
	 */
	get edgeIndices() {
		this._assertInsideBufferAccess('active edge indices');
		return this._getActiveIndexView('edge');
	}

	/**
	 * @returns {Uint32Array} Flattened `[from, to]` edge pairs.
	 */
	get edgesView() {
		this._assertInsideBufferAccess('edge endpoints');
		this._ensureActive();
		const ptr = this.module._CXNetworkEdgesBuffer(this.ptr);
		return new Uint32Array(this.module.HEAPU32.buffer, ptr, this.edgeCapacity * 2);
	}

	/**
	 * Returns a dictionary describing memory usage (in bytes) of the network's buffers.
	 * Sizes are derived from buffer capacities (not current active counts).
	 *
	 * @param {BufferMemoryUsageOptions} [options]
	 * @returns {object}
	 */
	getBufferMemoryUsage(options = {}) {
		this._ensureActive();
		void options;

		const buffers = Object.create(null);
		const byAttribute = {
			node: Object.create(null),
			edge: Object.create(null),
			network: Object.create(null),
		};

		const totals = {
			nodes: {
				activityBytes: 0,
				freeListBytes: 0,
				sparseAttributeBytes: 0,
				totalBytes: 0,
			},
			edges: {
				activityBytes: 0,
				freeListBytes: 0,
				fromToBytes: 0,
				sparseAttributeBytes: 0,
				totalBytes: 0,
			},
			network: {
				sparseAttributeBytes: 0,
				totalBytes: 0,
			},
			wasmHeapBytes: this.module?.HEAPU8?.buffer?.byteLength ?? 0,
		};

		const addBuffer = (key, bytes) => {
			buffers[key] = bytes >>> 0;
		};

		const bitsetBytesForCapacity = (capacity) => {
			const numeric = Number.isFinite(capacity) ? capacity : 0;
			return Math.ceil(Math.max(0, numeric) / 8);
		};

		const collectSparseAttributeBytes = (scope, name) => {
			const meta = this._ensureAttributeMetadata(scope, name);
			if (!meta) {
				return null;
			}
			const { stride } = this._attributePointers(scope, name, meta);
			const capacity = this._capacityForScope(scope);
			return capacity * stride;
		};

		const nodeActivityBytes = bitsetBytesForCapacity(this.nodeCapacity);
		const edgeActivityBytes = bitsetBytesForCapacity(this.edgeCapacity);
		const fromToBytes = this.edgeCapacity * 2 * Uint32Array.BYTES_PER_ELEMENT;

		const indexElementBytes = Uint32Array.BYTES_PER_ELEMENT;
		const nodeFreeListCapacity = typeof this.module._CXNetworkNodeFreeListCapacity === 'function'
			? this.module._CXNetworkNodeFreeListCapacity(this.ptr)
			: 0;
		const nodeFreeListCount = typeof this.module._CXNetworkNodeFreeListCount === 'function'
			? this.module._CXNetworkNodeFreeListCount(this.ptr)
			: 0;
		const edgeFreeListCapacity = typeof this.module._CXNetworkEdgeFreeListCapacity === 'function'
			? this.module._CXNetworkEdgeFreeListCapacity(this.ptr)
			: 0;
		const edgeFreeListCount = typeof this.module._CXNetworkEdgeFreeListCount === 'function'
			? this.module._CXNetworkEdgeFreeListCount(this.ptr)
			: 0;
		const nodeFreeListBytes = nodeFreeListCapacity * indexElementBytes;
		const edgeFreeListBytes = edgeFreeListCapacity * indexElementBytes;

		totals.nodes.activityBytes += nodeActivityBytes;
		totals.edges.activityBytes += edgeActivityBytes;
		totals.edges.fromToBytes += fromToBytes;
		totals.nodes.freeListBytes += nodeFreeListBytes;
		totals.edges.freeListBytes += edgeFreeListBytes;

		addBuffer('topology.nodeActivity', nodeActivityBytes);
		addBuffer('topology.edgeActivity', edgeActivityBytes);
		addBuffer('topology.nodeFreeList', nodeFreeListBytes);
		addBuffer('topology.edgeFreeList', edgeFreeListBytes);
		addBuffer('topology.edgeFromTo', fromToBytes);

		for (const scope of ['node', 'edge', 'network']) {
			for (const name of this._attributeNames(scope)) {
				const sparseBytes = collectSparseAttributeBytes(scope, name);
				if (sparseBytes == null) {
					continue;
				}
				addBuffer(`sparse.${scope}.attribute.${name}`, sparseBytes);
				byAttribute[scope][name] = {
					sparseBytes,
					totalBytes: sparseBytes,
				};
				if (scope === 'node') {
					totals.nodes.sparseAttributeBytes += sparseBytes;
				} else if (scope === 'edge') {
					totals.edges.sparseAttributeBytes += sparseBytes;
				} else {
					totals.network.sparseAttributeBytes += sparseBytes;
				}
			}
		}
		totals.nodes.totalBytes = totals.nodes.activityBytes
			+ totals.nodes.freeListBytes
			+ totals.nodes.sparseAttributeBytes;
		totals.edges.totalBytes = totals.edges.activityBytes
			+ totals.edges.freeListBytes
			+ totals.edges.fromToBytes
			+ totals.edges.sparseAttributeBytes;
		totals.network.totalBytes = totals.network.sparseAttributeBytes;

		const attributes = {
			node: {
				sparseBytes: totals.nodes.sparseAttributeBytes,
				totalBytes: totals.nodes.sparseAttributeBytes,
			},
			edge: {
				fromToBytes: totals.edges.fromToBytes,
				sparseBytes: totals.edges.sparseAttributeBytes,
				totalBytes: totals.edges.fromToBytes + totals.edges.sparseAttributeBytes,
			},
			network: {
				sparseBytes: totals.network.sparseAttributeBytes,
				totalBytes: totals.network.totalBytes,
			},
		};

		const wasm = {
			heapBytes: totals.wasmHeapBytes,
		};

		return {
			totals,
			attributes,
			byAttribute,
			buffers,
			wasm,
		};
	}

	/**
	 * Returns a dictionary containing version counters for all tracked items.
	 * Values are the version numbers only (no metadata).
	 *
	 * @param {BufferVersionOptions} [options]
	 * @returns {object}
	 */
	getBufferVersions(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('getBufferVersions');
		void options;

		const topology = this.getTopologyVersions();

		const attributes = {
			node: Object.create(null),
			edge: Object.create(null),
			network: Object.create(null),
		};
		for (const name of this.getNodeAttributeNames()) {
			attributes.node[name] = this.getNodeAttributeVersion(name);
		}
		for (const name of this.getEdgeAttributeNames()) {
			attributes.edge[name] = this.getEdgeAttributeVersion(name);
		}
		for (const name of this.getNetworkAttributeNames()) {
			attributes.network[name] = this.getNetworkAttributeVersion(name);
		}

		return {
			topology,
			attributes,
		};
	}

	/**
	 * Registers an edge attribute that is derived from a node attribute.
	 * The derived values are copied directly into the sparse edge buffer.
	 *
	 * @param {string} sourceName - Node attribute identifier.
	 * @param {string} edgeName - Edge attribute identifier that will expose the derived values.
	 * @param {EndpointSelection} [endpoints='both'] - Which endpoint to propagate (0/1/-1).
	 * @param {boolean} [doubleWidth=true] - When copying a single endpoint, duplicate it to fill a double-width layout.
	 */
		defineNodeToEdgeAttribute(sourceName, edgeName, endpoints = 'both', doubleWidth = true) {
		if (!this.hasNodeAttribute(sourceName)) {
			throw new Error(`Unknown node attribute "${sourceName}"`);
		}
		this._ensureActive();
		const endpointMode = this._normalizeEndpointMode(endpoints);
		const sourceMeta = this._ensureAttributeMetadata('node', sourceName);
		if (!sourceMeta) {
			throw new Error(`Node attribute "${sourceName}" is not defined`);
		}
		if (!this._isNumericType(sourceMeta.type)) {
			throw new Error('Node-to-edge passthrough only supports numeric node attributes');
		}
		const sourceComponents = Math.max(1, sourceMeta.dimension || 1);
		const targetComponents = endpointMode === -1
			? sourceComponents * 2
			: (doubleWidth ? sourceComponents * 2 : sourceComponents);
			if (this._edgeAttributes.has(edgeName)) {
				throw new Error(`Edge attribute "${edgeName}" already exists; remove it before registering a node-to-edge passthrough`);
			}
			this.defineEdgeAttribute(edgeName, sourceMeta.type, targetComponents);
			this._nodeToEdgePassthrough.set(edgeName, {
				sourceName,
				endpointMode,
				doubleWidth,
			});
			this._registerNodeToEdgeDependency(sourceName, edgeName);
			this._copyNodeToEdgeAttribute(sourceName, edgeName, endpointMode, doubleWidth);
		}

	/**
	 * Returns a snapshot of node-to-edge passthrough registrations.
	 * Each entry describes the node source, the derived edge attribute, and the endpoint policy.
	 * @returns {Array.<NodeToEdgePassthrough>}
	 */
	getNodeToEdgePassthroughs() {
		this._ensureActive();
		const results = [];
		for (const [edgeName, entry] of this._nodeToEdgePassthrough.entries()) {
			results.push({
				edgeName,
				sourceName: entry.sourceName,
				endpoints: this._denormalizeEndpointMode(entry.endpointMode),
				doubleWidth: Boolean(entry.doubleWidth),
			});
		}
		return results;
	}

	/**
	 * Removes a node-to-edge passthrough registration, stopping automatic copies.
	 *
	 * @param {string} edgeName - Passthrough edge attribute identifier.
	 */
	removeNodeToEdgeAttribute(edgeName) {
		this._ensureActive();
		const entry = this._nodeToEdgePassthrough.get(edgeName);
		if (!entry) {
			return;
		}
		this._nodeToEdgePassthrough.delete(edgeName);
		this._unregisterNodeToEdgeDependency(entry.sourceName, edgeName);
	}

	/**
	 * Removes a node attribute and its storage.
	 */
	removeNodeAttribute(name) {
		this._removeAttribute('node', name);
	}

	/**
	 * Removes an edge attribute and its storage.
	 */
	removeEdgeAttribute(name) {
		this._removeAttribute('edge', name);
	}

	/**
	 * Removes a network attribute and its storage.
	 */
	removeNetworkAttribute(name) {
		this._removeAttribute('network', name);
	}

	/**
	 * Returns the min/max active node indices as {start,end}.
	 */
	get nodeValidRange() {
		this._assertCanAllocate('nodeValidRange');
		this._ensureActive();
		const canCache = typeof this.module._CXNetworkNodeTopologyVersion === 'function';
		if (canCache) {
			this._refreshTopologyVersions('node');
			const version = this._nodeTopologyVersion;
			const cached = this._nodeValidRangeCache;
			if (cached && cached.version === version) {
				return cached.range;
			}
		} else {
			this._nodeValidRangeCache = null;
		}
		const scratch = this._ensureValidRangeScratch();
		const startPtr = scratch;
		const endPtr = scratch + 8;
		this.module._CXNetworkGetNodeValidRange(this.ptr, startPtr, endPtr);
		const range = {
			start: Number(this.module.HEAPU32[startPtr >>> 2]),
			end: Number(this.module.HEAPU32[endPtr >>> 2]),
		};
		if (canCache) {
			this._nodeValidRangeCache = { version: this._nodeTopologyVersion, range };
		}
		return range;
	}

	/**
	 * Returns the min/max active edge indices as {start,end}.
	 */
	get edgeValidRange() {
		this._assertCanAllocate('edgeValidRange');
		this._ensureActive();
		const canCache = typeof this.module._CXNetworkEdgeTopologyVersion === 'function';
		if (canCache) {
			this._refreshTopologyVersions('edge');
			const version = this._edgeTopologyVersion;
			const cached = this._edgeValidRangeCache;
			if (cached && cached.version === version) {
				return cached.range;
			}
		} else {
			this._edgeValidRangeCache = null;
		}
		const scratch = this._ensureValidRangeScratch();
		const startPtr = scratch;
		const endPtr = scratch + 8;
		this.module._CXNetworkGetEdgeValidRange(this.ptr, startPtr, endPtr);
		const range = {
			start: Number(this.module.HEAPU32[startPtr >>> 2]),
			end: Number(this.module.HEAPU32[endPtr >>> 2]),
		};
		if (canCache) {
			this._edgeValidRangeCache = { version: this._edgeTopologyVersion, range };
		}
		return range;
	}

	/**
	 * Returns a node attribute buffer slice over the valid node range.
	 * @param {string} name
	 * @returns {{view:TypedArray,start:number,end:number,stride:number}}
	 */
	getNodeAttributeBufferSlice(name) {
		const meta = this._ensureAttributeMetadata('node', name);
		if (!meta) {
			throw new Error(`Attribute "${name}" not defined on node`);
		}
		const { pointer, stride } = this._attributePointers('node', name, meta);
		const { start, end } = this.nodeValidRange;
		const capacity = this.nodeCapacity;
		const ctor = meta.type === AttributeType.String || meta.type === AttributeType.Data || meta.type === AttributeType.Javascript
			? Uint32Array
			: TypedArrayForType[meta.type];
		if (!ctor) {
			throw new Error('Unsupported attribute type for slicing');
		}
		const elementCount = (capacity * meta.dimension);
		const fullView = new ctor(this.module.HEAPU8.buffer, pointer, elementCount);
		const sliceStart = start * meta.dimension;
		const sliceEnd = end * meta.dimension;
		const view = fullView.subarray(sliceStart, sliceEnd);
		return { view, start, end, stride, version: this._getAttributeVersion('node', name), bumpVersion: () => this._bumpAttributeVersion('node', name, { op: 'bump' }) };
	}

	/**
	 * Returns an edge attribute buffer slice over the valid edge range.
	 * @param {string} name
	 * @returns {{view:TypedArray,start:number,end:number,stride:number}}
	 */
	getEdgeAttributeBufferSlice(name) {
		const meta = this._ensureAttributeMetadata('edge', name);
		if (!meta) {
			throw new Error(`Attribute "${name}" not defined on edge`);
		}
		const { pointer, stride } = this._attributePointers('edge', name, meta);
		const { start, end } = this.edgeValidRange;
		const capacity = this.edgeCapacity;
		const ctor = meta.type === AttributeType.String || meta.type === AttributeType.Data || meta.type === AttributeType.Javascript
			? Uint32Array
			: TypedArrayForType[meta.type];
		if (!ctor) {
			throw new Error('Unsupported attribute type for slicing');
		}
		const elementCount = (capacity * meta.dimension);
		const fullView = new ctor(this.module.HEAPU8.buffer, pointer, elementCount);
		const sliceStart = start * meta.dimension;
		const sliceEnd = end * meta.dimension;
		const view = fullView.subarray(sliceStart, sliceEnd);
		return { view, start, end, stride, version: this._getAttributeVersion('edge', name), bumpVersion: () => this._bumpAttributeVersion('edge', name, { op: 'bump' }) };
	}

	/**
	 * @private
	 * Writes active indices into a caller-provided WASM buffer.
	 */
	_writeActiveIndices(target, writer, label) {
		this._ensureActive();
		if (typeof writer !== 'function') {
			throw new Error(`${label} writer is unavailable in this WASM build`);
		}
		if (!(target instanceof Uint32Array)) {
			throw new Error(`${label} buffer must be a Uint32Array`);
		}
		if (target.buffer !== this.module.HEAPU32.buffer) {
			throw new Error(`${label} buffer must live in the WASM heap (module.HEAPU32.buffer)`);
		}
		return writer.call(this.module, this.ptr, target.byteOffset, target.length);
	}

	/**
	 * Fills a caller-provided buffer with active node indices.
	 *
	 * @param {Uint32Array} target - Preallocated Uint32Array backed by WASM memory.
	 * @returns {number} Number of active nodes (required capacity). When this
	 *   exceeds `target.length`, no writes occur and the return value indicates
	 *   the needed length.
	 */
	writeActiveNodes(target) {
		return this._writeActiveIndices(target, this.module._CXNetworkWriteActiveNodes, 'Node');
	}

	/**
	 * Fills a caller-provided buffer with active edge indices.
	 *
	 * @param {Uint32Array} target - Preallocated Uint32Array backed by WASM memory.
	 * @returns {number} Number of active edges (required capacity). When this
	 *   exceeds `target.length`, no writes occur and the return value indicates
	 *   the needed length.
	 */
	writeActiveEdges(target) {
		return this._writeActiveIndices(target, this.module._CXNetworkWriteActiveEdges, 'Edge');
	}

	/**
	 * Writes two vec-like position vectors per active edge directly into the provided buffer.
	 *
	 * @param {Float32Array} positions - WASM-backed position buffer (stride = `componentsPerNode`).
	 * @param {Float32Array} segments - Destination buffer in WASM memory; must have room for `count * componentsPerNode * 2` floats.
	 * @param {number} [componentsPerNode=4] - Number of floats to copy per node (e.g. 4 for vec4 layouts).
	 * @returns {number} Number of edges written or required. When this exceeds
	 *   `Math.floor(segments.length / (componentsPerNode * 2))`, no writes occur.
	 */
	writeActiveEdgeSegments(positions, segments, componentsPerNode = 4) {
		this._ensureActive();
		if (typeof this.module._CXNetworkWriteActiveEdgeSegments !== 'function') {
			throw new Error('CXNetworkWriteActiveEdgeSegments is unavailable in this WASM build');
		}
		const heapBuffer = this.module.HEAPU8.buffer;
		if (!(positions instanceof Float32Array) || positions.buffer !== heapBuffer) {
			throw new Error('positions must be a Float32Array backed by the WASM heap');
		}
		if (!(segments instanceof Float32Array) || segments.buffer !== heapBuffer) {
			throw new Error('segments must be a Float32Array backed by the WASM heap');
		}
		const width = Math.trunc(componentsPerNode);
		if (!Number.isFinite(width) || width <= 0) {
			throw new Error('componentsPerNode must be a positive integer');
		}
		const segmentEdgeCapacity = Math.floor(segments.length / (width * 2));
		return this.module._CXNetworkWriteActiveEdgeSegments(
			this.ptr,
			positions.byteOffset,
			width,
			segments.byteOffset,
			segmentEdgeCapacity
		);
	}

	/**
	 * Writes node attribute spans for both endpoints of each active edge into a caller-managed buffer.
	 *
	 * @param {TypedArray} source - WASM-backed node attribute view; element size drives copy width.
	 * @param {TypedArray} target - Destination buffer in WASM memory sized for `count * componentsPerNode * 2` elements.
	 * @param {number} [componentsPerNode=1] - Number of components to copy per node.
	 * @returns {number} Number of edges written or required. When this exceeds
	 *   `Math.floor(target.byteLength / (componentsPerNode * elementSize * 2))`, no writes occur.
	 */
	writeActiveEdgeAttributePairs(source, target, componentsPerNode = 1) {
		this._ensureActive();
		if (typeof this.module._CXNetworkWriteActiveEdgeNodeAttributes !== 'function') {
			throw new Error('CXNetworkWriteActiveEdgeNodeAttributes is unavailable in this WASM build');
		}
		const heapBuffer = this.module.HEAPU8.buffer;
		if (!ArrayBuffer.isView(source) || source.buffer !== heapBuffer) {
			throw new Error('source must be a typed array backed by the WASM heap');
		}
		if (!ArrayBuffer.isView(target) || target.buffer !== heapBuffer) {
			throw new Error('target must be a typed array backed by the WASM heap');
		}
		const componentWidth = Math.trunc(componentsPerNode);
		if (!Number.isFinite(componentWidth) || componentWidth <= 0) {
			throw new Error('componentsPerNode must be a positive integer');
		}
		const elementSize = source.BYTES_PER_ELEMENT;
		if (!Number.isFinite(elementSize) || elementSize <= 0) {
			throw new Error('source must expose a valid BYTES_PER_ELEMENT');
		}
		if (target.BYTES_PER_ELEMENT !== elementSize) {
			throw new Error('target element size must match source element size');
		}
		const edgeCapacity = Math.floor(target.byteLength / (elementSize * componentWidth * 2));
		return this.module._CXNetworkWriteActiveEdgeNodeAttributes(
			this.ptr,
			source.byteOffset,
			componentWidth,
			elementSize,
			target.byteOffset,
			edgeCapacity
		);
	}

	/**
	 * Interpolates a float node attribute toward a target buffer using layout-timed smoothing.
	 *
	 * @param {string} name - Node attribute name.
	 * @param {Float32Array|number[]} target - Target values (length = nodeCapacity * dimension).
	 * @param {{elapsedMs?:number,layoutElapsedMs?:number,smoothing?:number,minDisplacementRatio?:number,emitEvent?:boolean}} [options]
	 * @returns {boolean} True when further interpolation steps are recommended.
	 */
	interpolateNodeAttribute(name, target, options = {}) {
		this._ensureActive();
		const interpolateFn = this.module._CXAttributeInterpolateFloatBuffer;
		if (typeof interpolateFn !== 'function') {
			throw new Error('CXAttributeInterpolateFloatBuffer is unavailable in this WASM build');
		}
		const meta = this._ensureAttributeMetadata('node', name);
		if (!meta) {
			throw new Error(`Unknown node attribute "${name}"`);
		}
		if (meta.type !== AttributeType.Float) {
			throw new Error(`Node attribute "${name}" must be a float attribute to interpolate`);
		}
		const { attributePtr } = this._attributePointers('node', name, meta);
		if (!attributePtr) {
			throw new Error(`Attribute pointer for "${name}" is unavailable`);
		}
		const { name: targetName, meta: targetMeta } = this._ensureInterpolationTargetAttribute(name, meta?.dimension || 1);
		const targetPointers = this._attributePointers('node', targetName, targetMeta);
		let array = target;
		if (!ArrayBuffer.isView(array)) {
			array = Float32Array.from(array ?? []);
		}
		const count = array.length >>> 0;
		if (count === 0) {
			return false;
		}
		const dimension = Number.isFinite(targetMeta.dimension) && targetMeta.dimension > 0 ? targetMeta.dimension : 1;
		const maxCount = Math.max(0, this.nodeCapacity * dimension);
		const copyCount = Math.min(count, maxCount);
		if (copyCount === 0) {
			return false;
		}
		const heap = this.module.HEAPF32;
		const targetPtr = targetPointers.pointer >>> 0;
		if (!targetPtr) {
			throw new Error(`Interpolation target buffer for "${targetName}" is unavailable`);
		}
		const sourceSlice = copyCount === count ? array : array.subarray(0, copyCount);
		if (array.buffer !== heap.buffer || array.byteOffset !== targetPtr || sourceSlice.length !== array.length) {
			heap.set(sourceSlice, targetPtr >>> 2);
		}
		const copy = { ptr: targetPtr, count: copyCount };
		const elapsedMs = Number.isFinite(options.elapsedMs) ? Math.max(0, options.elapsedMs) : 16;
		const layoutElapsedMs = Number.isFinite(options.layoutElapsedMs)
			? Math.max(0, options.layoutElapsedMs)
			: elapsedMs;
		const smoothing = Number.isFinite(options.smoothing) ? options.smoothing : 6;
		const minDisplacementRatio = Number.isFinite(options.minDisplacementRatio)
			? Math.max(0, options.minDisplacementRatio)
			: 0.0005;
		const shouldContinue = Boolean(interpolateFn.call(
			this.module,
			attributePtr,
			copy.ptr,
			copy.count,
			elapsedMs,
			layoutElapsedMs,
			smoothing,
			minDisplacementRatio,
		));
		this._recordAttributeChangeSilently('node', name, {
			op: 'interpolate',
			emitEvent: options.emitEvent === true,
		});
		return shouldContinue;
	}

	/**
	 * Serializes the network into the `.bxnet` container format.
	 *
	 * @param {SaveSerializedOptions} [options]
	 * @returns {Promise<Uint8Array|ArrayBuffer|string|Blob|undefined>} Serialized payload or void when writing directly to disk.
	 */
	async saveBXNet(options = {}) {
		return this._saveSerialized('bxnet', options);
	}

	/**
	 * Serializes the network into the human-readable `.xnet` container format.
	 *
	 * @param {SaveSerializedOptions} [options]
	 * @returns {Promise<Uint8Array|ArrayBuffer|string|Blob|undefined>} Serialized payload or void when writing directly to disk.
	 */
	async saveXNet(options = {}) {
		return this._saveSerialized('xnet', options);
	}

	/**
	 * Serializes the network into the BGZF-compressed `.zxnet` container format.
	 *
	 * @param {SaveZXNetOptions} [options]
	 * @returns {Promise<Uint8Array|ArrayBuffer|string|Blob|undefined>} Serialized payload or void when writing directly to disk.
	 */
	async saveZXNet(options = {}) {
		return this._saveSerialized('zxnet', options);
	}

	/**
	 * Adds nodes to the network, returning their indices.
	 *
	 * @param {number} count - Number of nodes to add.
	 * @returns {Uint32Array} Copy of the inserted node indices.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(3);
	 * console.log([...nodes]); // e.g. [0, 1, 2]
	 */
	addNodes(count) {
		this._assertCanAllocate('addNodes');
		this._ensureActive();
		const prevNodeCount = this.nodeCount;
		const prevEdgeCount = this.edgeCount;
		const prevTopology = this.getTopologyVersions();
		if (!count) {
			return new Uint32Array();
		}
		const ptr = this.module._malloc(count * 4);
		if (!ptr) {
			throw new Error('Failed to allocate memory for node indices');
		}
		const success = this.module._CXNetworkAddNodes(this.ptr, count, ptr);
		if (!success) {
			this.module._free(ptr);
			throw new Error('Failed to add nodes');
		}
		const indices = new Uint32Array(this.module.HEAPU32.buffer, ptr, count).slice();
		this.module._free(ptr);
		this._markAllPassthroughEdgesDirty();
		this._bumpTopology('node');
		const topology = this.getTopologyVersions();
		this.emit(HELIOS_NETWORK_EVENTS.nodesAdded, {
			indices,
			count: indices.length >>> 0,
			oldNodeCount: prevNodeCount,
			nodeCount: this.nodeCount,
			oldEdgeCount: prevEdgeCount,
			edgeCount: this.edgeCount,
			topology,
			oldTopology: prevTopology,
		});
		this._emitTopologyChanged({
			kind: 'nodes',
			op: 'added',
			topology,
			oldTopology: prevTopology,
		});
		return indices;
	}

	/**
	 * Removes nodes from the network and clears related attributes.
	 *
	 * @param {Iterable<number>|Uint32Array} indices - Node indices slated for removal.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(2);
	 * net.removeNodes([nodes[0]]);
	 * console.log(net.nodeCount); // 1
	 */
	removeNodes(indices) {
		this._assertCanAllocate('removeNodes');
		this._ensureActive();
		const prevNodeCount = this.nodeCount;
		const prevEdgeCount = this.edgeCount;
		const prevTopology = this.getTopologyVersions();
		const array = Array.isArray(indices) ? Uint32Array.from(indices) : new Uint32Array(indices);
		if (array.length === 0) {
			return;
		}
		const ptr = this.module._malloc(array.length * 4);
		if (!ptr) {
			throw new Error('Failed to allocate memory for node removal');
		}
		this.module.HEAPU32.set(array, ptr / 4);
		this.module._CXNetworkRemoveNodes(this.ptr, ptr, array.length);
		this.module._free(ptr);

		for (const [name, meta] of this._nodeAttributes.entries()) {
			if (meta.complex || POINTER_ATTRIBUTE_TYPES.has(meta.type)) {
				const { pointer } = this._attributePointers('node', name, meta);
				const buffer = new Uint32Array(this.module.HEAPU32.buffer, pointer, this.nodeCapacity);
				for (const index of array) {
					if (meta.complex) {
						meta.jsStore.delete(index);
						buffer[index] = 0;
					}
					if (meta.type === AttributeType.String) {
						const oldPtr = meta.stringPointers.get(index);
						if (oldPtr) {
							this.module._free(oldPtr);
							meta.stringPointers.delete(index);
						}
						buffer[index] = 0;
					}
				}
			}
		}
		this._markAllPassthroughEdgesDirty();
		this._bumpTopology('node', true); // removing nodes also removes incident edges
		const topology = this.getTopologyVersions();
		this.emit(HELIOS_NETWORK_EVENTS.nodesRemoved, {
			indices: array,
			count: array.length >>> 0,
			oldNodeCount: prevNodeCount,
			nodeCount: this.nodeCount,
			oldEdgeCount: prevEdgeCount,
			edgeCount: this.edgeCount,
			topology,
			oldTopology: prevTopology,
		});
		this._emitTopologyChanged({
			kind: 'nodes',
			op: 'removed',
			topology,
			oldTopology: prevTopology,
		});
	}

	/**
	 * Adds edges to the network in bulk.
	 *
	 * Supported input shapes:
	 *   - `Uint32Array` / typed array with flattened `[from0, to0, from1, to1, ...]`
	 *   - `number[]` flattened pairs (same layout as above)
	 *   - `Array<[number, number]>`
	 *   - `Array<{ from: number, to: number }>`
	 *
	 * @param {Uint32Array|number[]|Array.<Array.<number>>|Array.<{from:number,to:number}>} edgeList - Collection describing edges to add.
	 * @returns {Uint32Array} Copy of the newly created edge indices.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(2);
	 * const edges = net.addEdges([{ from: nodes[0], to: nodes[1] }]);
	 * console.log(edges.length); // 1
	 * @throws {Error} When the input shape is invalid or memory allocation fails.
	 */
	addEdges(edgeList) {
		this._assertCanAllocate('addEdges');
		this._ensureActive();
		const prevNodeCount = this.nodeCount;
		const prevEdgeCount = this.edgeCount;
		const prevTopology = this.getTopologyVersions();
		if (!edgeList || edgeList.length === 0) {
			return new Uint32Array();
		}
		let array;
		if (edgeList instanceof Uint32Array) {
			array = edgeList;
		} else if (ArrayBuffer.isView(edgeList) && edgeList.BYTES_PER_ELEMENT) {
			array = new Uint32Array(edgeList);
		} else if (Array.isArray(edgeList)) {
			if (edgeList.length === 0) {
				return new Uint32Array();
			}
			const first = edgeList[0];
			if (Array.isArray(first)) {
				array = new Uint32Array(edgeList.length * 2);
				let offset = 0;
				for (const pair of edgeList) {
					if (!Array.isArray(pair) || pair.length < 2) {
						throw new Error('Edge tuple must contain [from, to]');
					}
					array[offset++] = pair[0];
					array[offset++] = pair[1];
				}
			} else if (typeof first === 'number') {
				if (edgeList.length % 2 !== 0) {
					throw new Error('Flat edge array must contain an even number of entries');
				}
				array = Uint32Array.from(edgeList);
			} else {
				array = new Uint32Array(edgeList.length * 2);
				let offset = 0;
				for (const entry of edgeList) {
					if (entry == null || typeof entry.from !== 'number' || typeof entry.to !== 'number') {
						throw new Error('Edge list entries must expose numeric from/to properties');
					}
					array[offset++] = entry.from;
					array[offset++] = entry.to;
				}
			}
		} else {
			throw new Error('Unsupported edge list format');
		}
		const edgeCount = array.length / 2;
		const edgesPtr = this.module._malloc(array.length * 4);
		const outPtr = this.module._malloc(edgeCount * 4);
		if (!edgesPtr || !outPtr) {
			if (edgesPtr) this.module._free(edgesPtr);
			if (outPtr) this.module._free(outPtr);
			throw new Error('Failed to allocate memory for edges');
		}
		this.module.HEAPU32.set(array, edgesPtr / 4);
		const success = this.module._CXNetworkAddEdges(this.ptr, edgesPtr, edgeCount, outPtr);
		this.module._free(edgesPtr);
		if (!success) {
			this.module._free(outPtr);
			throw new Error('Failed to add edges');
		}
		const indices = new Uint32Array(this.module.HEAPU32.buffer, outPtr, edgeCount).slice();
		this.module._free(outPtr);
		this._markAllPassthroughEdgesDirty();
		this._bumpTopology('edge');
		const topology = this.getTopologyVersions();
		this.emit(HELIOS_NETWORK_EVENTS.edgesAdded, {
			indices,
			count: indices.length >>> 0,
			oldNodeCount: prevNodeCount,
			nodeCount: this.nodeCount,
			oldEdgeCount: prevEdgeCount,
			edgeCount: this.edgeCount,
			topology,
			oldTopology: prevTopology,
		});
		this._emitTopologyChanged({
			kind: 'edges',
			op: 'added',
			topology,
			oldTopology: prevTopology,
		});
		return indices;
	}

	/**
	 * Removes edges from the network and clears related attributes.
	 *
	 * @param {Iterable<number>|Uint32Array} indices - Edge indices slated for removal.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * const edges = net.addEdges([[0, 1]]);
	 * net.removeEdges(edges);
	 * console.log(net.edgeCount); // 0
	 */
	removeEdges(indices) {
		this._assertCanAllocate('removeEdges');
		this._ensureActive();
		const prevNodeCount = this.nodeCount;
		const prevEdgeCount = this.edgeCount;
		const prevTopology = this.getTopologyVersions();
		const array = Array.isArray(indices) ? Uint32Array.from(indices) : new Uint32Array(indices);
		if (array.length === 0) {
			return;
		}
		const ptr = this.module._malloc(array.length * 4);
		if (!ptr) {
			throw new Error('Failed to allocate memory for edge removal');
		}
		this.module.HEAPU32.set(array, ptr / 4);
		this.module._CXNetworkRemoveEdges(this.ptr, ptr, array.length);
		this.module._free(ptr);

		for (const [name, meta] of this._edgeAttributes.entries()) {
			if (meta.complex || POINTER_ATTRIBUTE_TYPES.has(meta.type)) {
				const { pointer } = this._attributePointers('edge', name, meta);
				const buffer = new Uint32Array(this.module.HEAPU32.buffer, pointer, this.edgeCapacity);
				for (const index of array) {
					if (meta.complex) {
						meta.jsStore.delete(index);
						buffer[index] = 0;
					}
					if (meta.type === AttributeType.String) {
						const oldPtr = meta.stringPointers.get(index);
						if (oldPtr) {
							this.module._free(oldPtr);
							meta.stringPointers.delete(index);
						}
						buffer[index] = 0;
					}
				}
			}
		}
		this._markAllPassthroughEdgesDirty();
		this._bumpTopology('edge');
		const topology = this.getTopologyVersions();
		this.emit(HELIOS_NETWORK_EVENTS.edgesRemoved, {
			indices: array,
			count: array.length >>> 0,
			oldNodeCount: prevNodeCount,
			nodeCount: this.nodeCount,
			oldEdgeCount: prevEdgeCount,
			edgeCount: this.edgeCount,
			topology,
			oldTopology: prevTopology,
		});
		this._emitTopologyChanged({
			kind: 'edges',
			op: 'removed',
			topology,
			oldTopology: prevTopology,
		});
	}

	/**
	 * Returns the outgoing neighbors of the provided node.
	 *
	 * @param {number} node - Source node index.
	 * @returns {{nodes: Uint32Array, edges: Uint32Array}} Neighbor node and edge indices.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(3);
	 * net.addEdges([[nodes[0], nodes[1]], [nodes[0], nodes[2]]]);
	 *
	 * const { nodes: outgoing } = net.getOutNeighbors(nodes[0]);
	 * console.log([...outgoing]); // [nodes[1], nodes[2]]
	 */
	getOutNeighbors(node) {
		this._ensureActive();
		const container = this.module._CXNetworkOutNeighbors(this.ptr, node);
		return this._readNeighborContainer(container);
	}

	/**
	 * Returns the incoming neighbors of the provided node.
	 *
	 * @param {number} node - Target node index.
	 * @returns {{nodes: Uint32Array, edges: Uint32Array}} Neighbor node and edge indices.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * const nodes = net.addNodes(2);
	 * net.addEdges([[nodes[0], nodes[1]]]);
	 * const { nodes: incoming } = net.getInNeighbors(nodes[1]);
	 * console.log([...incoming]); // [nodes[0]]
	 */
	getInNeighbors(node) {
		this._ensureActive();
		const container = this.module._CXNetworkInNeighbors(this.ptr, node);
		return this._readNeighborContainer(container);
	}

	/**
	 * Returns one-hop neighbors of a node with configurable direction.
	 *
	 * @param {number} node - Source node index.
	 * @param {object} [options]
	 * @param {(number|string)} [options.direction='both'] - out/in/both direction.
	 * @param {boolean} [options.includeEdges=true] - Include traversed edge ids.
	 * @param {boolean} [options.includeSourceNodes=true] - Allow source node in results.
	 * @returns {Uint32Array|{nodes:Uint32Array,edges:Uint32Array}} Neighbor nodes (and optional edges).
	 */
	getNeighbors(node, options = {}) {
		return this.getNeighborsForNodes([node], options);
	}

	/**
	 * Returns unique one-hop neighbors for a sequence of source nodes.
	 *
	 * @param {Iterable<number>|Uint32Array} sourceNodes - Source node ids.
	 * @param {object} [options]
	 * @param {(number|string)} [options.direction='both'] - out/in/both direction.
	 * @param {boolean} [options.includeEdges=true] - Include traversed edge ids.
	 * @param {boolean} [options.includeSourceNodes=true] - Allow source nodes in results.
	 * @param {boolean} [options.asSelector=false] - Return selectors instead of arrays.
	 * @returns {(Uint32Array|NodeSelector)|{nodes:(Uint32Array|NodeSelector),edges:(Uint32Array|EdgeSelector)}}
	 */
	getNeighborsForNodes(sourceNodes, options = {}) {
		const {
			direction = 'both',
			includeEdges = true,
			includeSourceNodes = true,
			asSelector = false,
		} = options;
		return this._collectNeighborsNative({
			sourceNodes,
			direction,
			includeEdges,
			includeSourceNodes,
			asSelector,
			mode: 'one-hop',
		});
	}

	/**
	 * Returns neighbors at exactly the given concentric level.
	 *
	 * @param {number|Iterable<number>|Uint32Array} sourceNodes - Source node id(s).
	 * @param {number} level - Hop distance (0 = source set).
	 * @param {object} [options]
	 * @param {(number|string)} [options.direction='both'] - out/in/both direction.
	 * @param {boolean} [options.includeEdges=true] - Include traversed edge ids.
	 * @param {boolean} [options.includeSourceNodes=false] - Allow source nodes in results.
	 * @param {boolean} [options.asSelector=false] - Return selectors instead of arrays.
	 * @returns {(Uint32Array|NodeSelector)|{nodes:(Uint32Array|NodeSelector),edges:(Uint32Array|EdgeSelector)}}
	 */
	getNeighborsAtLevel(sourceNodes, level, options = {}) {
		const {
			direction = 'both',
			includeEdges = true,
			includeSourceNodes = false,
			asSelector = false,
		} = options;
		const numericLevel = Number(level);
		const normalizedLevel = Number.isFinite(numericLevel) && numericLevel > 0
			? Math.floor(numericLevel)
			: 0;
		return this._collectNeighborsNative({
			sourceNodes,
			direction,
			includeEdges,
			includeSourceNodes,
			asSelector,
			mode: 'exact-level',
			level: normalizedLevel,
		});
	}

	/**
	 * Returns neighbors up to and including the given concentric level.
	 *
	 * @param {number|Iterable<number>|Uint32Array} sourceNodes - Source node id(s).
	 * @param {number} maxLevel - Maximum hop distance (inclusive).
	 * @param {object} [options]
	 * @param {(number|string)} [options.direction='both'] - out/in/both direction.
	 * @param {boolean} [options.includeEdges=true] - Include traversed edge ids.
	 * @param {boolean} [options.includeSourceNodes=false] - Allow source nodes in results.
	 * @param {boolean} [options.asSelector=false] - Return selectors instead of arrays.
	 * @returns {(Uint32Array|NodeSelector)|{nodes:(Uint32Array|NodeSelector),edges:(Uint32Array|EdgeSelector)}}
	 */
	getNeighborsUpToLevel(sourceNodes, maxLevel, options = {}) {
		const {
			direction = 'both',
			includeEdges = true,
			includeSourceNodes = false,
			asSelector = false,
		} = options;
		const numericLevel = Number(maxLevel);
		const normalizedLevel = Number.isFinite(numericLevel) && numericLevel > 0
			? Math.floor(numericLevel)
			: 0;
		return this._collectNeighborsNative({
			sourceNodes,
			direction,
			includeEdges,
			includeSourceNodes,
			asSelector,
			mode: 'up-to-level',
			level: normalizedLevel,
		});
	}

	_collectNeighborsNative({
		sourceNodes,
		direction = 'both',
		includeEdges = true,
		includeSourceNodes = false,
		asSelector = false,
		mode = 'one-hop',
		level = 0,
	}) {
		this._ensureActive();
		this._assertCanAllocate('neighbor collection');

		const sourceArray = this._normalizeNeighborSourceNodes(sourceNodes);
		const sourceInfo = this._copyIndicesToWasm(sourceArray);
		const nodeSelector = NodeSelector.create(this.module, this);
		const edgeSelector = includeEdges ? EdgeSelector.create(this.module, this) : null;
		const directionId = this._normalizeNeighborDirection(direction) >>> 0;
		const includeSourcesFlag = includeSourceNodes ? 1 : 0;

		const fn = mode === 'one-hop'
			? this.module._CXNetworkCollectNeighbors
			: mode === 'exact-level'
				? this.module._CXNetworkCollectNeighborsAtLevel
				: this.module._CXNetworkCollectNeighborsUpToLevel;
		if (typeof fn !== 'function') {
			sourceInfo.dispose();
			nodeSelector.dispose();
			if (edgeSelector) {
				edgeSelector.dispose();
			}
			throw new Error('Neighbor collection helpers are unavailable in this WASM build. Rebuild the module to enable neighbor traversal APIs.');
		}

		let ok = false;
		try {
			if (mode === 'one-hop') {
				ok = fn.call(
					this.module,
					this.ptr,
					sourceInfo.ptr,
					sourceInfo.count,
					directionId,
					includeSourcesFlag,
					nodeSelector.ptr,
					edgeSelector ? edgeSelector.ptr : 0
				);
			} else {
				ok = fn.call(
					this.module,
					this.ptr,
					sourceInfo.ptr,
					sourceInfo.count,
					directionId,
					level >>> 0,
					includeSourcesFlag,
					nodeSelector.ptr,
					edgeSelector ? edgeSelector.ptr : 0
				);
			}
		} finally {
			sourceInfo.dispose();
		}
		if (!ok) {
			nodeSelector.dispose();
			if (edgeSelector) {
				edgeSelector.dispose();
			}
			throw new Error('Neighbor collection failed');
		}

		const nodes = nodeSelector.toTypedArray();
		nodeSelector.dispose();
		if (!includeEdges) {
			return asSelector ? NodeSelector.fromIndices(this, nodes) : nodes;
		}
		const edges = edgeSelector ? edgeSelector.toTypedArray() : new Uint32Array();
		if (edgeSelector) {
			edgeSelector.dispose();
		}
		if (asSelector) {
			return {
				nodes: NodeSelector.fromIndices(this, nodes),
				edges: EdgeSelector.fromIndices(this, edges),
			};
		}
		return { nodes, edges };
	}

	_normalizeNeighborSourceNodes(sourceNodes) {
		if (sourceNodes == null) {
			return new Uint32Array();
		}
		if (typeof sourceNodes === 'number') {
			const value = Number(sourceNodes);
			if (!Number.isFinite(value) || value < 0) {
				return new Uint32Array();
			}
			return Uint32Array.of(value >>> 0);
		}
		const iterable = ArrayBuffer.isView(sourceNodes) ? sourceNodes : Array.from(sourceNodes);
		const out = [];
		for (let i = 0; i < iterable.length; i += 1) {
			const value = Number(iterable[i]);
			if (!Number.isFinite(value) || value < 0) {
				continue;
			}
			out.push(value >>> 0);
		}
		return Uint32Array.from(out);
	}

	/**
	 * Hydrates a native neighbor container into JavaScript typed arrays.
	 * @private
	 *
	 * @param {number} containerPtr - Pointer to the native container.
	 * @returns {{nodes: Uint32Array, edges: Uint32Array}} Neighbor information.
	 */
	_readNeighborContainer(containerPtr) {
		this._assertCanAllocate('neighbor container hydration');
		const count = this.module._CXNeighborContainerCount(containerPtr);
		if (!count) {
			return { nodes: new Uint32Array(), edges: new Uint32Array() };
		}
		const nodesPtr = this.module._malloc(count * 4);
		const edgesPtr = this.module._malloc(count * 4);
		if (!nodesPtr || !edgesPtr) {
			if (nodesPtr) this.module._free(nodesPtr);
			if (edgesPtr) this.module._free(edgesPtr);
			throw new Error('Failed to allocate neighbor buffers');
		}
		this.module._CXNeighborContainerGetNodes(containerPtr, nodesPtr, count);
		this.module._CXNeighborContainerGetEdges(containerPtr, edgesPtr, count);
		const nodes = new Uint32Array(this.module.HEAPU32.buffer, nodesPtr, count).slice();
		const edges = new Uint32Array(this.module.HEAPU32.buffer, edgesPtr, count).slice();
		this.module._free(nodesPtr);
		this.module._free(edgesPtr);
		return { nodes, edges };
	}

	/**
	 * Defines a node attribute backed by linear WASM memory.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {AttributeType} type - Attribute type constant.
	 * @param {number} [dimension=1] - Number of elements per node.
	 * @throws {Error} When the attribute already exists or native allocation fails.
	 * @example
	 * import HeliosNetwork, { AttributeType } from 'helios-network';
	 *
	 * const net = await HeliosNetwork.create({ initialNodes: 2 });
	 * net.defineNodeAttribute('weight', AttributeType.Float);
	 *
	 * const weightBuffer = net.getNodeAttributeBuffer('weight').view;
	 * weightBuffer[0] = 1.5;
	 */
	defineNodeAttribute(name, type, dimension = 1) {
		this._defineAttribute('node', name, type, dimension, this.module._CXNetworkDefineNodeAttribute);
	}

	/**
	 * Defines an edge attribute backed by linear WASM memory.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {AttributeType} type - Attribute type constant.
	 * @param {number} [dimension=1] - Number of elements per edge.
	 * @throws {Error} When the attribute already exists or native allocation fails.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * net.defineEdgeAttribute('capacity', AttributeType.Integer);
	 * const edges = net.addEdges([[0, 1]]);
	 * const capacity = net.getEdgeAttributeBuffer('capacity').view;
	 * capacity[edges[0]] = 10n;
	 */
	defineEdgeAttribute(name, type, dimension = 1) {
		this._defineAttribute('edge', name, type, dimension, this.module._CXNetworkDefineEdgeAttribute);
	}

	/**
	 * Defines a network-level attribute backed by linear WASM memory.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {AttributeType} type - Attribute type constant.
	 * @param {number} [dimension=1] - Number of elements per network (capacity is always 1).
	 * @throws {Error} When the attribute already exists or native allocation fails.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.defineNetworkAttribute('temperature', AttributeType.Float);
	 * const networkValues = net.getNetworkAttributeBuffer('temperature').view;
	 * networkValues[0] = 21.5;
	 */
	defineNetworkAttribute(name, type, dimension = 1) {
		this._defineAttribute('network', name, type, dimension, this.module._CXNetworkDefineNetworkAttribute);
	}

	/**
	 * Defines a multi-category node attribute.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {boolean} [hasWeights=false] - Whether weights are stored per category.
	 */
	defineNodeMultiCategoryAttribute(name, hasWeights = false) {
		this._defineMultiCategoryAttribute('node', name, hasWeights);
	}

	/**
	 * Defines a multi-category edge attribute.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {boolean} [hasWeights=false] - Whether weights are stored per category.
	 */
	defineEdgeMultiCategoryAttribute(name, hasWeights = false) {
		this._defineMultiCategoryAttribute('edge', name, hasWeights);
	}

	/**
	 * Defines a multi-category network attribute.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {boolean} [hasWeights=false] - Whether weights are stored per category.
	 */
	defineNetworkMultiCategoryAttribute(name, hasWeights = false) {
		this._defineMultiCategoryAttribute('network', name, hasWeights);
	}

	/**
	 * Converts a string node attribute into categorical codes.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {CategorizeOptions} [options] - Categorization options.
	 */
	categorizeNodeAttribute(name, options) {
		this._categorizeAttribute('node', name, options);
	}

	/**
	 * Converts a string edge attribute into categorical codes.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {CategorizeOptions} [options] - Categorization options.
	 */
	categorizeEdgeAttribute(name, options) {
		this._categorizeAttribute('edge', name, options);
	}

	/**
	 * Converts a string network attribute into categorical codes.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {CategorizeOptions} [options] - Categorization options.
	 */
	categorizeNetworkAttribute(name, options) {
		this._categorizeAttribute('network', name, options);
	}

	/**
	 * Converts a categorical node attribute back into strings.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {DecategorizeOptions} [options] - Decategorization options.
	 */
	decategorizeNodeAttribute(name, options) {
		this._decategorizeAttribute('node', name, options);
	}

	/**
	 * Converts a categorical edge attribute back into strings.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {DecategorizeOptions} [options] - Decategorization options.
	 */
	decategorizeEdgeAttribute(name, options) {
		this._decategorizeAttribute('edge', name, options);
	}

	/**
	 * Converts a categorical network attribute back into strings.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {DecategorizeOptions} [options] - Decategorization options.
	 */
	decategorizeNetworkAttribute(name, options) {
		this._decategorizeAttribute('network', name, options);
	}

	/**
	 * Lists all node attribute names currently registered on the network.
	 * @returns {string[]} Node attribute identifiers.
	 */
	getNodeAttributeNames() {
		return this._attributeNames('node');
	}

	/**
	 * Lists all edge attribute names currently registered on the network.
	 * @returns {string[]} Edge attribute identifiers.
	 */
	getEdgeAttributeNames() {
		return this._attributeNames('edge');
	}

	/**
	 * Lists all network-level attribute names currently registered.
	 * @returns {string[]} Network attribute identifiers.
	 */
	getNetworkAttributeNames() {
		return this._attributeNames('network');
	}

	/**
	 * Returns the stored type metadata for a node attribute.
	 * @param {string} name - Attribute identifier.
	 * @returns {{type:number, dimension:number, complex:boolean}|null}
	 */
	getNodeAttributeInfo(name) {
		return this._attributeInfo('node', name);
	}

	/**
	 * Returns the stored type metadata for an edge attribute.
	 * @param {string} name - Attribute identifier.
	 * @returns {{type:number, dimension:number, complex:boolean}|null}
	 */
	getEdgeAttributeInfo(name) {
		return this._attributeInfo('edge', name);
	}

	/**
	 * Returns the stored type metadata for a network attribute.
	 * @param {string} name - Attribute identifier.
	 * @returns {{type:number, dimension:number, complex:boolean}|null}
	 */
	getNetworkAttributeInfo(name) {
		return this._attributeInfo('network', name);
	}

	/**
	 * Returns categorical dictionary entries for a node attribute.
	 * @param {string} name - Attribute identifier.
	 * @param {{sortById?:boolean}=} options - Optional sort control.
	 * @returns {{entries:{id:number,label:string}[], ids:number[], labels:string[]}}
	 */
	getNodeAttributeCategoryDictionary(name, options) {
		return this._getAttributeCategoryDictionary('node', name, options);
	}

	/**
	 * Returns categorical dictionary entries for an edge attribute.
	 * @param {string} name - Attribute identifier.
	 * @param {{sortById?:boolean}=} options - Optional sort control.
	 * @returns {{entries:{id:number,label:string}[], ids:number[], labels:string[]}}
	 */
	getEdgeAttributeCategoryDictionary(name, options) {
		return this._getAttributeCategoryDictionary('edge', name, options);
	}

	/**
	 * Returns categorical dictionary entries for a network attribute.
	 * @param {string} name - Attribute identifier.
	 * @param {{sortById?:boolean}=} options - Optional sort control.
	 * @returns {{entries:{id:number,label:string}[], ids:number[], labels:string[]}}
	 */
	getNetworkAttributeCategoryDictionary(name, options) {
		return this._getAttributeCategoryDictionary('network', name, options);
	}

	/**
	 * Replaces the categorical dictionary for a node attribute.
	 * @param {string} name - Attribute identifier.
	 * @param {(string|{id:number,label:string})[]} entries - Labels or {id,label} entries.
	 * @param {{remapExisting?:boolean}=} options - When true, remaps stored codes to the new ids.
	 * @returns {boolean} Whether the update succeeded.
	 */
	setNodeAttributeCategoryDictionary(name, entries, options) {
		return this._setAttributeCategoryDictionary('node', name, entries, options);
	}

	/**
	 * Replaces the categorical dictionary for an edge attribute.
	 * @param {string} name - Attribute identifier.
	 * @param {(string|{id:number,label:string})[]} entries - Labels or {id,label} entries.
	 * @param {{remapExisting?:boolean}=} options - When true, remaps stored codes to the new ids.
	 * @returns {boolean} Whether the update succeeded.
	 */
	setEdgeAttributeCategoryDictionary(name, entries, options) {
		return this._setAttributeCategoryDictionary('edge', name, entries, options);
	}

	/**
	 * Replaces the categorical dictionary for a network attribute.
	 * @param {string} name - Attribute identifier.
	 * @param {(string|{id:number,label:string})[]} entries - Labels or {id,label} entries.
	 * @param {{remapExisting?:boolean}=} options - When true, remaps stored codes to the new ids.
	 * @returns {boolean} Whether the update succeeded.
	 */
	setNetworkAttributeCategoryDictionary(name, entries, options) {
		return this._setAttributeCategoryDictionary('network', name, entries, options);
	}

	getNodeAttributeVersion(name) {
		return this._getAttributeVersion('node', name);
	}

	getEdgeAttributeVersion(name) {
		return this._getAttributeVersion('edge', name);
	}

	getNetworkAttributeVersion(name) {
		return this._getAttributeVersion('network', name);
	}

	bumpNodeAttributeVersion(name) {
		return this._bumpAttributeVersion('node', name, { op: 'bump' });
	}

	bumpEdgeAttributeVersion(name) {
		return this._bumpAttributeVersion('edge', name, { op: 'bump' });
	}

	bumpNetworkAttributeVersion(name) {
		return this._bumpAttributeVersion('network', name, { op: 'bump' });
	}

	getTopologyVersions() {
		this._refreshTopologyVersions();
		return { node: this._nodeTopologyVersion, edge: this._edgeTopologyVersion };
	}

	/**
	 * Checks whether a node-to-edge passthrough is registered for a given edge attribute.
	 * @param {string} edgeName - Edge attribute identifier.
	 * @returns {boolean}
	 */
	hasNodeToEdgeAttribute(edgeName) {
		this._ensureActive();
		return this._nodeToEdgePassthrough.has(edgeName);
	}

	/**
	 * Checks whether a node attribute is present.
	 * @param {string} name - Attribute identifier.
	 * @returns {boolean}
	 */
	hasNodeAttribute(name) {
		return this._hasAttribute('node', name);
	}

	/**
	 * Checks whether an edge attribute is present.
	 * @param {string} name - Attribute identifier.
	 * @returns {boolean}
	 */
	/**
	 * Checks whether an edge attribute is present.
	 * @param {string} name - Attribute identifier.
	 * @param {boolean} [pure=false] - When true, only considers native edge attributes and ignores passthroughs.
	 * @returns {boolean}
	 */
	hasEdgeAttribute(name, pure = false) {
		if (pure) {
			return this._hasAttribute('edge', name) && !this._nodeToEdgePassthrough.has(name);
		}
		return this._hasAttribute('edge', name) || this._nodeToEdgePassthrough.has(name);
	}

	/**
	 * Checks whether a network attribute is present.
	 * @param {string} name - Attribute identifier.
	 * @returns {boolean}
	 */
	hasNetworkAttribute(name) {
		return this._hasAttribute('network', name);
	}

	/**
	 * Defines a multi-category attribute backed by sparse CSR-like buffers.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @param {boolean} hasWeights - Whether the attribute stores weights per category.
	 */
	_defineMultiCategoryAttribute(scope, name, hasWeights) {
		this._assertCanAllocate(`define ${scope} multi-category attribute`);
		this._ensureActive();
		const metaMap = this._attributeMap(scope);
		if (metaMap.has(name)) {
			throw new Error(`Attribute "${name}" already defined on ${scope}`);
		}
		const defineFn = this.module._CXNetworkDefineMultiCategoryAttribute;
		if (typeof defineFn !== 'function') {
			throw new Error('Multi-category attributes are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let success = false;
		try {
			success = defineFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr, hasWeights ? 1 : 0);
		} finally {
			cstr.dispose();
		}
		if (!success) {
			throw new Error(`Failed to define ${scope} multi-category attribute "${name}"`);
		}
		metaMap.set(name, {
			type: AttributeType.MultiCategory,
			dimension: 1,
			hasWeights: !!hasWeights,
			complex: false,
			jsStore: new Map(),
			stringPointers: new Map(),
			nextHandle: 1,
		});
		this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeDefined, {
			scope,
			name,
			type: AttributeType.MultiCategory,
			dimension: 1,
			version: this._getAttributeVersion(scope, name),
		});
	}

	/**
	 * Internal helper used to register attributes with the native layer.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @param {AttributeType} type - Attribute type constant.
	 * @param {number} dimension - Number of elements per entity.
	 * @param {function} defineFn - Bound native function that declares the attribute.
	 * @throws {Error} When the attribute exists already or the native call fails.
	 */
	_defineAttribute(scope, name, type, dimension, defineFn) {
		this._assertCanAllocate(`define ${scope} attribute`);
		this._ensureActive();
		const metaMap = this._attributeMap(scope);
		if (metaMap.has(name)) {
			throw new Error(`Attribute "${name}" already defined on ${scope}`);
		}
		const cstr = new CString(this.module, name);
		try {
			const success = defineFn.call(this.module, this.ptr, cstr.ptr, type, dimension);
			if (!success) {
				throw new Error(`Failed to define ${scope} attribute "${name}"`);
			}
		} finally {
			cstr.dispose();
		}
		metaMap.set(name, {
			type,
			dimension,
			complex: COMPLEX_ATTRIBUTE_TYPES.has(type),
			jsStore: new Map(),
			stringPointers: new Map(),
			nextHandle: 1,
		});
		this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeDefined, {
			scope,
			name,
			type,
			dimension,
			version: this._getAttributeVersion(scope, name),
		});
	}

	/**
	 * Retrieves a wrapper around the node attribute buffer.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {object} Wrapper providing type information and buffer helpers.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.defineNodeAttribute('flag', AttributeType.Boolean);
	 * const nodes = net.addNodes(1);
	 * const attribute = net.getNodeAttributeBuffer('flag');
	 * attribute.view[nodes[0]] = 1;
	 */
	getNodeAttributeBuffer(name) {
		this._assertInsideBufferAccess(`node attribute buffer ${name}`);
		return this._getAttributeBuffer('node', name);
	}

	/**
	 * Retrieves a wrapper around the edge attribute buffer.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {object} Wrapper providing type information and buffer helpers.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * net.defineEdgeAttribute('capacity', AttributeType.Double);
	 * const edges = net.addEdges([[0, 1]]);
	 * const attribute = net.getEdgeAttributeBuffer('capacity');
	 * attribute.view[edges[0]] = 12.5;
	 */
	getEdgeAttributeBuffer(name) {
		this._assertInsideBufferAccess(`edge attribute buffer ${name}`);
		return this._getAttributeBuffer('edge', name);
	}

	/**
	 * Retrieves a wrapper around the network attribute buffer.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {object} Wrapper providing type information and buffer helpers.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.defineNetworkAttribute('version', AttributeType.UnsignedInteger);
	 * const attribute = net.getNetworkAttributeBuffer('version');
	 * attribute.view[0] = 1n;
	 */
	getNetworkAttributeBuffer(name) {
		this._assertInsideBufferAccess(`network attribute buffer ${name}`);
		return this._getAttributeBuffer('network', name);
	}

	/**
	 * Retrieves the buffer views for a multi-category node attribute.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {MultiCategoryBuffers}
	 */
	getNodeMultiCategoryBuffers(name) {
		return this._getMultiCategoryBuffers('node', name);
	}

	/**
	 * Retrieves the buffer views for a multi-category edge attribute.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {MultiCategoryBuffers}
	 */
	getEdgeMultiCategoryBuffers(name) {
		return this._getMultiCategoryBuffers('edge', name);
	}

	/**
	 * Retrieves the buffer views for a multi-category network attribute.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {MultiCategoryBuffers}
	 */
	getNetworkMultiCategoryBuffers(name) {
		return this._getMultiCategoryBuffers('network', name);
	}

	/**
	 * Returns the [start, end) slice for a multi-category node entry.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Node index.
	 * @returns {{start:number,end:number}}
	 */
	getNodeMultiCategoryEntryRange(name, index) {
		return this._getMultiCategoryEntryRange('node', name, index);
	}

	/**
	 * Returns the [start, end) slice for a multi-category edge entry.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Edge index.
	 * @returns {{start:number,end:number}}
	 */
	getEdgeMultiCategoryEntryRange(name, index) {
		return this._getMultiCategoryEntryRange('edge', name, index);
	}

	/**
	 * Returns the [start, end) slice for a multi-category network entry.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {{start:number,end:number}}
	 */
	getNetworkMultiCategoryEntryRange(name) {
		return this._getMultiCategoryEntryRange('network', name, 0);
	}

	/**
	 * Assigns a string attribute to a node.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Node index.
	 * @param {string|null|undefined} value - String to store (or nullish to clear).
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.defineNodeAttribute('label', AttributeType.String);
	 * const node = net.addNodes(1)[0];
	 * net.setNodeStringAttribute('label', node, 'source');
	 * console.log(net.getNodeStringAttribute('label', node)); // "source"
	 */
	setNodeStringAttribute(name, index, value) {
		this._setStringAttribute('node', name, index, value);
	}

	/**
	 * Reads a string attribute from a node.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Node index.
	 * @returns {string|null} Stored string value if present.
	 */
	getNodeStringAttribute(name, index) {
		return this._getStringAttribute('node', name, index);
	}

	/**
	 * Assigns a string attribute to an edge.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Edge index.
	 * @param {string|null|undefined} value - String to store (or nullish to clear).
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * net.defineEdgeAttribute('label', AttributeType.String);
	 * const edges = net.addEdges([[0, 1]]);
	 * net.setEdgeStringAttribute('label', edges[0], 'link');
	 */
	setEdgeStringAttribute(name, index, value) {
		this._setStringAttribute('edge', name, index, value);
	}

	/**
	 * Reads a string attribute from an edge.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Edge index.
	 * @returns {string|null} Stored string value if present.
	 */
	getEdgeStringAttribute(name, index) {
		return this._getStringAttribute('edge', name, index);
	}

	/**
	 * Assigns a string attribute to the network itself.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {string|null|undefined} value - String to store (or nullish to clear).
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.defineNetworkAttribute('status', AttributeType.String);
	 * net.setNetworkStringAttribute('status', 'ready');
	 * console.log(net.getNetworkStringAttribute('status')); // "ready"
	 */
	setNetworkStringAttribute(name, value) {
		this._setStringAttribute('network', name, 0, value);
	}

	/**
	 * Reads a string attribute from the network.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {string|null} Stored string value if present.
	 */
	getNetworkStringAttribute(name) {
		return this._getStringAttribute('network', name, 0);
	}

	/**
	 * Sets a multi-category node entry using category labels.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Node index.
	 * @param {string[]} categories - Category labels.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setNodeMultiCategoryEntry(name, index, categories, weights) {
		this._setMultiCategoryEntryByLabels('node', name, index, categories, weights);
	}

	/**
	 * Sets a multi-category edge entry using category labels.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Edge index.
	 * @param {string[]} categories - Category labels.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setEdgeMultiCategoryEntry(name, index, categories, weights) {
		this._setMultiCategoryEntryByLabels('edge', name, index, categories, weights);
	}

	/**
	 * Sets the multi-category network entry using category labels.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {string[]} categories - Category labels.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setNetworkMultiCategoryEntry(name, categories, weights) {
		this._setMultiCategoryEntryByLabels('network', name, 0, categories, weights);
	}

	/**
	 * Sets a multi-category node entry using category ids.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Node index.
	 * @param {Uint32Array|number[]} ids - Category ids.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setNodeMultiCategoryEntryByIds(name, index, ids, weights) {
		this._setMultiCategoryEntryByIds('node', name, index, ids, weights);
	}

	/**
	 * Sets a multi-category edge entry using category ids.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Edge index.
	 * @param {Uint32Array|number[]} ids - Category ids.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setEdgeMultiCategoryEntryByIds(name, index, ids, weights) {
		this._setMultiCategoryEntryByIds('edge', name, index, ids, weights);
	}

	/**
	 * Sets the multi-category network entry using category ids.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {Uint32Array|number[]} ids - Category ids.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setNetworkMultiCategoryEntryByIds(name, ids, weights) {
		this._setMultiCategoryEntryByIds('network', name, 0, ids, weights);
	}

	/**
	 * Clears a multi-category node entry.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Node index.
	 */
	clearNodeMultiCategoryEntry(name, index) {
		this._clearMultiCategoryEntry('node', name, index);
	}

	/**
	 * Clears a multi-category edge entry.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Edge index.
	 */
	clearEdgeMultiCategoryEntry(name, index) {
		this._clearMultiCategoryEntry('edge', name, index);
	}

	/**
	 * Clears the multi-category network entry.
	 *
	 * @param {string} name - Attribute identifier.
	 */
	clearNetworkMultiCategoryEntry(name) {
		this._clearMultiCategoryEntry('network', name, 0);
	}

	/**
	 * Replaces the multi-category node buffers in bulk.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {Uint32Array|number[]} offsets - CSR offsets (length = nodeCapacity + 1).
	 * @param {Uint32Array|number[]} ids - Flat category ids.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setNodeMultiCategoryBuffers(name, offsets, ids, weights) {
		this._setMultiCategoryBuffers('node', name, offsets, ids, weights);
	}

	/**
	 * Replaces the multi-category edge buffers in bulk.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {Uint32Array|number[]} offsets - CSR offsets (length = edgeCapacity + 1).
	 * @param {Uint32Array|number[]} ids - Flat category ids.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setEdgeMultiCategoryBuffers(name, offsets, ids, weights) {
		this._setMultiCategoryBuffers('edge', name, offsets, ids, weights);
	}

	/**
	 * Replaces the multi-category network buffers in bulk.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {Uint32Array|number[]} offsets - CSR offsets (length = 2).
	 * @param {Uint32Array|number[]} ids - Flat category ids.
	 * @param {number[]|Float32Array} [weights] - Optional weights (required for weighted attributes).
	 */
	setNetworkMultiCategoryBuffers(name, offsets, ids, weights) {
		this._setMultiCategoryBuffers('network', name, offsets, ids, weights);
	}

	_getAttributeVersion(scope, name) {
		const versionFn = this.module._CXAttributeVersion;
		if (typeof versionFn !== 'function') {
			return this._localVersions.get(`attr:${scope}:${name}`) || 0;
		}
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta) {
			throw new Error(`Unknown ${scope} attribute "${name}"`);
		}
		const attributePtr = meta.attributePtr >>> 0;
		const version = attributePtr ? this._normalizeVersion(versionFn.call(this.module, attributePtr)) : 0;
		return version ?? 0;
	}

	_bumpAttributeVersion(scope, name, change = null) {
		const bumpFn = scope === 'node'
			? this.module._CXNetworkBumpNodeAttributeVersion
			: scope === 'edge'
				? this.module._CXNetworkBumpEdgeAttributeVersion
				: this.module._CXNetworkBumpNetworkAttributeVersion;
		this._ensureActive();
		if (typeof bumpFn !== 'function') {
			if (scope === 'node') {
				this._markPassthroughEdgesDirtyForNode(name);
			}
			const version = this._nextLocalVersion(`attr:${scope}:${name}`);
			this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeChanged, {
				scope,
				name,
				version,
				op: change?.op ?? 'bump',
				index: typeof change?.index === 'number' ? change.index : null,
			});
			return version;
		}
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta) {
			throw new Error(`Unknown ${scope} attribute "${name}"`);
		}
		const cstr = new CString(this.module, name);
		let bumped = 0;
		try {
			const value = bumpFn.call(this.module, this.ptr, cstr.ptr);
			const normalized = this._normalizeVersion(value);
			bumped = normalized ?? this._nextLocalVersion(`attr:${scope}:${name}`);
		} finally {
			cstr.dispose();
		}
		this._localVersions.set(`attr:${scope}:${name}`, bumped);
		if (scope === 'node') {
			this._markPassthroughEdgesDirtyForNode(name);
		}
		this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeChanged, {
			scope,
			name,
			version: bumped,
			op: change?.op ?? 'bump',
			index: typeof change?.index === 'number' ? change.index : null,
		});
		return bumped;
	}

	_recordAttributeChange(scope, name, change = null) {
		const versionFn = this.module._CXAttributeVersion;
		let version = 0;
		if (typeof versionFn === 'function') {
			version = this._getAttributeVersion(scope, name);
		} else {
			version = this._nextLocalVersion(`attr:${scope}:${name}`);
		}
		this._localVersions.set(`attr:${scope}:${name}`, version);
		if (scope === 'node') {
			this._markPassthroughEdgesDirtyForNode(name);
		}
		this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeChanged, {
			scope,
			name,
			version,
			op: change?.op ?? 'bump',
			index: typeof change?.index === 'number' ? change.index : null,
		});
		return version;
	}

	_recordAttributeChangeSilently(scope, name, change = null) {
		const versionFn = this.module._CXAttributeVersion;
		let version = 0;
		if (typeof versionFn === 'function') {
			version = this._getAttributeVersion(scope, name);
		} else {
			version = this._nextLocalVersion(`attr:${scope}:${name}`);
		}
		this._localVersions.set(`attr:${scope}:${name}`, version);
		if (scope === 'node') {
			this._markPassthroughEdgesDirtyForNode(name);
		}
		if (change?.emitEvent) {
			this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeChanged, {
				scope,
				name,
				version,
				op: change?.op ?? 'bump',
				index: typeof change?.index === 'number' ? change.index : null,
			});
		}
		return version;
	}

	/**
	 * Internal helper that resolves attribute metadata and buffer handles.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @returns {object} Buffer wrapper tailored to the attribute type.
	 */
	_getAttributeBuffer(scope, name) {
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta) {
			throw new Error(`Unknown ${scope} attribute "${name}"`);
		}
		const { pointer, stride } = this._attributePointers(scope, name, meta);
		const version = this._getAttributeVersion(scope, name);
		const capacity = scope === 'node' ? this.nodeCapacity : scope === 'edge' ? this.edgeCapacity : 1;

		if (meta.complex) {
			return {
				type: meta.type,
				dimension: meta.dimension,
				version,
				get: (index = 0) => meta.jsStore.get(index)?.value ?? null,
				set: (index, value) => this._setComplexAttribute(scope, name, meta, index, value, pointer),
				delete: (index) => this._deleteComplexAttribute(scope, name, meta, index, pointer),
				store: meta.jsStore,
				bumpVersion: () => this._bumpAttributeVersion(scope, name, { op: 'bump' }),
			};
		}

		if (meta.type === AttributeType.String) {
			return {
				type: meta.type,
				dimension: meta.dimension,
				version,
				getView: () => new Uint32Array(this.module.HEAPU32.buffer, pointer, capacity * meta.dimension),
				getString: (index) => {
					const view = new Uint32Array(this.module.HEAPU32.buffer, pointer, capacity * meta.dimension);
					return this.module.UTF8ToString(view[index]);
				},
				bumpVersion: () => this._bumpAttributeVersion(scope, name, { op: 'bump' }),
			};
		}
		if (meta.type === AttributeType.MultiCategory) {
			throw new Error(`Multi-category attributes require get${scope === 'node' ? 'Node' : scope === 'edge' ? 'Edge' : 'Network'}MultiCategoryBuffers("${name}")`);
		}

		const TypedArrayCtor = TypedArrayForType[meta.type];
		if (!TypedArrayCtor) {
			throw new Error(`Unsupported attribute type ${meta.type}`);
		}
		const length = capacity * meta.dimension * (TYPE_ELEMENT_SIZE[meta.type] / TypedArrayCtor.BYTES_PER_ELEMENT);
		return {
			type: meta.type,
			dimension: meta.dimension,
			stride,
			view: new TypedArrayCtor(this.module.HEAPU8.buffer, pointer, length),
			version,
			bumpVersion: () => this._bumpAttributeVersion(scope, name, { op: 'bump' }),
		};
	}

	_getMultiCategoryBuffers(scope, name) {
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.MultiCategory) {
			throw new Error(`Attribute "${name}" on ${scope} is not a multi-category attribute`);
		}
		const offsetsFn = this.module._CXNetworkGetMultiCategoryOffsets;
		const idsFn = this.module._CXNetworkGetMultiCategoryIds;
		const weightsFn = this.module._CXNetworkGetMultiCategoryWeights;
		const offsetCountFn = this.module._CXNetworkGetMultiCategoryOffsetCount;
		const entryCountFn = this.module._CXNetworkGetMultiCategoryEntryCount;
		const hasWeightsFn = this.module._CXNetworkMultiCategoryHasWeights;
		if (typeof offsetsFn !== 'function' || typeof idsFn !== 'function' || typeof offsetCountFn !== 'function' || typeof entryCountFn !== 'function') {
			throw new Error('Multi-category buffers are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let offsetsPtr = 0;
		let idsPtr = 0;
		let weightsPtr = 0;
		let offsetCount = 0;
		let entryCount = 0;
		let hasWeights = false;
		try {
			offsetCount = offsetCountFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr) >>> 0;
			entryCount = entryCountFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr) >>> 0;
			offsetsPtr = offsetsFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr) >>> 0;
			idsPtr = idsFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr) >>> 0;
			hasWeights = typeof hasWeightsFn === 'function'
				? !!hasWeightsFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr)
				: !!meta.hasWeights;
			weightsPtr = hasWeights && typeof weightsFn === 'function'
				? weightsFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr) >>> 0
				: 0;
		} finally {
			cstr.dispose();
		}
		if (offsetCount && !offsetsPtr) {
			throw new Error(`Offsets buffer for "${name}" is not available`);
		}
		if (entryCount && !idsPtr) {
			throw new Error(`Ids buffer for "${name}" is not available`);
		}
		if (hasWeights && entryCount && !weightsPtr) {
			throw new Error(`Weights buffer for "${name}" is not available`);
		}
		return {
			offsets: new Uint32Array(this.module.HEAPU32.buffer, offsetsPtr, offsetCount),
			ids: new Uint32Array(this.module.HEAPU32.buffer, idsPtr, entryCount),
			weights: hasWeights ? new Float32Array(this.module.HEAPF32.buffer, weightsPtr, entryCount) : null,
			offsetCount,
			entryCount,
			hasWeights,
			version: this._getAttributeVersion(scope, name),
			bumpVersion: () => this._recordAttributeChange(scope, name, { op: 'bump' }),
		};
	}

	_getMultiCategoryEntryRange(scope, name, index) {
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.MultiCategory) {
			throw new Error(`Attribute "${name}" on ${scope} is not a multi-category attribute`);
		}
		const rangeFn = this.module._CXNetworkGetMultiCategoryEntryRange;
		if (typeof rangeFn !== 'function') {
			throw new Error('Multi-category range queries are unavailable in this WASM build');
		}
		const startPtr = this.module._malloc(4);
		const endPtr = this.module._malloc(4);
		if (!startPtr || !endPtr) {
			if (startPtr) this.module._free(startPtr);
			if (endPtr) this.module._free(endPtr);
			throw new Error('Failed to allocate range buffers');
		}
		const cstr = new CString(this.module, name);
		let ok = false;
		try {
			ok = rangeFn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr, index, startPtr, endPtr);
		} finally {
			cstr.dispose();
		}
		const start = this.module.HEAPU32[startPtr >>> 2] >>> 0;
		const end = this.module.HEAPU32[endPtr >>> 2] >>> 0;
		this.module._free(startPtr);
		this.module._free(endPtr);
		if (!ok) {
			throw new Error(`Failed to read multi-category entry range for "${name}"`);
		}
		return { start, end };
	}

	_setMultiCategoryEntryByLabels(scope, name, index, categories, weights) {
		this._assertCanAllocate(`set ${scope} multi-category entry`);
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.MultiCategory) {
			throw new Error(`Attribute "${name}" on ${scope} is not a multi-category attribute`);
		}
		if (!Array.isArray(categories)) {
			throw new Error('Multi-category labels must be provided as an array of strings');
		}
		if (weights && weights.length !== categories.length) {
			throw new Error('Multi-category weights must match the number of categories');
		}
		const labels = new CStringArray(this.module, categories);
		const weightInfo = weights ? this._copyFloat32ToWasm(weights) : { ptr: 0, count: 0, dispose: () => {} };
		const fn = this.module._CXNetworkSetMultiCategoryEntryByLabels;
		if (typeof fn !== 'function') {
			labels.dispose();
			weightInfo.dispose();
			throw new Error('Multi-category updates are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let ok = false;
		try {
			ok = fn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr, index, labels.ptr, labels.count, weightInfo.ptr);
		} finally {
			cstr.dispose();
			labels.dispose();
			weightInfo.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to update ${scope} multi-category entry "${name}"`);
		}
		this._recordAttributeChange(scope, name, { op: 'set', index });
	}

	_setMultiCategoryEntryByIds(scope, name, index, ids, weights) {
		this._assertCanAllocate(`set ${scope} multi-category entry`);
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.MultiCategory) {
			throw new Error(`Attribute "${name}" on ${scope} is not a multi-category attribute`);
		}
		const idInfo = this._copyIndicesToWasm(ids);
		if (weights && weights.length !== idInfo.count) {
			idInfo.dispose();
			throw new Error('Multi-category weights must match the number of ids');
		}
		const weightInfo = weights ? this._copyFloat32ToWasm(weights) : { ptr: 0, count: 0, dispose: () => {} };
		const fn = this.module._CXNetworkSetMultiCategoryEntry;
		if (typeof fn !== 'function') {
			idInfo.dispose();
			weightInfo.dispose();
			throw new Error('Multi-category updates are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let ok = false;
		try {
			ok = fn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr, index, idInfo.ptr, idInfo.count, weightInfo.ptr);
		} finally {
			cstr.dispose();
			idInfo.dispose();
			weightInfo.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to update ${scope} multi-category entry "${name}"`);
		}
		this._recordAttributeChange(scope, name, { op: 'set', index });
	}

	_clearMultiCategoryEntry(scope, name, index) {
		this._assertCanAllocate(`clear ${scope} multi-category entry`);
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.MultiCategory) {
			throw new Error(`Attribute "${name}" on ${scope} is not a multi-category attribute`);
		}
		const fn = this.module._CXNetworkClearMultiCategoryEntry;
		if (typeof fn !== 'function') {
			throw new Error('Multi-category updates are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let ok = false;
		try {
			ok = fn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr, index);
		} finally {
			cstr.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to clear ${scope} multi-category entry "${name}"`);
		}
		this._recordAttributeChange(scope, name, { op: 'clear', index });
	}

	_setMultiCategoryBuffers(scope, name, offsets, ids, weights) {
		this._assertCanAllocate(`set ${scope} multi-category buffers`);
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.MultiCategory) {
			throw new Error(`Attribute "${name}" on ${scope} is not a multi-category attribute`);
		}
		const offsetInfo = this._copyIndicesToWasm(offsets);
		const idInfo = this._copyIndicesToWasm(ids);
		if (weights && weights.length !== idInfo.count) {
			offsetInfo.dispose();
			idInfo.dispose();
			throw new Error('Multi-category weights must match the number of ids');
		}
		const weightInfo = weights ? this._copyFloat32ToWasm(weights) : { ptr: 0, count: 0, dispose: () => {} };
		const fn = this.module._CXNetworkSetMultiCategoryBuffers;
		if (typeof fn !== 'function') {
			offsetInfo.dispose();
			idInfo.dispose();
			weightInfo.dispose();
			throw new Error('Multi-category updates are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let ok = false;
		try {
			ok = fn.call(this.module, this.ptr, this._scopeId(scope), cstr.ptr, offsetInfo.ptr, offsetInfo.count, idInfo.ptr, idInfo.count, weightInfo.ptr);
		} finally {
			cstr.dispose();
			offsetInfo.dispose();
			idInfo.dispose();
			weightInfo.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to update ${scope} multi-category buffers "${name}"`);
		}
		this._recordAttributeChange(scope, name, { op: 'set' });
	}

	/**
	 * Resolves the native attribute pointer and backing buffer pointer.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @param {object} meta - Stored attribute metadata.
	 * @returns {{pointer:number,stride:number}} Buffer pointer and element stride.
	 */
	_attributePointers(scope, name, meta) {
		const bufferGetter = scope === 'node'
			? this.module._CXNetworkGetNodeAttributeBuffer
			: scope === 'edge'
				? this.module._CXNetworkGetEdgeAttributeBuffer
				: this.module._CXNetworkGetNetworkAttributeBuffer;

		let attributePtr = meta?.attributePtr >>> 0;
		if (!attributePtr) {
			const getter = scope === 'node'
				? this.module._CXNetworkGetNodeAttribute
				: scope === 'edge'
					? this.module._CXNetworkGetEdgeAttribute
					: this.module._CXNetworkGetNetworkAttribute;
			this._assertCanAllocate(`attribute metadata lookup for ${scope}:${name}`);
			const cstr = new CString(this.module, name);
			try {
				attributePtr = getter.call(this.module, this.ptr, cstr.ptr) >>> 0;
			} finally {
				cstr.dispose();
			}
			meta.attributePtr = attributePtr;
			meta.stride = attributePtr ? this.module._CXAttributeStride(attributePtr) : 0;
		}

		let bufferPtr = 0;
		const directDataFn = this.module._CXAttributeData;
		if (typeof directDataFn === 'function' && attributePtr) {
			bufferPtr = directDataFn.call(this.module, attributePtr) >>> 0;
		}
		if (!bufferPtr) {
			this._assertCanAllocate(`attribute buffer lookup for ${scope}:${name}`);
			const cstr = new CString(this.module, name);
			try {
				bufferPtr = bufferGetter.call(this.module, this.ptr, cstr.ptr) >>> 0;
			} finally {
				cstr.dispose();
			}
		}
		if (!bufferPtr) {
			throw new Error(`Attribute buffer for "${name}" is not available`);
		}
		const stride = meta?.stride || (attributePtr ? this.module._CXAttributeStride(attributePtr) : 0);
		return { pointer: bufferPtr >>> 0, stride, attributePtr };
	}

	/**
	 * Updates the JS-side registry and backing buffer for complex attributes.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @param {object} meta - Stored attribute metadata.
	 * @param {number} index - Entity index.
	 * @param {*} value - Value to persist.
	 * @param {number} pointer - Pointer to the backing handle buffer.
	 */
	_setComplexAttribute(scope, name, meta, index, value, pointer) {
		const buffer = new Uint32Array(this.module.HEAPU32.buffer, pointer, this._capacityForScope(scope));
		let entry = meta.jsStore.get(index);
		if (!entry) {
			entry = { id: meta.nextHandle++, value: null };
			meta.jsStore.set(index, entry);
		}
		entry.value = value;
		buffer[index] = entry.id;
		this._bumpAttributeVersion(scope, name, { op: 'set', index });
	}

	/**
	 * Clears a complex attribute handle from the backing buffer and store.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {object} meta - Stored attribute metadata.
	 * @param {number} index - Entity index.
	 * @param {number} pointer - Pointer to the backing handle buffer.
	 */
	_deleteComplexAttribute(scope, name, meta, index, pointer) {
		const buffer = new Uint32Array(this.module.HEAPU32.buffer, pointer, this._capacityForScope(scope));
		meta.jsStore.delete(index);
		buffer[index] = 0;
		this._bumpAttributeVersion(scope, name, { op: 'delete', index });
	}

	/**
	 * @private
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @returns {number} Allocated capacity for the scope.
	 */
	_capacityForScope(scope) {
		if (scope === 'node') return this.nodeCapacity;
		if (scope === 'edge') return this.edgeCapacity;
		return 1;
	}

	_attributeNames(scope) {
		this._ensureActive();
		this._syncAttributeNamesFromNative(scope);
		return Array.from(this._attributeMap(scope).keys());
	}

	_syncAttributeNamesFromNative(scope) {
		const metaMap = this._attributeMap(scope);
		let countFn = null;
		let nameAtFn = null;
		switch (scope) {
			case 'node':
				countFn = this.module._CXNetworkNodeAttributeCount;
				nameAtFn = this.module._CXNetworkNodeAttributeNameAt;
				break;
			case 'edge':
				countFn = this.module._CXNetworkEdgeAttributeCount;
				nameAtFn = this.module._CXNetworkEdgeAttributeNameAt;
				break;
			case 'network':
				countFn = this.module._CXNetworkNetworkAttributeCount;
				nameAtFn = this.module._CXNetworkNetworkAttributeNameAt;
				break;
			default:
				return;
		}
		if (typeof countFn !== 'function' || typeof nameAtFn !== 'function') {
			return;
		}
		const rawCount = Number(countFn.call(this.module, this.ptr));
		const count = Number.isFinite(rawCount) && rawCount > 0 ? Math.floor(rawCount) : 0;
		const names = [];
		for (let i = 0; i < count; i += 1) {
			const namePtr = nameAtFn.call(this.module, this.ptr, i) >>> 0;
			if (!namePtr) {
				continue;
			}
			const decoded = this.module.UTF8ToString(namePtr);
			if (typeof decoded !== 'string' || decoded.length === 0) {
				continue;
			}
			names.push(decoded);
		}
		const nativeSet = new Set(names);
		if (metaMap.size > nativeSet.size) {
			for (const existing of Array.from(metaMap.keys())) {
				if (!nativeSet.has(existing)) {
					metaMap.delete(existing);
				}
			}
		}
		for (const name of names) {
			if (!metaMap.has(name)) {
				this._ensureAttributeMetadata(scope, name);
			}
		}
	}

	_attributeInfo(scope, name) {
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta) {
			return null;
		}
		return {
			type: meta.type,
			dimension: meta.dimension,
			complex: meta.complex,
		};
	}

	_hasAttribute(scope, name) {
		this._ensureActive();
		const metaMap = this._attributeMap(scope);
		if (metaMap.has(name)) {
			return true;
		}
		return Boolean(this._ensureAttributeMetadata(scope, name));
	}

	/**
	 * @private
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @returns {Map<string, object>} Metadata registry matching the scope.
	 */
	_attributeMap(scope) {
		switch (scope) {
		case 'node': return this._nodeAttributes;
		case 'edge': return this._edgeAttributes;
		case 'network': return this._networkAttributes;
			default: throw new Error(`Unknown attribute scope "${scope}"`);
		}
	}

	_scopeId(scope) {
		if (scope === 'node') return 0;
		if (scope === 'edge') return 1;
		if (scope === 'network') return 2;
		throw new Error(`Unknown attribute scope "${scope}"`);
	}

	_normalizeDimensionMethod(value) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			const n = value | 0;
			if (n >= DimensionDifferenceMethod.Forward && n <= DimensionDifferenceMethod.LeastSquares) {
				return n;
			}
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'forward' || normalized === 'fw') return DimensionDifferenceMethod.Forward;
			if (normalized === 'backward' || normalized === 'bk') return DimensionDifferenceMethod.Backward;
			if (normalized === 'central' || normalized === 'centered' || normalized === 'ce') return DimensionDifferenceMethod.Central;
			if (normalized === 'leastsquares' || normalized === 'least_squares' || normalized === 'ls') return DimensionDifferenceMethod.LeastSquares;
		}
		return DimensionDifferenceMethod.LeastSquares;
	}

	_dimensionMaxOrder(method) {
		switch (method) {
			case DimensionDifferenceMethod.Forward:
				return DIMENSION_FORWARD_MAX_ORDER;
			case DimensionDifferenceMethod.Backward:
				return DIMENSION_BACKWARD_MAX_ORDER;
			case DimensionDifferenceMethod.Central:
				return DIMENSION_CENTRAL_MAX_ORDER;
			case DimensionDifferenceMethod.LeastSquares:
			default:
				return Number.POSITIVE_INFINITY;
		}
	}

	_dimensionExtraPadding(method, order) {
		if (method === DimensionDifferenceMethod.Forward
			|| method === DimensionDifferenceMethod.Central
			|| method === DimensionDifferenceMethod.LeastSquares) {
			return order;
		}
		return 0;
	}

	_normalizeDimensionOrder(method, value) {
		const order = Math.max(1, ((value ?? 1) | 0));
		const maxOrder = this._dimensionMaxOrder(method);
		if (order > maxOrder) {
			throw new Error(`order must be <= ${maxOrder} for method ${method}`);
		}
		return order;
	}

	_normalizeNeighborDirection(value) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			const n = value | 0;
			if (n >= NeighborDirection.Out && n <= NeighborDirection.Both) {
				return n;
			}
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'out' || normalized === 'outgoing') return NeighborDirection.Out;
			if (normalized === 'in' || normalized === 'incoming') return NeighborDirection.In;
			if (normalized === 'both' || normalized === 'all' || normalized === 'union') return NeighborDirection.Both;
		}
		return NeighborDirection.Both;
	}

	_normalizeStrengthMeasure(value) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			const n = value | 0;
			if (n >= StrengthMeasure.Sum && n <= StrengthMeasure.Minimum) {
				return n;
			}
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'sum' || normalized === 'total') return StrengthMeasure.Sum;
			if (normalized === 'avg' || normalized === 'average' || normalized === 'mean') return StrengthMeasure.Average;
			if (normalized === 'max' || normalized === 'maximum') return StrengthMeasure.Maximum;
			if (normalized === 'min' || normalized === 'minimum') return StrengthMeasure.Minimum;
		}
		return StrengthMeasure.Sum;
	}

	_normalizeClusteringVariant(value) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			const n = value | 0;
			if (n >= ClusteringCoefficientVariant.Unweighted && n <= ClusteringCoefficientVariant.Newman) {
				return n;
			}
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'unweighted' || normalized === 'binary') return ClusteringCoefficientVariant.Unweighted;
			if (normalized === 'onnela') return ClusteringCoefficientVariant.Onnela;
			if (normalized === 'newman' || normalized === 'barrat' || normalized === 'weighted') return ClusteringCoefficientVariant.Newman;
		}
		return ClusteringCoefficientVariant.Unweighted;
	}

	_normalizeMeasurementExecutionMode(value, fallback = MeasurementExecutionMode.Auto) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			const n = value | 0;
			if (n >= MeasurementExecutionMode.Auto && n <= MeasurementExecutionMode.Parallel) {
				return n;
			}
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'auto') return MeasurementExecutionMode.Auto;
			if (normalized === 'single' || normalized === 'singlethread' || normalized === 'single-thread' || normalized === 'js') {
				return MeasurementExecutionMode.SingleThread;
			}
			if (normalized === 'parallel' || normalized === 'native') return MeasurementExecutionMode.Parallel;
		}
		return fallback;
	}

	_normalizeConnectedComponentsMode(value) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			const n = value | 0;
			if (n >= ConnectedComponentsMode.Weak && n <= ConnectedComponentsMode.Strong) {
				return n;
			}
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'weak' || normalized === 'wcc') return ConnectedComponentsMode.Weak;
			if (normalized === 'strong' || normalized === 'scc') return ConnectedComponentsMode.Strong;
		}
		return ConnectedComponentsMode.Weak;
	}

	_normalizeNodeSelection(nodes) {
		if (nodes == null) {
			return this.withBufferAccess(() => this.nodeIndices.slice(), { nodeIndices: true });
		}
		const source = ArrayBuffer.isView(nodes) ? nodes : Array.from(nodes);
		const out = [];
		for (let i = 0; i < source.length; i += 1) {
			const raw = Number(source[i]);
			if (!Number.isFinite(raw) || raw < 0) {
				continue;
			}
			const node = raw >>> 0;
			if (this.hasNodeIndex(node)) {
				out.push(node);
			}
		}
		return Uint32Array.from(out);
	}

	_collectNodeMetricResult(valuesByNode, nodes = null) {
		const nodeIndices = this._normalizeNodeSelection(nodes);
		const values = new Float32Array(nodeIndices.length);
		for (let i = 0; i < nodeIndices.length; i += 1) {
			values[i] = valuesByNode[nodeIndices[i]] ?? 0;
		}
		return { nodeIndices, values, valuesByNode };
	}

	_collectNodeMetricResultUint32(valuesByNode, nodes = null) {
		const nodeIndices = this._normalizeNodeSelection(nodes);
		const values = new Uint32Array(nodeIndices.length);
		for (let i = 0; i < nodeIndices.length; i += 1) {
			values[i] = valuesByNode[nodeIndices[i]] >>> 0;
		}
		return { nodeIndices, values, valuesByNode };
	}

	_resolveNodeMetricOutputAttribute(name, type, label) {
		if (!name) {
			return null;
		}
		const attrName = String(name);
		if (!this._nodeAttributes.has(attrName)) {
			this.defineNodeAttribute(attrName, type, 1);
		}
		const meta = this._nodeAttributes.get(attrName) ?? this._ensureAttributeMetadata('node', attrName);
		if (!meta) {
			throw new Error(`Node attribute "${attrName}" is not defined`);
		}
		if (meta.type !== type) {
			throw new Error(`Node attribute "${attrName}" must be ${label}`);
		}
		if ((meta.dimension ?? 1) !== 1) {
			throw new Error(`Node attribute "${attrName}" must have dimension 1`);
		}
		const pointers = this._attributePointers('node', attrName, meta);
		return {
			name: attrName,
			pointer: pointers.pointer >>> 0,
		};
	}

	_copyFloat32NodeValuesFromPointer(pointer) {
		return this.withBufferAccess(
			() => new Float32Array(this.module.HEAPF32.buffer, pointer, this.nodeCapacity).slice()
		);
	}

	_copyUint32NodeValuesFromPointer(pointer) {
		return this.withBufferAccess(
			() => new Uint32Array(this.module.HEAPU32.buffer, pointer, this.nodeCapacity).slice()
		);
	}

	_copyFloat32NodeValuesToWasm(values) {
		if (!values) {
			return { ptr: 0, dispose: () => {} };
		}
		const source = ArrayBuffer.isView(values) ? values : Array.from(values);
		if (source.length !== this.nodeCapacity) {
			throw new Error(`Expected ${this.nodeCapacity} initial values (nodeCapacity)`);
		}
		const ptr = this.module._malloc(this.nodeCapacity * Float32Array.BYTES_PER_ELEMENT);
		if (!ptr) {
			throw new Error('Failed to allocate initial node value buffer');
		}
		const view = new Float32Array(this.module.HEAPF32.buffer, ptr, this.nodeCapacity);
		for (let i = 0; i < this.nodeCapacity; i += 1) {
			const value = Number(source[i]);
			view[i] = Number.isFinite(value) ? value : 0;
		}
		return {
			ptr,
			dispose: () => this.module._free(ptr),
		};
	}

	_normalizeCategorySortOrder(value) {
		if (typeof value === 'number' && Number.isFinite(value)) {
			return value;
		}
		if (typeof value === 'string') {
			const normalized = value.trim().toLowerCase();
			if (normalized === 'frequency') return CategorySortOrder.Frequency;
			if (normalized === 'alphabetical' || normalized === 'alpha') return CategorySortOrder.Alphabetical;
			if (normalized === 'natural') return CategorySortOrder.Natural;
		}
		return CategorySortOrder.None;
	}

	_getAttributeCategoryDictionary(scope, name, options = {}) {
		this._ensureActive();
		if (typeof name !== 'string' || !name) {
			throw new Error('Attribute name must be a non-empty string');
		}
		const countFn = this.module._CXNetworkGetAttributeCategoryDictionaryCount;
		const entriesFn = this.module._CXNetworkGetAttributeCategoryDictionaryEntries;
		if (typeof countFn !== 'function' || typeof entriesFn !== 'function') {
			throw new Error('Category dictionary helpers are unavailable in this WASM build');
		}
		const nameCstr = new CString(this.module, name);
		let count = 0;
		try {
			count = countFn.call(this.module, this.ptr, this._scopeId(scope), nameCstr.ptr) >>> 0;
		} catch (error) {
			nameCstr.dispose();
			throw error;
		}
		if (!count) {
			nameCstr.dispose();
			return { entries: [], ids: [], labels: [] };
		}
		const idsPtr = this.module._malloc(count * 4);
		const labelsPtr = this.module._malloc(count * 4);
		if (!idsPtr || !labelsPtr) {
			nameCstr.dispose();
			if (idsPtr) this.module._free(idsPtr);
			if (labelsPtr) this.module._free(labelsPtr);
			throw new Error('Failed to allocate category dictionary buffers');
		}
		const ok = entriesFn.call(this.module, this.ptr, this._scopeId(scope), nameCstr.ptr, idsPtr, labelsPtr, count);
		nameCstr.dispose();
		if (!ok) {
			this.module._free(idsPtr);
			this.module._free(labelsPtr);
			throw new Error(`Failed to read category dictionary for ${scope} attribute "${name}"`);
		}
		const idsView = new Int32Array(this.module.HEAP32.buffer, idsPtr, count);
		const labelsView = new Uint32Array(this.module.HEAPU32.buffer, labelsPtr, count);
		const entries = [];
		for (let i = 0; i < count; i += 1) {
			const labelPtr = labelsView[i];
			const label = labelPtr ? this.module.UTF8ToString(labelPtr) : '';
			entries.push({ id: idsView[i], label });
		}
		this.module._free(idsPtr);
		this.module._free(labelsPtr);
		if (options?.sortById !== false) {
			entries.sort((a, b) => a.id - b.id);
		}
		const ids = entries.map((entry) => entry.id);
		const labels = entries.map((entry) => entry.label);
		return { entries, ids, labels };
	}

	_setAttributeCategoryDictionary(scope, name, entries, options = {}) {
		this._ensureActive();
		this._assertCanAllocate(`set ${scope} category dictionary`);
		if (typeof name !== 'string' || !name) {
			throw new Error('Attribute name must be a non-empty string');
		}
		const setFn = this.module._CXNetworkSetAttributeCategoryDictionary;
		if (typeof setFn !== 'function') {
			throw new Error('Category dictionary helpers are unavailable in this WASM build');
		}
		const labels = [];
		const ids = [];
		if (Array.isArray(entries)) {
			for (let i = 0; i < entries.length; i += 1) {
				const entry = entries[i];
				if (typeof entry === 'string') {
					labels.push(entry);
					ids.push(i);
					continue;
				}
				if (entry && typeof entry === 'object') {
					const label = entry.label ?? entry.name ?? entry.value;
					if (typeof label !== 'string') continue;
					labels.push(label);
					const id = Number(entry.id);
					ids.push(Number.isFinite(id) ? id : labels.length - 1);
				}
			}
		}
		const count = labels.length;
		const nameCstr = new CString(this.module, name);
		const labelArray = new CStringArray(this.module, labels);
		let idsPtr = 0;
		try {
			if (count) {
				idsPtr = this.module._malloc(count * 4);
				if (!idsPtr) {
					throw new Error('Failed to allocate category id buffer');
				}
				this.module.HEAP32.set(Int32Array.from(ids), idsPtr / 4);
			}
			const remapExisting = options?.remapExisting !== false ? 1 : 0;
			const ok = setFn.call(
				this.module,
				this.ptr,
				this._scopeId(scope),
				nameCstr.ptr,
				labelArray.ptr,
				idsPtr,
				count,
				remapExisting,
			);
			if (!ok) {
				throw new Error(`Failed to update category dictionary for ${scope} attribute "${name}"`);
			}
			return true;
		} finally {
			nameCstr.dispose();
			labelArray.dispose();
			if (idsPtr) this.module._free(idsPtr);
		}
	}

	_categorizeAttribute(scope, name, options) {
		this._assertCanAllocate(`categorize ${scope} attribute`);
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.String) {
			throw new Error(`Attribute "${name}" on ${scope} is not a string attribute`);
		}
		const categorizeFn = this.module._CXNetworkCategorizeAttribute;
		if (typeof categorizeFn !== 'function') {
			throw new Error('Categorical helpers are unavailable in this WASM build');
		}
		const sortOrder = this._normalizeCategorySortOrder(options?.sortOrder);
		const missingLabel = options?.missingLabel ?? '__NA__';
		const nameCstr = new CString(this.module, name);
		const missingCstr = missingLabel != null ? new CString(this.module, missingLabel) : null;
		let ok = false;
		try {
			ok = categorizeFn.call(this.module, this.ptr, this._scopeId(scope), nameCstr.ptr, sortOrder, missingCstr ? missingCstr.ptr : 0);
		} finally {
			nameCstr.dispose();
			if (missingCstr) missingCstr.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to categorize ${scope} attribute "${name}"`);
		}
		meta.type = AttributeType.Category;
		meta.dimension = 1;
		meta.stride = this.module._CXAttributeStride(meta.attributePtr);
		meta.complex = false;
		if (meta.stringPointers) {
			meta.stringPointers.clear();
		}
		this._bumpAttributeVersion(scope, name, { op: 'categorize' });
	}

	_decategorizeAttribute(scope, name, options) {
		this._assertCanAllocate(`decategorize ${scope} attribute`);
		this._ensureActive();
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta || meta.type !== AttributeType.Category) {
			throw new Error(`Attribute "${name}" on ${scope} is not a categorical attribute`);
		}
		const decategorizeFn = this.module._CXNetworkDecategorizeAttribute;
		if (typeof decategorizeFn !== 'function') {
			throw new Error('Categorical helpers are unavailable in this WASM build');
		}
		const missingLabel = options?.missingLabel ?? '__NA__';
		const nameCstr = new CString(this.module, name);
		const missingCstr = missingLabel != null ? new CString(this.module, missingLabel) : null;
		let ok = false;
		try {
			ok = decategorizeFn.call(this.module, this.ptr, this._scopeId(scope), nameCstr.ptr, missingCstr ? missingCstr.ptr : 0);
		} finally {
			nameCstr.dispose();
			if (missingCstr) missingCstr.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to decategorize ${scope} attribute "${name}"`);
		}
		meta.type = AttributeType.String;
		meta.dimension = 1;
		meta.stride = this.module._CXAttributeStride(meta.attributePtr);
		meta.complex = true;
		meta.stringPointers = new Map();
		this._bumpAttributeVersion(scope, name, { op: 'decategorize' });
	}

	_copyIndicesToWasm(indices) {
		this._assertCanAllocate('copy indices to WASM memory');
		if (!indices) {
			return { ptr: 0, count: 0, dispose: () => {} };
		}
		let array = indices;
		if (!ArrayBuffer.isView(indices)) {
			array = Uint32Array.from(indices);
		}
		const count = array.length >>> 0;
		if (count === 0) {
			return { ptr: 0, count: 0, dispose: () => {} };
		}
		if (array.buffer === this.module.HEAPU32.buffer) {
			return {
				ptr: array.byteOffset,
				count,
				dispose: () => {},
			};
		}
		const bytes = count * Uint32Array.BYTES_PER_ELEMENT;
		const ptr = this.module._malloc(bytes);
		if (!ptr) {
			throw new Error('Failed to allocate WASM memory for indices');
		}
		this.module.HEAPU32.set(array, ptr >>> 2);
		return {
			ptr,
			count,
			dispose: () => this.module._free(ptr),
		};
	}

	_copyFloat32ToWasm(values) {
		this._assertCanAllocate('copy float32 data to WASM memory');
		if (!values) {
			return { ptr: 0, count: 0, dispose: () => {} };
		}
		let array = values;
		if (!ArrayBuffer.isView(values)) {
			array = Float32Array.from(values);
		}
		const count = array.length >>> 0;
		if (count === 0) {
			return { ptr: 0, count: 0, dispose: () => {} };
		}
		if (array.buffer === this.module.HEAPF32.buffer) {
			return {
				ptr: array.byteOffset,
				count,
				dispose: () => {},
			};
		}
		const bytes = count * Float32Array.BYTES_PER_ELEMENT;
		const ptr = this.module._malloc(bytes);
		if (!ptr) {
			throw new Error('Failed to allocate WASM memory for float data');
		}
		this.module.HEAPF32.set(array, ptr >>> 2);
		return {
			ptr,
			count,
			dispose: () => this.module._free(ptr),
		};
	}

	_ensureInterpolationTargetAttribute(name, dimension) {
		const targetName = `__helios_target_${name}`;
		let targetMeta = this._ensureAttributeMetadata('node', targetName);
		if (!targetMeta) {
			this._defineAttribute('node', targetName, AttributeType.Float, dimension, this.module._CXNetworkDefineNodeAttribute);
			targetMeta = this._ensureAttributeMetadata('node', targetName);
		}
		if (!targetMeta) {
			throw new Error(`Failed to define interpolation target attribute "${targetName}"`);
		}
		if (targetMeta.type !== AttributeType.Float || targetMeta.dimension !== dimension) {
			throw new Error(`Interpolation target attribute "${targetName}" has incompatible type or dimension`);
		}
		return { name: targetName, meta: targetMeta };
	}

	_disposeActiveIndexBuffer(scope) {
		if (scope === 'node') {
			this._activeNodeIndexBuffer = { ptr: 0, capacity: 0, count: 0, version: 0, dirty: true };
		} else {
			this._activeEdgeIndexBuffer = { ptr: 0, capacity: 0, count: 0, version: 0, dirty: true };
		}
	}

	_ensureActiveIndexBuffer(scope) {
		this._ensureActive();
		this._refreshTopologyVersions(scope);
		const pointerReader = scope === 'node'
			? this.module._CXNetworkActiveNodeIndices
			: this.module._CXNetworkActiveEdgeIndices;
		const countReader = scope === 'node'
			? this.module._CXNetworkActiveNodeIndexCount
			: this.module._CXNetworkActiveEdgeIndexCount;
		const version = scope === 'node' ? this._nodeTopologyVersion : this._edgeTopologyVersion;
		const buffer = scope === 'node' ? this._activeNodeIndexBuffer : this._activeEdgeIndexBuffer;

		if (typeof pointerReader !== 'function' || typeof countReader !== 'function') {
			throw new Error(`This WASM build does not expose stable active ${scope} index buffers`);
		}

		buffer.ptr = pointerReader.call(this.module, this.ptr) >>> 0;
		buffer.count = countReader.call(this.module, this.ptr) >>> 0;
		buffer.capacity = buffer.count;
		buffer.version = version;
		buffer.dirty = false;
		return buffer;
	}

	_getActiveIndexView(scope) {
		const buffer = this._ensureActiveIndexBuffer(scope);
		const view = buffer.ptr
			? new Uint32Array(this.module.HEAPU8.buffer, buffer.ptr, buffer.count)
			: EMPTY_UINT32;
		Object.defineProperty(view, 'version', {
			value: buffer.version,
			configurable: true,
		});
		return view;
	}

	_ensureAttributeMetadata(scope, name) {
		const metaMap = this._attributeMap(scope);
		let meta = metaMap.get(name);
		if (meta) {
			if (meta.attributePtr) {
				return meta;
			}
			const getter = scope === 'node'
				? this.module._CXNetworkGetNodeAttribute
				: scope === 'edge'
					? this.module._CXNetworkGetEdgeAttribute
					: this.module._CXNetworkGetNetworkAttribute;
			this._assertCanAllocate(`attribute metadata lookup for ${scope}:${name}`);
			const cstr = new CString(this.module, name);
			let attributePtr = 0;
			try {
				attributePtr = getter.call(this.module, this.ptr, cstr.ptr) >>> 0;
			} finally {
				cstr.dispose();
			}
			if (!attributePtr) {
				return null;
			}
			meta.attributePtr = attributePtr;
			meta.stride = this.module._CXAttributeStride(attributePtr);
			return meta;
		}

		const getter = scope === 'node'
			? this.module._CXNetworkGetNodeAttribute
			: scope === 'edge'
				? this.module._CXNetworkGetEdgeAttribute
				: this.module._CXNetworkGetNetworkAttribute;

		this._assertCanAllocate(`attribute metadata lookup for ${scope}:${name}`);
		const cstr = new CString(this.module, name);
		let attributePtr = 0;
		try {
			attributePtr = getter.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		if (!attributePtr) {
			return null;
		}

		const base = attributePtr >>> 2;
		const type = this.module.HEAP32[base];
		const dimension = this.module.HEAP32[base + 1] || 1;
		meta = {
			type,
			dimension,
			complex: COMPLEX_ATTRIBUTE_TYPES.has(type),
			jsStore: new Map(),
			stringPointers: new Map(),
			nextHandle: 1,
			attributePtr: attributePtr >>> 0,
			stride: this.module._CXAttributeStride(attributePtr),
		};
		metaMap.set(name, meta);
		return meta;
	}

	/**
	 * Frees any WASM strings owned by attributes in the provided map.
	 * @private
	 *
	 * @param {Map<string, object>} metaMap - Attribute metadata map.
	 */
	_releaseStringAttributes(metaMap) {
		for (const meta of metaMap.values()) {
			if (meta.type === AttributeType.String) {
				for (const ptr of meta.stringPointers.values()) {
					this.module._free(ptr);
				}
				meta.stringPointers.clear();
			}
		}
	}

	_freeStringPointers(meta) {
		if (meta && meta.type === AttributeType.String && meta.stringPointers) {
			for (const ptr of meta.stringPointers.values()) {
				this.module._free(ptr);
			}
			meta.stringPointers.clear();
		}
	}

	/**
	 * Writes string data to WASM memory for a specific attribute slot.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Entity index.
	 * @param {string|null|undefined} value - String to store (or nullish to clear).
	 */
	_setStringAttribute(scope, name, index, value) {
		let meta = this._attributeMap(scope).get(name);
		if (!meta) {
			meta = this._ensureAttributeMetadata(scope, name);
		}
		if (!meta || meta.type !== AttributeType.String) {
			throw new Error(`Attribute "${name}" on ${scope} is not a string attribute`);
		}
		const { pointer } = this._attributePointers(scope, name, meta);
		const capacity = this._capacityForScope(scope);
		const view = new Uint32Array(this.module.HEAPU32.buffer, pointer, capacity * meta.dimension);
		const oldPtr = meta.stringPointers.get(index);
		if (oldPtr) {
			this.module._free(oldPtr);
		}
		if (value == null) {
			view[index] = 0;
			meta.stringPointers.delete(index);
			this._bumpAttributeVersion(scope, name, { op: 'clear', index });
			return;
		}
		const length = this.module.lengthBytesUTF8(value) + 1;
		const ptr = this.module._malloc(length);
		if (!ptr) {
			throw new Error('Failed to allocate memory for string attribute');
		}
		this.module.stringToUTF8(value, ptr, length);
		meta.stringPointers.set(index, ptr);
		view[index] = ptr;
		this._bumpAttributeVersion(scope, name, { op: 'set', index });
	}

	/**
	 * Reads string data from WASM memory for a specific attribute slot.
	 * @private
	 *
	 * @param {'node'|'edge'|'network'} scope - Attribute scope.
	 * @param {string} name - Attribute identifier.
	 * @param {number} index - Entity index.
	 * @returns {string|null} Retrieved string value.
	 */
	_getStringAttribute(scope, name, index) {
		let meta = this._attributeMap(scope).get(name);
		if (!meta) {
			meta = this._ensureAttributeMetadata(scope, name);
		}
		if (!meta || meta.type !== AttributeType.String) {
			throw new Error(`Attribute "${name}" on ${scope} is not a string attribute`);
		}
		const { pointer } = this._attributePointers(scope, name, meta);
		const capacity = this._capacityForScope(scope);
		const view = new Uint32Array(this.module.HEAPU32.buffer, pointer, capacity * meta.dimension);
		const addr = view[index];
		return addr ? this.module.UTF8ToString(addr) : null;
	}

	_getQueryError() {
		const messagePtr = typeof this.module._CXNetworkQueryLastErrorMessage === 'function'
			? this.module._CXNetworkQueryLastErrorMessage()
			: 0;
		const offset = typeof this.module._CXNetworkQueryLastErrorOffset === 'function'
			? this.module._CXNetworkQueryLastErrorOffset()
			: 0;
		const message = messagePtr ? this.module.UTF8ToString(messagePtr) : 'Unknown query error';
		return { message, offset };
	}

	/**
	 * Creates a node selector optionally seeded with indices.
	 *
	 * @param {Iterable<number>|Uint32Array} [indices] - Indices to preload into the selector.
	 * @returns {NodeSelector} Prepared selector.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(3);
	 *
	 * const selector = net.createNodeSelector(); // includes every node
	 * console.log(selector.count); // 3
	 *
	 * const subset = net.createNodeSelector([0, 2]);
	 * console.log([...subset]); // [0, 2]
	 */
	createNodeSelector(indices) {
		const selector = NodeSelector.create(this.module, this);
		if (indices) {
			selector.fillFromArray(this, indices);
		} else {
			selector.fillAll(this);
		}
		return selector._asProxy();
	}

	/**
	 * Creates an edge selector optionally seeded with indices.
	 *
	 * @param {Iterable<number>|Uint32Array} [indices] - Indices to preload into the selector.
	 * @returns {EdgeSelector} Prepared selector.
	 * @example
	 * const net = await HeliosNetwork.create();
	 * net.addNodes(2);
	 * const edges = net.addEdges([[0, 1]]);
	 *
	 * const selector = net.createEdgeSelector(); // iterates over all edges
	 * const sample = net.createEdgeSelector(edges);
	 * console.log(sample.endpoints()); // [[0, 1]]
	 */
	createEdgeSelector(indices) {
		const selector = EdgeSelector.create(this.module, this);
		if (indices) {
			selector.fillFromArray(this, indices);
		} else {
			selector.fillAll(this);
		}
		return selector._asProxy();
	}

	/**
	 * Applies a text batch of stream commands to this network.
	 *
	 * Supported commands (MVP):
	 * - ADD_NODES n=#
	 * - ADD_EDGES pairs=[(a,b),...]
	 * - SET_ATTR_VALUES scope=node|edge name=attr ids=[...] values=[...]
	 *
	 * Relative ids:
	 * - suffix `! relative [varName]` remaps ids against a prior result set.
	 * - result sets are captured as `varName = ADD_NODES ...` or `varName = ADD_EDGES ...`.
	 *
	 * @param {string} text - Batch payload.
	 * @param {{stopOnError?: boolean}=} [options]
	 * @returns {{results:Array, variables:Object}} Execution results and captured variables.
	 */
	applyTextBatch(text, options = {}) {
		this._ensureActive();
		if (typeof text !== 'string') {
			throw new Error('Batch text must be a string');
		}
		const stopOnError = options.stopOnError !== false;
		const results = [];
		const variables = {};
		let lastAddedNodes = null;
		let lastAddedEdges = null;

		const lines = text.split(/\r?\n/);
		for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
			let line = lines[lineIndex].trim();
			if (!line || line.startsWith('#')) {
				continue;
			}

			const result = { line: lineIndex + 1, ok: false, op: null };
			try {
				let relativeName = null;
				let relative = false;
				const relativeIdx = line.indexOf('! relative');
				if (relativeIdx >= 0) {
					const suffix = line.slice(relativeIdx + '! relative'.length).trim();
					relative = true;
					if (suffix) {
						relativeName = suffix.split(/\s+/)[0];
					}
					line = line.slice(0, relativeIdx).trim();
				}

				let varName = null;
				const assignMatch = line.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$/);
				if (assignMatch) {
					varName = assignMatch[1];
					line = assignMatch[2];
				}

				const tokens = splitCommandTokens(line);
				if (!tokens.length) {
					throw new Error('Empty command');
				}
				const op = tokens[0];
				result.op = op;
				const args = parseKeyValueArgs(tokens.slice(1).join(' '));

				if (op === 'ADD_NODES') {
					const count = Number(args.n ?? args.count);
					if (!Number.isFinite(count) || count < 0) {
						throw new Error('ADD_NODES requires n=<count>');
					}
					const ids = this.addNodes(count);
					lastAddedNodes = ids;
					variables.added_node_ids = ids;
					if (varName) {
						variables[varName] = ids;
					}
					result.ok = true;
					result.result = ids;
				} else if (op === 'ADD_EDGES') {
					const pairs = parsePairs(args.pairs);
					if (!pairs.length) {
						throw new Error('ADD_EDGES requires pairs=[(a,b),...]');
					}
					const resolvedPairs = relative
						? resolvePairsRelative(pairs, resolveRelativeSet(relativeName, lastAddedNodes, variables, 'node'))
						: pairs;
					const edges = this.addEdges(resolvedPairs.map(([from, to]) => ({ from, to })));
					lastAddedEdges = edges;
					variables.added_edge_ids = edges;
					if (varName) {
						variables[varName] = edges;
					}
					result.ok = true;
					result.result = edges;
				} else if (op === 'SET_ATTR_VALUES') {
					const scope = args.scope;
					const name = args.name;
					const ids = parseNumberList(args.ids);
					const parsedValues = parseValueList(args.values);
					const values = parsedValues.values;
					if (!scope || !name || !ids.length || !values.length) {
						throw new Error('SET_ATTR_VALUES requires scope, name, ids, and values');
					}
					const resolvedIds = relative
						? resolveIdsRelative(ids, resolveRelativeSet(relativeName, scope === 'edge' ? lastAddedEdges : lastAddedNodes, variables, scope))
						: ids;
					if (values.length !== 1 && values.length !== resolvedIds.length) {
						throw new Error('SET_ATTR_VALUES values length must be 1 or match ids length');
					}
					const attrValues = values.length === 1 ? resolvedIds.map(() => values[0]) : values;
					const metaMap = this._attributeMap(scope);
					let meta = metaMap.get(name);
					if (!meta) {
						meta = this._ensureAttributeMetadata(scope, name);
					}
					if (!meta) {
						throw new Error(`Attribute "${name}" on ${scope} is not defined`);
					}
					if (meta.dimension && meta.dimension > 1) {
						throw new Error('Vector attributes are not supported in text batches');
					}
					if (meta.type === AttributeType.String) {
						if (parsedValues.type !== 'string') {
							throw new Error('String attributes require string values');
						}
						for (let i = 0; i < resolvedIds.length; i += 1) {
							if (scope === 'edge') {
								this.setEdgeStringAttribute(name, resolvedIds[i], attrValues[i]);
							} else {
								this.setNodeStringAttribute(name, resolvedIds[i], attrValues[i]);
							}
						}
					} else if (meta.type === AttributeType.Category && parsedValues.type === 'string') {
						const dict = scope === 'edge'
							? this.getEdgeAttributeCategoryDictionary(name)
							: this.getNodeAttributeCategoryDictionary(name);
						this.withBufferAccess(() => {
							const buffer = scope === 'edge'
								? this.getEdgeAttributeBuffer(name)
								: this.getNodeAttributeBuffer(name);
							for (let i = 0; i < resolvedIds.length; i += 1) {
								const label = attrValues[i];
								const id = dict[label];
								if (id == null) {
									throw new Error(`Category label "${label}" not found`);
								}
								buffer.view[resolvedIds[i]] = id;
							}
						});
					} else {
						if (parsedValues.type !== 'number') {
							throw new Error('Numeric attributes require numeric values');
						}
						this.withBufferAccess(() => {
							const buffer = scope === 'edge'
								? this.getEdgeAttributeBuffer(name)
								: this.getNodeAttributeBuffer(name);
							if (buffer.type === AttributeType.Data || buffer.type === AttributeType.Javascript || buffer.type === AttributeType.MultiCategory) {
								throw new Error('Unsupported attribute type for text batches');
							}
							for (let i = 0; i < resolvedIds.length; i += 1) {
								buffer.view[resolvedIds[i]] = attrValues[i];
							}
						});
					}
					result.ok = true;
				} else {
					throw new Error(`Unsupported command: ${op}`);
				}
			} catch (err) {
				result.error = err instanceof Error ? err.message : String(err);
				results.push(result);
				if (stopOnError) {
					return { results, variables };
				}
				continue;
			}
			results.push(result);
		}
		return { results, variables };
	}

	/**
	 * Applies a binary batch of stream commands to this network.
	 *
	 * Binary format (MVP):
	 * Header:
	 * - u8[4] magic = HNPB
	 * - u8 version = 1
	 * - u8 flags
	 * - u16 reserved
	 * - u32 recordCount
	 *
	 * Record:
	 * - u8 op (1=ADD_NODES, 2=ADD_EDGES, 3=SET_ATTR_VALUES)
	 * - u8 flags (bit0=relative)
	 * - u16 reserved
	 * - u32 resultSlot (0 = unused)
	 * - u32 payloadLen
	 * - payload bytes
	 *
	 * Payloads:
	 * - ADD_NODES: u32 count
	 * - ADD_EDGES: u32 pairCount, u32 baseSlot, u32[pairCount*2] pairs
	 * - SET_ATTR_VALUES: u8 scope(0=node,1=edge), u8 valueType(0=f64), u16 reserved,
	 *   u32 idCount, u32 valueCount, u32 baseSlot, u32 nameLen, u8[nameLen] name,
	 *   u32[idCount] ids, f64[valueCount] values
	 *
	 * @param {ArrayBuffer|Uint8Array} data - Binary batch.
	 * @param {{stopOnError?: boolean}=} [options]
	 * @returns {{results:Array, slots:Map<number, Uint32Array>}} Execution results and result slots.
	 */
	applyBinaryBatch(data, options = {}) {
		this._ensureActive();
		const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
		const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
		let offset = 0;
		const readU8 = () => view.getUint8(offset++);
		const readU16 = () => {
			const v = view.getUint16(offset, true);
			offset += 2;
			return v;
		};
		const readU32 = () => {
			const v = view.getUint32(offset, true);
			offset += 4;
			return v;
		};
		const readF64 = () => {
			const v = view.getFloat64(offset, true);
			offset += 8;
			return v;
		};
		const magic = String.fromCharCode(readU8(), readU8(), readU8(), readU8());
		if (magic !== 'HNPB') {
			throw new Error('Invalid batch magic');
		}
		const version = readU8();
		if (version !== 1) {
			throw new Error(`Unsupported batch version ${version}`);
		}
		readU8(); // flags
		readU16(); // reserved
		const recordCount = readU32();
		const stopOnError = options.stopOnError !== false;
		const slots = new Map();
		const results = [];

		for (let recordIndex = 0; recordIndex < recordCount; recordIndex += 1) {
			const result = { record: recordIndex, ok: false };
			try {
				const op = readU8();
				const flags = readU8();
				readU16();
				const resultSlot = readU32();
				const payloadLen = readU32();
				const payloadStart = offset;
				const relative = (flags & 0x1) !== 0;

				if (op === 1) {
					const count = readU32();
					const ids = this.addNodes(count);
					if (resultSlot) {
						slots.set(resultSlot, ids);
					}
					result.ok = true;
					result.result = ids;
				} else if (op === 2) {
					const pairCount = readU32();
					const baseSlot = readU32();
					const base = relative ? slots.get(baseSlot) : null;
					if (relative && !base) {
						throw new Error('Missing base slot for relative edge pairs');
					}
					const pairs = [];
					for (let i = 0; i < pairCount; i += 1) {
						const a = readU32();
						const b = readU32();
						const from = relative ? base[a] : a;
						const to = relative ? base[b] : b;
						pairs.push({ from, to });
					}
					const edges = this.addEdges(pairs);
					if (resultSlot) {
						slots.set(resultSlot, edges);
					}
					result.ok = true;
					result.result = edges;
				} else if (op === 3) {
					const scopeId = readU8();
					const valueType = readU8();
					readU16();
					const idCount = readU32();
					const valueCount = readU32();
					const baseSlot = readU32();
					const nameLen = readU32();
					const nameBytes = bytes.subarray(offset, offset + nameLen);
					offset += nameLen;
					const name = new TextDecoder().decode(nameBytes);
					const ids = [];
					for (let i = 0; i < idCount; i += 1) {
						ids.push(readU32());
					}
					const values = [];
					if (valueType === 0) {
						for (let i = 0; i < valueCount; i += 1) {
							values.push(readF64());
						}
					} else if (valueType === 1) {
						const decoder = new TextDecoder();
						for (let i = 0; i < valueCount; i += 1) {
							const len = readU32();
							const chunk = bytes.subarray(offset, offset + len);
							offset += len;
							values.push(decoder.decode(chunk));
						}
					} else {
						throw new Error('Unsupported value type');
					}
					const base = relative ? slots.get(baseSlot) : null;
					if (relative && !base) {
						throw new Error('Missing base slot for relative ids');
					}
					const resolvedIds = relative ? ids.map((id) => base[id]) : ids;
					const scope = scopeId === 1 ? 'edge' : 'node';
					const metaMap = this._attributeMap(scope);
					let meta = metaMap.get(name);
					if (!meta) {
						meta = this._ensureAttributeMetadata(scope, name);
					}
					if (!meta) {
						throw new Error(`Attribute \"${name}\" on ${scope} is not defined`);
					}
					if (meta.dimension && meta.dimension > 1) {
						throw new Error('Vector attributes are not supported in binary batches');
					}
					const attrValues = values.length === 1 ? resolvedIds.map(() => values[0]) : values;
					if (meta.type === AttributeType.String) {
						if (valueType !== 1) {
							throw new Error('String attributes require string values');
						}
						for (let i = 0; i < resolvedIds.length; i += 1) {
							if (scope === 'edge') {
								this.setEdgeStringAttribute(name, resolvedIds[i], attrValues[i]);
							} else {
								this.setNodeStringAttribute(name, resolvedIds[i], attrValues[i]);
							}
						}
					} else if (meta.type === AttributeType.Category && valueType === 1) {
						const dict = scope === 'edge'
							? this.getEdgeAttributeCategoryDictionary(name)
							: this.getNodeAttributeCategoryDictionary(name);
						this.withBufferAccess(() => {
							const buffer = scope === 'edge'
								? this.getEdgeAttributeBuffer(name)
								: this.getNodeAttributeBuffer(name);
							for (let i = 0; i < resolvedIds.length; i += 1) {
								const label = attrValues[i];
								const id = dict[label];
								if (id == null) {
									throw new Error(`Category label \"${label}\" not found`);
								}
								buffer.view[resolvedIds[i]] = id;
							}
						});
					} else {
						if (valueType !== 0) {
							throw new Error('Numeric attributes require numeric values');
						}
						this.withBufferAccess(() => {
							const buffer = scope === 'edge'
								? this.getEdgeAttributeBuffer(name)
								: this.getNodeAttributeBuffer(name);
							if (buffer.type === AttributeType.Data || buffer.type === AttributeType.Javascript || buffer.type === AttributeType.MultiCategory) {
								throw new Error('Unsupported attribute type for binary batches');
							}
							for (let i = 0; i < resolvedIds.length; i += 1) {
								buffer.view[resolvedIds[i]] = attrValues[i];
							}
						});
					}
					result.ok = true;
				} else {
					throw new Error(`Unsupported op ${op}`);
				}

				offset = payloadStart + payloadLen;
			} catch (err) {
				result.error = err instanceof Error ? err.message : String(err);
				results.push(result);
				if (stopOnError) {
					return { results, slots };
				}
				continue;
			}
			results.push(result);
		}
		return { results, slots };
	}

	/**
	 * Selects nodes matching a query expression.
	 *
	 * @param {string} whereExpr - Query expression.
	 * @param {{asSelector?: boolean}=} [options] - Selector return control.
	 * @returns {Uint32Array|NodeSelector} Matching node indices or selector proxy.
	 */
	selectNodes(whereExpr, options = {}) {
		this._ensureActive();
		if (typeof whereExpr !== 'string') {
			throw new Error('Query expression must be a string');
		}
		if (typeof this.module._CXNetworkSelectNodesByQuery !== 'function') {
			throw new Error('Query selection is unavailable in this WASM build');
		}
		const { asSelector = false } = options;
		const selector = NodeSelector.create(this.module, this);
		const cstr = new CString(this.module, whereExpr);
		let ok = false;
		try {
			ok = this.module._CXNetworkSelectNodesByQuery(this.ptr, cstr.ptr, selector.ptr);
		} finally {
			cstr.dispose();
		}
		if (!ok) {
			const { message, offset } = this._getQueryError();
			selector.dispose();
			throw new Error(`Query failed at ${offset}: ${message}`);
		}
		if (asSelector) {
			return selector._asProxy();
		}
		const array = selector.toTypedArray();
		selector.dispose();
		return array;
	}

	/**
	 * Selects edges matching a query expression.
	 *
	 * @param {string} whereExpr - Query expression.
	 * @param {{asSelector?: boolean}=} [options] - Selector return control.
	 * @returns {Uint32Array|EdgeSelector} Matching edge indices or selector proxy.
	 */
	selectEdges(whereExpr, options = {}) {
		this._ensureActive();
		if (typeof whereExpr !== 'string') {
			throw new Error('Query expression must be a string');
		}
		if (typeof this.module._CXNetworkSelectEdgesByQuery !== 'function') {
			throw new Error('Query selection is unavailable in this WASM build');
		}
		const { asSelector = false } = options;
		const selector = EdgeSelector.create(this.module, this);
		const cstr = new CString(this.module, whereExpr);
		let ok = false;
		try {
			ok = this.module._CXNetworkSelectEdgesByQuery(this.ptr, cstr.ptr, selector.ptr);
		} finally {
			cstr.dispose();
		}
		if (!ok) {
			const { message, offset } = this._getQueryError();
			selector.dispose();
			throw new Error(`Query failed at ${offset}: ${message}`);
		}
		if (asSelector) {
			return selector._asProxy();
		}
		const array = selector.toTypedArray();
		selector.dispose();
		return array;
	}

	_normalizeFilterOrder(scope, orderBy) {
		if (orderBy == null || orderBy === false) {
			return null;
		}

		if (typeof orderBy === 'string') {
			const trimmed = orderBy.trim();
			if (!trimmed) {
				return null;
			}
			const lower = trimmed.toLowerCase();
			if (lower === 'id' || lower === 'id:asc' || lower === 'asc:id') {
				return { mode: 'id', descending: false, component: 0 };
			}
			if (lower === '-id' || lower === 'id:desc' || lower === 'desc:id') {
				return { mode: 'id', descending: true, component: 0 };
			}
			const descending = trimmed.startsWith('-');
			const attribute = descending ? trimmed.slice(1).trim() : trimmed;
			if (!attribute) {
				return null;
			}
			return { mode: 'attribute', name: attribute, descending, component: 0 };
		}

		if (typeof orderBy !== 'object') {
			throw new Error(`${scope} order descriptor must be a string or object`);
		}

		const directionValue = typeof orderBy.direction === 'string'
			? orderBy.direction.trim().toLowerCase()
			: 'asc';
		const descending = directionValue === 'desc' || directionValue === 'descending';
		const requestedType = typeof orderBy.type === 'string'
			? orderBy.type.trim().toLowerCase()
			: (typeof orderBy.by === 'string' ? orderBy.by.trim().toLowerCase() : '');
		const componentRaw = Number.isFinite(orderBy.component) ? Math.floor(orderBy.component) : 0;
		const component = Math.max(0, componentRaw);

		if (requestedType === 'id') {
			return { mode: 'id', descending, component: 0 };
		}

		const name = (typeof orderBy.attribute === 'string' && orderBy.attribute.trim())
			|| (typeof orderBy.by === 'string' && orderBy.by.trim() && orderBy.by.toLowerCase() !== 'id' ? orderBy.by.trim() : '')
			|| (typeof orderBy.name === 'string' && orderBy.name.trim())
			|| '';
		if (!name) {
			throw new Error(`${scope} order descriptor must provide an attribute name or "id"`);
		}
		return { mode: 'attribute', name, descending, component };
	}

	_applyFilterOrder(scope, indices, order) {
		if (!order || !(indices instanceof Uint32Array) || indices.length <= 1) {
			return indices;
		}

		const sorted = Array.from(indices);
		if (order.mode === 'id') {
			sorted.sort((a, b) => (order.descending ? b - a : a - b));
			return Uint32Array.from(sorted);
		}

		const attributeName = order.name;
		const hasAttribute = scope === 'edge'
			? this.hasEdgeAttribute(attributeName)
			: this.hasNodeAttribute(attributeName);
		if (!hasAttribute) {
			throw new Error(`Unknown ${scope} attribute "${attributeName}" for ordering`);
		}

		const info = scope === 'edge'
			? this.getEdgeAttributeInfo(attributeName)
			: this.getNodeAttributeInfo(attributeName);
		if (!info) {
			throw new Error(`Attribute "${attributeName}" metadata is unavailable`);
		}
		if (
			info.type === AttributeType.String
			|| info.type === AttributeType.Data
			|| info.type === AttributeType.Javascript
			|| info.type === AttributeType.MultiCategory
		) {
			throw new Error(`Attribute "${attributeName}" on ${scope} is not orderable`);
		}

		const dimension = Math.max(1, Math.floor(info.dimension || 1));
		const component = Math.min(Math.max(0, Math.floor(order.component || 0)), dimension - 1);
		const sign = order.descending ? -1 : 1;
		const indexStride = dimension;

		this.withBufferAccess(() => {
			const values = scope === 'edge'
				? this.getEdgeAttributeBuffer(attributeName).view
				: this.getNodeAttributeBuffer(attributeName).view;
			sorted.sort((a, b) => {
				const valueA = Number(values[a * indexStride + component]);
				const valueB = Number(values[b * indexStride + component]);
				const finiteA = Number.isFinite(valueA);
				const finiteB = Number.isFinite(valueB);
				if (finiteA && finiteB) {
					if (valueA < valueB) return -1 * sign;
					if (valueA > valueB) return 1 * sign;
					return a - b;
				}
				if (finiteA) return -1;
				if (finiteB) return 1;
				return a - b;
			});
		});

		return Uint32Array.from(sorted);
	}

	/**
	 * Builds a filtered node/edge view with optional query and selector constraints.
	 *
	 * Node and edge filters are independent, but returned edges are always induced
	 * by the final node set (an edge is kept only when both endpoints are kept).
	 *
	 * @param {FilterSubgraphOptions} [options]
	 * @returns {{nodeIndices:Uint32Array,edgeIndices:Uint32Array}|{nodes:NodeSelector,edges:EdgeSelector}}
	 */
	filterSubgraph(options = {}) {
		this._assertCanAllocate('filterSubgraph');
		this._ensureActive();
		if (typeof this.module._CXNetworkBuildFilteredSubgraph !== 'function') {
			throw new Error('Filtered subgraph helpers are unavailable in this WASM build. Rebuild the artefacts.');
		}
		if (typeof this.module._CXNodeSelectorIntersect !== 'function' || typeof this.module._CXEdgeSelectorIntersect !== 'function') {
			throw new Error('Selector intersection helpers are unavailable in this WASM build. Rebuild the artefacts.');
		}

		const asSelector = options.asSelector === true;
		const nodeQuery = options.nodeQuery;
		const edgeQuery = options.edgeQuery;
		if (nodeQuery != null && typeof nodeQuery !== 'string') {
			throw new Error('nodeQuery must be a string');
		}
		if (edgeQuery != null && typeof edgeQuery !== 'string') {
			throw new Error('edgeQuery must be a string');
		}

		const nodeInput = options.nodeSelection ?? options.nodeSelector ?? null;
		const edgeInput = options.edgeSelection ?? options.edgeSelector ?? null;
		const nodeOrder = this._normalizeFilterOrder('node', options.orderNodesBy);
		const edgeOrder = this._normalizeFilterOrder('edge', options.orderEdgesBy);

		const temporarySelectors = [];
		const ownSelector = (selector) => {
			if (selector) {
				temporarySelectors.push(selector);
			}
			return selector;
		};
		const disposeTemporaries = () => {
			for (let i = temporarySelectors.length - 1; i >= 0; i -= 1) {
				try {
					temporarySelectors[i].dispose?.();
				} catch (_) {
					// ignore cleanup failures
				}
			}
			temporarySelectors.length = 0;
		};

		const isSelectorLike = (value) => value
			&& typeof value === 'object'
			&& typeof value.ptr === 'number'
			&& typeof value.toTypedArray === 'function';

		const cloneSelector = (scope, selectorLike) => {
			const raw = scope === 'edge'
				? EdgeSelector.create(this.module, this)
				: NodeSelector.create(this.module, this);
			raw.fillFromArray(this, selectorLike.toTypedArray());
			return ownSelector(raw._asProxy());
		};

		const materializeInputSelector = (scope, input) => {
			if (input == null) {
				return null;
			}
			if (isSelectorLike(input)) {
				if (input.network === this) {
					return input;
				}
				return cloneSelector(scope, input);
			}
			const proxy = scope === 'edge'
				? this.createEdgeSelector(input)
				: this.createNodeSelector(input);
			return ownSelector(proxy);
		};

		let nodeOutput = null;
		let edgeOutput = null;
		try {
			const queryNodeSelector = (typeof nodeQuery === 'string' && nodeQuery.length > 0)
				? ownSelector(this.selectNodes(nodeQuery, { asSelector: true }))
				: null;
			const queryEdgeSelector = (typeof edgeQuery === 'string' && edgeQuery.length > 0)
				? ownSelector(this.selectEdges(edgeQuery, { asSelector: true }))
				: null;

			const inputNodeSelector = materializeInputSelector('node', nodeInput);
			const inputEdgeSelector = materializeInputSelector('edge', edgeInput);

			let nodeFilter = queryNodeSelector ?? inputNodeSelector ?? null;
			if (queryNodeSelector && inputNodeSelector) {
				const writable = inputNodeSelector.network === this && !temporarySelectors.includes(inputNodeSelector)
					? cloneSelector('node', inputNodeSelector)
					: inputNodeSelector;
				const ok = this.module._CXNodeSelectorIntersect(writable.ptr, queryNodeSelector.ptr, this.ptr);
				if (!ok) {
					throw new Error('Failed to intersect node selectors');
				}
				nodeFilter = writable;
			}

			let edgeFilter = queryEdgeSelector ?? inputEdgeSelector ?? null;
			if (queryEdgeSelector && inputEdgeSelector) {
				const writable = inputEdgeSelector.network === this && !temporarySelectors.includes(inputEdgeSelector)
					? cloneSelector('edge', inputEdgeSelector)
					: inputEdgeSelector;
				const ok = this.module._CXEdgeSelectorIntersect(writable.ptr, queryEdgeSelector.ptr, this.ptr);
				if (!ok) {
					throw new Error('Failed to intersect edge selectors');
				}
				edgeFilter = writable;
			}

			nodeOutput = NodeSelector.create(this.module, this);
			edgeOutput = EdgeSelector.create(this.module, this);
			const built = this.module._CXNetworkBuildFilteredSubgraph(
				this.ptr,
				nodeFilter?.ptr ?? 0,
				edgeFilter?.ptr ?? 0,
				nodeOutput.ptr,
				edgeOutput.ptr,
			);
			if (!built) {
				throw new Error('Failed to build filtered subgraph');
			}

			if (asSelector && !nodeOrder && !edgeOrder) {
				const nodes = nodeOutput._asProxy();
				const edges = edgeOutput._asProxy();
				nodeOutput = null;
				edgeOutput = null;
				return { nodes, edges };
			}

			let nodeIndices = nodeOutput.toTypedArray();
			let edgeIndices = edgeOutput.toTypedArray();
			if (nodeOrder) {
				nodeIndices = this._applyFilterOrder('node', nodeIndices, nodeOrder);
			}
			if (edgeOrder) {
				edgeIndices = this._applyFilterOrder('edge', edgeIndices, edgeOrder);
			}

			if (asSelector) {
				nodeOutput.fillFromArray(this, nodeIndices);
				edgeOutput.fillFromArray(this, edgeIndices);
				const nodes = nodeOutput._asProxy();
				const edges = edgeOutput._asProxy();
				nodeOutput = null;
				edgeOutput = null;
				return { nodes, edges };
			}

			return { nodeIndices, edgeIndices };
		} finally {
			if (nodeOutput) {
				nodeOutput.dispose();
			}
			if (edgeOutput) {
				edgeOutput.dispose();
			}
			disposeTemporaries();
		}
	}

	/**
	 * Copies a node attribute into an edge attribute buffer (sparse) with endpoint control.
	 * Useful when you want to start from a passthrough and then tweak edge values manually.
	 *
	 * @param {string} sourceName - Node attribute identifier.
	 * @param {string} destinationName - Edge attribute identifier.
	 * @param {EndpointSelection} [endpoints='both'] - Which endpoints to copy (0/1/-1).
	 * @param {boolean} [doubleWidth=true] - When copying a single endpoint, duplicate it to fill double width.
	 */
	copyNodeAttributeToEdgeAttribute(sourceName, destinationName, endpoints = 'both', doubleWidth = true) {
		this._copyNodeToEdgeAttribute(sourceName, destinationName, this._normalizeEndpointMode(endpoints), doubleWidth);
	}

	_removeAttribute(scope, name) {
		this._assertCanAllocate(`remove ${scope} attribute`);
		this._ensureActive();
		const metaMap = this._attributeMap(scope);
		const meta = metaMap.get(name);
		if (!meta) {
			throw new Error(`Attribute "${name}" on ${scope} is not defined`);
		}
		const prevVersion = this._getAttributeVersion(scope, name);
		const remover = scope === 'node'
			? this.module._CXNetworkRemoveNodeAttribute
			: scope === 'edge'
				? this.module._CXNetworkRemoveEdgeAttribute
				: this.module._CXNetworkRemoveNetworkAttribute;
		if (typeof remover !== 'function') {
			throw new Error('Attribute removal is unavailable in this WASM build');
		}
		const cstr = new CString(this.module, name);
		let ok = false;
		try {
			ok = remover.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to remove ${scope} attribute "${name}"`);
		}
		this._freeStringPointers(meta);
		metaMap.delete(name);
		this._emitAttributeEvent(HELIOS_NETWORK_EVENTS.attributeRemoved, {
			scope,
			name,
			previousVersion: prevVersion,
		});
		if (scope === 'node') {
			const dependents = this._nodeAttributeDependents.get(name);
			if (dependents) {
				for (const edgeName of dependents) {
					this._nodeToEdgePassthrough.delete(edgeName);
				}
				this._nodeAttributeDependents.delete(name);
			}
		} else if (scope === 'edge') {
			if (this._nodeToEdgePassthrough.has(name)) {
				const entry = this._nodeToEdgePassthrough.get(name);
				this._unregisterNodeToEdgeDependency(entry.sourceName, name);
				this._nodeToEdgePassthrough.delete(name);
			}
			for (const dependents of this._nodeAttributeDependents.values()) {
				dependents.delete(name);
			}
		}
	}

	_isNumericType(type) {
		return type === AttributeType.Float
			|| type === AttributeType.Double
			|| type === AttributeType.Integer
			|| type === AttributeType.UnsignedInteger
			|| type === AttributeType.BigInteger
			|| type === AttributeType.UnsignedBigInteger;
	}

	_normalizeEndpointMode(endpoints) {
		if (endpoints === 0 || endpoints === 'source') return 0;
		if (endpoints === 1 || endpoints === 'destination') return 1;
		if (endpoints === -1 || endpoints === 'both' || endpoints === undefined || endpoints === null) return -1;
		throw new Error('endpoints must be "source", "destination", "both", 0, 1, or -1');
	}

	_denormalizeEndpointMode(mode) {
		if (mode === 0) return 'source';
		if (mode === 1) return 'destination';
		return 'both';
	}

	_copyNodeToEdgeAttribute(sourceName, destinationName, endpointMode, doubleWidth, cachedMeta = null, cachedPointers = null) {
		this._ensureActive();
		if (this.nodeCount === 0 || this.edgeCount === 0) {
			return;
		}
		const sourceMeta = cachedMeta?.sourceMeta ?? this._ensureAttributeMetadata('node', sourceName);
		const targetMeta = cachedMeta?.targetMeta ?? this._ensureAttributeMetadata('edge', destinationName);
		if (!sourceMeta || !targetMeta) {
			throw new Error('Unknown source or destination attribute');
		}
		if (!this._isNumericType(sourceMeta.type) || !this._isNumericType(targetMeta.type)) {
			throw new Error('Node-to-edge copy only supports numeric attributes');
		}
		if (sourceMeta.type !== targetMeta.type) {
			throw new Error('Source node attribute type must match destination edge attribute type');
		}
		const sourceComponents = Math.max(1, sourceMeta.dimension || 1);
		const targetComponents = Math.max(1, targetMeta.dimension || 1);
		const expectedTargetComponents = endpointMode === -1
			? sourceComponents * 2
			: (doubleWidth ? sourceComponents * 2 : sourceComponents);
		if (targetComponents !== expectedTargetComponents) {
			throw new Error(`Edge attribute "${destinationName}" must have dimension ${expectedTargetComponents}`);
		}
		this.withBufferAccess(() => {
			const sourceBuffer = this.getNodeAttributeBuffer(sourceName);
			const targetBuffer = this.getEdgeAttributeBuffer(destinationName);
			if (sourceBuffer.view.constructor !== targetBuffer.view.constructor) {
				throw new Error('Source and destination attribute storage types must match');
			}
			const sourceStrideBytes = cachedMeta?.sourceStrideBytes ?? sourceBuffer.stride;
			const targetStrideBytes = cachedMeta?.targetStrideBytes ?? targetBuffer.stride;
			const sourceStride = cachedMeta?.sourceStride ?? Math.max(1, Math.floor(sourceStrideBytes / sourceBuffer.view.BYTES_PER_ELEMENT));
			const targetStride = cachedMeta?.targetStride ?? Math.max(1, Math.floor(targetStrideBytes / targetBuffer.view.BYTES_PER_ELEMENT));
			const expectedStride = endpointMode === -1
				? sourceStride * 2
				: (doubleWidth ? sourceStride * 2 : sourceStride);
			if (targetStride !== expectedStride) {
				throw new Error(`Edge attribute "${destinationName}" stride does not match expected dimension ${expectedStride}`);
			}
			const duplicateSingleEndpoint = endpointMode !== -1 && doubleWidth;
			const sourcePointers = cachedPointers?.sourcePointers ?? this._attributePointers('node', sourceName, sourceMeta);
			const targetPointers = cachedPointers?.targetPointers ?? this._attributePointers('edge', destinationName, targetMeta);
			if (typeof this.module._CXNetworkCopyNodeAttributesToEdgeAttributes !== 'function') {
				throw new Error('CXNetworkCopyNodeAttributesToEdgeAttributes is unavailable in this WASM build');
			}
			this.module._CXNetworkCopyNodeAttributesToEdgeAttributes(
				this.ptr,
				sourcePointers.pointer,
				sourceStrideBytes,
				targetPointers.pointer,
				targetStrideBytes,
				endpointMode,
				duplicateSingleEndpoint ? 1 : 0
			);
		});
		this._bumpAttributeVersion('edge', destinationName, { op: 'copy' });
	}

	_registerNodeToEdgeDependency(sourceName, edgeName) {
		if (!this._nodeAttributeDependents.has(sourceName)) {
			this._nodeAttributeDependents.set(sourceName, new Set());
		}
		this._nodeAttributeDependents.get(sourceName).add(edgeName);
	}

	_unregisterNodeToEdgeDependency(sourceName, edgeName) {
		const dependents = this._nodeAttributeDependents.get(sourceName);
		if (!dependents) {
			return;
		}
		dependents.delete(edgeName);
		if (dependents.size === 0) {
			this._nodeAttributeDependents.delete(sourceName);
		}
	}

	_markPassthroughEdgesDirtyForNode(sourceName) {
		const dependents = this._nodeAttributeDependents.get(sourceName);
		if (!dependents) return;
		for (const edgeName of dependents) {
			const entry = this._nodeToEdgePassthrough.get(edgeName);
			if (entry) {
				this._copyNodeToEdgeAttribute(entry.sourceName, edgeName, entry.endpointMode, entry.doubleWidth);
			}
		}
	}

	_markAllPassthroughEdgesDirty() {
		for (const edgeName of this._nodeToEdgePassthrough.keys()) {
			const entry = this._nodeToEdgePassthrough.get(edgeName);
			if (entry) {
				this._copyNodeToEdgeAttribute(entry.sourceName, edgeName, entry.endpointMode, entry.doubleWidth);
			}
		}
	}

	/**
	 * Compacts the network so nodes and edges occupy contiguous indices.
	 *
	 * @param {object} [options]
	 * @param {string} [options.nodeOriginalIndexAttribute] - Optional node attribute to store previous indices.
	 * @param {string} [options.edgeOriginalIndexAttribute] - Optional edge attribute to store previous indices.
	 * @returns {HeliosNetwork} The compacted network instance.
	 */
	compact(options = {}) {
		this._ensureActive();
		const prevNodeCount = this.nodeCount;
		const prevEdgeCount = this.edgeCount;
		const prevTopology = this.getTopologyVersions();
		const {
			nodeOriginalIndexAttribute = null,
			edgeOriginalIndexAttribute = null,
		} = options;

		if (typeof this.module._CXNetworkCompact !== 'function') {
			throw new Error('CXNetworkCompact is not available in this WASM build. Rebuild the module to enable compact().');
		}

		const { nodeIndices, edgeIndices } = this.withBufferAccess(() => ({
			nodeIndices: this.nodeIndices.slice(),
			edgeIndices: this.edgeIndices.slice(),
		}), { nodeIndices: true, edgeIndices: true });
		const nodeRemap = this._buildRemapFromIndices(nodeIndices);
		const edgeRemap = this._buildRemapFromIndices(edgeIndices);

		const nodeName = nodeOriginalIndexAttribute ? new CString(this.module, nodeOriginalIndexAttribute) : null;
		const edgeName = edgeOriginalIndexAttribute ? new CString(this.module, edgeOriginalIndexAttribute) : null;

		let success = false;
		try {
			success = this.module._CXNetworkCompact(
				this.ptr,
				nodeName ? nodeName.ptr : 0,
				edgeName ? edgeName.ptr : 0
			);
		} finally {
			if (nodeName) {
				nodeName.dispose();
			}
			if (edgeName) {
				edgeName.dispose();
			}
		}

		if (!success) {
			throw new Error('Failed to compact network');
		}

		if (nodeOriginalIndexAttribute && !this._nodeAttributes.has(nodeOriginalIndexAttribute)) {
			this._nodeAttributes.set(nodeOriginalIndexAttribute, {
				type: AttributeType.UnsignedInteger,
				dimension: 1,
				complex: false,
				jsStore: new Map(),
				stringPointers: new Map(),
				nextHandle: 1,
			});
		}
		if (edgeOriginalIndexAttribute && !this._edgeAttributes.has(edgeOriginalIndexAttribute)) {
			this._edgeAttributes.set(edgeOriginalIndexAttribute, {
				type: AttributeType.UnsignedInteger,
				dimension: 1,
				complex: false,
				jsStore: new Map(),
				stringPointers: new Map(),
				nextHandle: 1,
			});
		}

		this._remapAttributeStores(this._nodeAttributes, nodeRemap);
		this._remapAttributeStores(this._edgeAttributes, edgeRemap);
		this._bumpTopology('node', true);
		const topology = this.getTopologyVersions();
		this._emitTopologyChanged({
			kind: 'network',
			op: 'compacted',
			topology,
			oldTopology: prevTopology,
			oldNodeCount: prevNodeCount,
			nodeCount: this.nodeCount,
			oldEdgeCount: prevEdgeCount,
			edgeCount: this.edgeCount,
		});
		this._invalidateAttributePointerCache();
		this._nodeValidRangeCache = null;
		this._edgeValidRangeCache = null;
		return this;
	}

	/**
	 * Measures degree for selected nodes.
	 *
	 * @param {object} [options]
	 * @param {(number|string)} [options.direction='both'] - out/in/both
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset.
	 * @param {string|null} [options.outNodeAttribute=null] - Optional node attribute name to store degree values.
	 * @returns {{nodeIndices:Uint32Array, values:Float32Array, valuesByNode:Float32Array, direction:number}}
	 */
	measureDegree(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('degree measurement');
		if (typeof this.module._CXNetworkMeasureDegree !== 'function') {
			throw new Error('CXNetworkMeasureDegree is not available in this WASM build. Rebuild the module to enable measureDegree().');
		}

		const direction = this._normalizeNeighborDirection(options.direction ?? 'both');
		const output = this._resolveNodeMetricOutputAttribute(options.outNodeAttribute ?? null, AttributeType.Float, 'Float');
		const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Float32Array.BYTES_PER_ELEMENT);
		if (!outPtr) {
			throw new Error('Failed to allocate WASM buffer for degree measurement');
		}
		try {
			const ok = this.module._CXNetworkMeasureDegree(this.ptr, direction >>> 0, outPtr);
			if (!ok) {
				throw new Error('Degree measurement failed');
			}
			const valuesByNode = this._copyFloat32NodeValuesFromPointer(outPtr);
			if (output) {
				this._bumpAttributeVersion('node', output.name, { op: 'set' });
			}
			const result = this._collectNodeMetricResult(valuesByNode, options.nodes ?? null);
			return { ...result, direction };
		} finally {
			if (!output) {
				this.module._free(outPtr);
			}
		}
	}

	/**
	 * Measures weighted node strength.
	 *
	 * @param {object} [options]
	 * @param {string|null} [options.edgeWeightAttribute=null] - Edge attribute name (dimension 1).
	 * @param {(number|string)} [options.direction='both'] - out/in/both
	 * @param {(number|string)} [options.measure='sum'] - sum/average/maximum/minimum
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset.
	 * @param {string|null} [options.outNodeAttribute=null] - Optional node attribute name to store strength values.
	 * @returns {{nodeIndices:Uint32Array, values:Float32Array, valuesByNode:Float32Array, direction:number, measure:number}}
	 */
	measureStrength(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('strength measurement');
		if (typeof this.module._CXNetworkMeasureStrength !== 'function') {
			throw new Error('CXNetworkMeasureStrength is not available in this WASM build. Rebuild the module to enable measureStrength().');
		}

		const direction = this._normalizeNeighborDirection(options.direction ?? 'both');
		const measure = this._normalizeStrengthMeasure(options.measure ?? 'sum');
		const edgeWeightAttribute = options.edgeWeightAttribute ?? null;
		const output = this._resolveNodeMetricOutputAttribute(options.outNodeAttribute ?? null, AttributeType.Float, 'Float');
		const weightName = edgeWeightAttribute ? new CString(this.module, String(edgeWeightAttribute)) : null;
		const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Float32Array.BYTES_PER_ELEMENT);
		if (!outPtr) {
			if (weightName) weightName.dispose();
			throw new Error('Failed to allocate WASM buffer for strength measurement');
		}
		try {
			const ok = this.module._CXNetworkMeasureStrength(
				this.ptr,
				weightName ? weightName.ptr : 0,
				direction >>> 0,
				measure >>> 0,
				outPtr
			);
			if (!ok) {
				throw new Error('Strength measurement failed');
			}
			const valuesByNode = this._copyFloat32NodeValuesFromPointer(outPtr);
			if (output) {
				this._bumpAttributeVersion('node', output.name, { op: 'set' });
			}
			const result = this._collectNodeMetricResult(valuesByNode, options.nodes ?? null);
			return { ...result, direction, measure };
		} finally {
			if (!output) {
				this.module._free(outPtr);
			}
			if (weightName) {
				weightName.dispose();
			}
		}
	}

	/**
	 * Measures local clustering coefficients.
	 *
	 * @param {object} [options]
	 * @param {(number|string)} [options.variant='unweighted'] - unweighted/onnela/newman
	 * @param {string|null} [options.edgeWeightAttribute=null] - Required for weighted variants.
	 * @param {(number|string)} [options.direction='both'] - out/in/both
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset.
	 * @param {string|null} [options.outNodeAttribute=null] - Optional node attribute name to store local clustering values.
	 * @returns {{nodeIndices:Uint32Array, values:Float32Array, valuesByNode:Float32Array, direction:number, variant:number}}
	 */
		measureLocalClusteringCoefficient(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('local clustering measurement');
		if (typeof this.module._CXNetworkMeasureLocalClusteringCoefficient !== 'function') {
			throw new Error('CXNetworkMeasureLocalClusteringCoefficient is not available in this WASM build. Rebuild the module to enable measureLocalClusteringCoefficient().');
		}

		const direction = this._normalizeNeighborDirection(options.direction ?? 'both');
		const variant = this._normalizeClusteringVariant(options.variant ?? 'unweighted');
		const edgeWeightAttribute = options.edgeWeightAttribute ?? null;
		const output = this._resolveNodeMetricOutputAttribute(options.outNodeAttribute ?? null, AttributeType.Float, 'Float');
		const weightName = edgeWeightAttribute ? new CString(this.module, String(edgeWeightAttribute)) : null;
		const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Float32Array.BYTES_PER_ELEMENT);
		if (!outPtr) {
			if (weightName) weightName.dispose();
			throw new Error('Failed to allocate WASM buffer for clustering measurement');
		}
		try {
			const ok = this.module._CXNetworkMeasureLocalClusteringCoefficient(
				this.ptr,
				weightName ? weightName.ptr : 0,
				direction >>> 0,
				variant >>> 0,
				outPtr
			);
			if (!ok) {
				throw new Error('Local clustering coefficient measurement failed');
			}
			const valuesByNode = this._copyFloat32NodeValuesFromPointer(outPtr);
			if (output) {
				this._bumpAttributeVersion('node', output.name, { op: 'set' });
			}
			const result = this._collectNodeMetricResult(valuesByNode, options.nodes ?? null);
			return { ...result, direction, variant };
		} finally {
			if (!output) {
				this.module._free(outPtr);
			}
			if (weightName) {
				weightName.dispose();
			}
			}
		}

		/**
		 * Measures node coreness (k-core index).
		 *
		 * @param {object} [options]
		 * @param {(number|string)} [options.direction='both'] - out/in/both
		 * @param {(number|string)} [options.executionMode='single-thread'] - auto/single-thread/parallel
		 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset.
		 * @param {string|null} [options.outNodeCorenessAttribute=null] - Optional node attribute name to store coreness.
		 * @returns {{nodeIndices:Uint32Array, values:Uint32Array, valuesByNode:Uint32Array, direction:number, executionMode:number, maxCore:number}}
		 */
		measureCoreness(options = {}) {
			this._ensureActive();
			this._assertCanAllocate('coreness measurement');
			if (typeof this.module._CXNetworkMeasureCoreness !== 'function') {
				throw new Error('CXNetworkMeasureCoreness is not available in this WASM build. Rebuild the module to enable measureCoreness().');
			}

			const direction = this._normalizeNeighborDirection(options.direction ?? 'both');
			const executionMode = this._normalizeMeasurementExecutionMode(
				options.executionMode ?? 'single-thread',
				MeasurementExecutionMode.SingleThread
			);
			const output = this._resolveNodeMetricOutputAttribute(
				options.outNodeCorenessAttribute ?? null,
				AttributeType.UnsignedInteger,
				'UnsignedInteger'
			);

			const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Uint32Array.BYTES_PER_ELEMENT);
			const maxCorePtr = this.module._malloc(Uint32Array.BYTES_PER_ELEMENT);
			if (!outPtr || !maxCorePtr) {
				if (!output && outPtr) this.module._free(outPtr);
				if (maxCorePtr) this.module._free(maxCorePtr);
				throw new Error('Failed to allocate WASM buffers for coreness measurement');
			}
			this.module.HEAPU32[maxCorePtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

			let maxCore = 0;
			let valuesByNode;
			try {
				const ok = this.module._CXNetworkMeasureCoreness(
					this.ptr,
					direction >>> 0,
					executionMode >>> 0,
					outPtr,
					maxCorePtr
				);
				if (!ok) {
					throw new Error('Coreness measurement failed');
				}
				maxCore = this.module.HEAPU32[maxCorePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
				valuesByNode = this._copyUint32NodeValuesFromPointer(outPtr);
			} finally {
				if (!output) {
					this.module._free(outPtr);
				}
				this.module._free(maxCorePtr);
			}

			if (output) {
				this._bumpAttributeVersion('node', output.name, { op: 'set' });
			}

			const result = this._collectNodeMetricResultUint32(valuesByNode, options.nodes ?? null);
			return { ...result, direction, executionMode, maxCore };
		}

		/**
		 * Creates a steppable coreness session for incremental execution.
		 *
		 * Run `session.step({budget})` in a loop until `phase` becomes `3` (done),
		 * then call `session.finalize()` to retrieve coreness values.
		 *
		 * @param {object} [options]
		 * @param {(number|string)} [options.direction='both'] - out/in/both
		 * @param {(number|string)} [options.executionMode='single-thread'] - auto/single-thread/parallel
		 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset for finalize() return payload.
		 * @param {string|null} [options.outNodeCorenessAttribute=null] - Optional node attribute to write coreness in finalize().
		 * @returns {CorenessSession} Session handle.
		 */
		createCorenessSession(options = {}) {
			this._ensureActive();
			this._assertCanAllocate('coreness session creation');
			if (typeof this.module._CXCorenessSessionCreate !== 'function') {
				throw new Error('CXCorenessSessionCreate is not available in this WASM build. Rebuild the module to enable createCorenessSession().');
			}
			const direction = this._normalizeNeighborDirection(options.direction ?? 'both');
			const executionMode = this._normalizeMeasurementExecutionMode(
				options.executionMode ?? 'single-thread',
				MeasurementExecutionMode.SingleThread
			);
			const ptr = this.module._CXCorenessSessionCreate(
				this.ptr,
				direction >>> 0,
				executionMode >>> 0
			);
			if (!ptr) {
				throw new Error('Failed to create coreness session');
			}
			return new CorenessSession(this.module, this, ptr, {
				direction,
				executionMode,
				nodes: options.nodes ?? null,
				outNodeCorenessAttribute: options.outNodeCorenessAttribute ?? null,
			});
		}

		/**
		 * Measures eigenvector centrality.
		 *
	 * @param {object} [options]
	 * @param {string|null} [options.edgeWeightAttribute=null] - Edge weight attribute name.
	 * @param {(number|string)} [options.direction='both'] - out/in/both
	 * @param {(number|string)} [options.executionMode='single-thread'] - auto/single-thread/parallel
	 * @param {number} [options.maxIterations=100]
	 * @param {number} [options.tolerance=1e-6]
	 * @param {Float32Array|Array<number>|null} [options.initialValues=null] - Optional node-capacity-sized initial vector.
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset.
	 * @param {string|null} [options.outNodeAttribute=null] - Optional node attribute name to store eigenvector values.
	 * @returns {{nodeIndices:Uint32Array, values:Float32Array, valuesByNode:Float32Array, direction:number, eigenvalue:number, delta:number, iterations:number, converged:boolean}}
	 */
	measureEigenvectorCentrality(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('eigenvector centrality measurement');
		if (typeof this.module._CXNetworkMeasureEigenvectorCentrality !== 'function') {
			throw new Error('CXNetworkMeasureEigenvectorCentrality is not available in this WASM build. Rebuild the module to enable measureEigenvectorCentrality().');
		}

		const direction = this._normalizeNeighborDirection(options.direction ?? 'both');
		const executionMode = this._normalizeMeasurementExecutionMode(
			options.executionMode ?? 'single-thread',
			MeasurementExecutionMode.SingleThread
		);
		const maxIterations = Math.max(1, (options.maxIterations ?? 100) | 0);
		const tolerance = Number.isFinite(options.tolerance) ? Number(options.tolerance) : 1e-6;
		const edgeWeightAttribute = options.edgeWeightAttribute ?? null;
		const output = this._resolveNodeMetricOutputAttribute(options.outNodeAttribute ?? null, AttributeType.Float, 'Float');
		const weightName = edgeWeightAttribute ? new CString(this.module, String(edgeWeightAttribute)) : null;
		const initialInfo = this._copyFloat32NodeValuesToWasm(options.initialValues ?? null);

		const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Float32Array.BYTES_PER_ELEMENT);
		const eigenvaluePtr = this.module._malloc(Float64Array.BYTES_PER_ELEMENT);
		const deltaPtr = this.module._malloc(Float64Array.BYTES_PER_ELEMENT);
		const iterationsPtr = this.module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		const convergedPtr = this.module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		if (!outPtr || !eigenvaluePtr || !deltaPtr || !iterationsPtr || !convergedPtr) {
			if (!output && outPtr) this.module._free(outPtr);
			if (eigenvaluePtr) this.module._free(eigenvaluePtr);
			if (deltaPtr) this.module._free(deltaPtr);
			if (iterationsPtr) this.module._free(iterationsPtr);
			if (convergedPtr) this.module._free(convergedPtr);
			initialInfo.dispose();
			if (weightName) weightName.dispose();
			throw new Error('Failed to allocate WASM buffers for eigenvector centrality measurement');
		}

		try {
			this.module.HEAPF64[eigenvaluePtr / Float64Array.BYTES_PER_ELEMENT] = 0;
			this.module.HEAPF64[deltaPtr / Float64Array.BYTES_PER_ELEMENT] = 0;
			this.module.HEAPU32[iterationsPtr / Uint32Array.BYTES_PER_ELEMENT] = 0;
			this.module.HEAPU32[convergedPtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

			const ok = this.module._CXNetworkMeasureEigenvectorCentrality(
				this.ptr,
				weightName ? weightName.ptr : 0,
				direction >>> 0,
				executionMode >>> 0,
				maxIterations >>> 0,
				tolerance,
				initialInfo.ptr,
				outPtr,
				eigenvaluePtr,
				deltaPtr,
				iterationsPtr,
				convergedPtr
			);
			if (!ok) {
				throw new Error('Eigenvector centrality measurement failed');
			}

			const valuesByNode = this._copyFloat32NodeValuesFromPointer(outPtr);
			if (output) {
				this._bumpAttributeVersion('node', output.name, { op: 'set' });
			}
			const result = this._collectNodeMetricResult(valuesByNode, options.nodes ?? null);
			const eigenvalue = this.module.HEAPF64[eigenvaluePtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
			const delta = this.module.HEAPF64[deltaPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
			const iterations = this.module.HEAPU32[iterationsPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
			const converged = !!this.module.HEAPU32[convergedPtr / Uint32Array.BYTES_PER_ELEMENT];
			return {
				...result,
				direction,
				eigenvalue,
				delta,
				iterations,
				converged,
			};
		} finally {
			if (!output) {
				this.module._free(outPtr);
			}
			this.module._free(eigenvaluePtr);
			this.module._free(deltaPtr);
			this.module._free(iterationsPtr);
			this.module._free(convergedPtr);
			initialInfo.dispose();
			if (weightName) {
				weightName.dispose();
			}
		}
	}

	/**
	 * Measures betweenness centrality (weighted or unweighted).
	 *
	 * @param {object} [options]
	 * @param {string|null} [options.edgeWeightAttribute=null] - Edge weight attribute name.
	 * @param {(number|string)} [options.executionMode='single-thread'] - auto/single-thread/parallel
	 * @param {Array<number>|TypedArray|null} [options.sourceNodes=null] - Optional source-node subset.
	 * @param {boolean} [options.normalize=true] - Apply canonical normalization.
	 * @param {boolean} [options.accumulate=false] - Add into `initialValues` instead of resetting.
	 * @param {Float32Array|Array<number>|null} [options.initialValues=null] - Optional node-capacity-sized seed values.
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset for returned vector.
	 * @param {string|null} [options.outNodeAttribute=null] - Optional node attribute name to store betweenness values.
	 * @returns {{nodeIndices:Uint32Array, values:Float32Array, valuesByNode:Float32Array, processedSources:number, normalize:boolean, accumulate:boolean}}
	 */
	measureBetweennessCentrality(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('betweenness centrality measurement');
		if (typeof this.module._CXNetworkMeasureBetweennessCentrality !== 'function') {
			throw new Error('CXNetworkMeasureBetweennessCentrality is not available in this WASM build. Rebuild the module to enable measureBetweennessCentrality().');
		}

		const executionMode = this._normalizeMeasurementExecutionMode(
			options.executionMode ?? 'single-thread',
			MeasurementExecutionMode.SingleThread
		);
		const normalize = options.normalize !== false;
		const accumulate = options.accumulate === true;
		const edgeWeightAttribute = options.edgeWeightAttribute ?? null;
		const output = this._resolveNodeMetricOutputAttribute(options.outNodeAttribute ?? null, AttributeType.Float, 'Float');
		const weightName = edgeWeightAttribute ? new CString(this.module, String(edgeWeightAttribute)) : null;
		const sourceInfo = this._copyIndicesToWasm(options.sourceNodes ?? null);
		const initialInfo = this._copyFloat32NodeValuesToWasm(options.initialValues ?? null);

		const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Float32Array.BYTES_PER_ELEMENT);
		if (!outPtr) {
			if (weightName) weightName.dispose();
			sourceInfo.dispose();
			initialInfo.dispose();
			throw new Error('Failed to allocate WASM buffer for betweenness centrality measurement');
		}

		try {
			// Initialize the output buffer before calling into native code.
			// Do not keep this view after the call because native execution may
			// trigger WASM memory growth, which can detach/repoint HEAP views.
			const outView = new Float32Array(this.module.HEAPF32.buffer, outPtr, this.nodeCapacity);
			if (accumulate && initialInfo.ptr) {
				const seedView = new Float32Array(this.module.HEAPF32.buffer, initialInfo.ptr, this.nodeCapacity);
				outView.set(seedView);
			} else {
				outView.fill(0);
			}

			const processedSources = this.module._CXNetworkMeasureBetweennessCentrality(
				this.ptr,
				weightName ? weightName.ptr : 0,
				executionMode >>> 0,
				sourceInfo.ptr,
				sourceInfo.count,
				normalize ? 1 : 0,
				accumulate ? 1 : 0,
				outPtr
			) >>> 0;

			const valuesByNode = this._copyFloat32NodeValuesFromPointer(outPtr);
			if (output) {
				this._bumpAttributeVersion('node', output.name, { op: 'set' });
			}
			const result = this._collectNodeMetricResult(valuesByNode, options.nodes ?? null);
			return {
				...result,
				processedSources,
				normalize,
				accumulate,
			};
		} finally {
			if (!output) {
				this.module._free(outPtr);
			}
			if (weightName) {
				weightName.dispose();
			}
			sourceInfo.dispose();
			initialInfo.dispose();
		}
	}

	/**
	 * Measures connected components.
	 *
	 * Component ids are 1-based for active nodes; inactive nodes remain 0.
	 *
	 * @param {object} [options]
	 * @param {(number|string)} [options.mode='weak'] - weak/strong
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset for returned vector.
	 * @param {string|null} [options.outNodeComponentAttribute=null] - Optional node attribute name to store component ids.
	 * @returns {{nodeIndices:Uint32Array, values:Uint32Array, valuesByNode:Uint32Array, mode:number, componentCount:number, largestComponentSize:number}}
	 */
	measureConnectedComponents(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('connected components measurement');
		if (typeof this.module._CXNetworkMeasureConnectedComponents !== 'function') {
			throw new Error('CXNetworkMeasureConnectedComponents is not available in this WASM build. Rebuild the module to enable measureConnectedComponents().');
		}

		const mode = this._normalizeConnectedComponentsMode(options.mode ?? 'weak');
		const output = this._resolveNodeMetricOutputAttribute(
			options.outNodeComponentAttribute ?? null,
			AttributeType.UnsignedInteger,
			'UnsignedInteger'
		);
		const outPtr = output?.pointer ?? this.module._malloc(this.nodeCapacity * Uint32Array.BYTES_PER_ELEMENT);
		const largestComponentSizePtr = this.module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		if (!outPtr || !largestComponentSizePtr) {
			if (!output && outPtr) this.module._free(outPtr);
			if (largestComponentSizePtr) this.module._free(largestComponentSizePtr);
			throw new Error('Failed to allocate WASM buffers for connected components measurement');
		}
		this.module.HEAPU32[largestComponentSizePtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

		let componentCount = 0;
		let largestComponentSize = 0;
		let valuesByNode;
		try {
			componentCount = this.module._CXNetworkMeasureConnectedComponents(
				this.ptr,
				mode >>> 0,
				outPtr,
				largestComponentSizePtr
			) >>> 0;
			largestComponentSize = this.module.HEAPU32[largestComponentSizePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
			valuesByNode = this._copyUint32NodeValuesFromPointer(outPtr);
		} finally {
			if (!output) {
				this.module._free(outPtr);
			}
			this.module._free(largestComponentSizePtr);
		}

		if (output) {
			this._bumpAttributeVersion('node', output.name, { op: 'set' });
		}

		const result = this._collectNodeMetricResultUint32(valuesByNode, options.nodes ?? null);
		return {
			...result,
			mode,
			componentCount,
			largestComponentSize,
		};
	}

	/**
	 * Creates a steppable connected-components session for incremental execution.
	 *
	 * Run `session.step({budget})` in a loop until `phase` becomes `3` (done),
	 * then call `session.finalize()` to retrieve component ids.
	 *
	 * @param {object} [options]
	 * @param {(number|string)} [options.mode='weak'] - weak/strong
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional node subset for finalize() return payload.
	 * @param {string|null} [options.outNodeComponentAttribute=null] - Optional node attribute to write component ids in finalize().
	 * @returns {ConnectedComponentsSession} Session handle.
	 */
	createConnectedComponentsSession(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('connected components session creation');
		if (typeof this.module._CXConnectedComponentsSessionCreate !== 'function') {
			throw new Error('CXConnectedComponentsSessionCreate is not available in this WASM build. Rebuild the module to enable createConnectedComponentsSession().');
		}
		const mode = this._normalizeConnectedComponentsMode(options.mode ?? 'weak');
		const ptr = this.module._CXConnectedComponentsSessionCreate(this.ptr, mode >>> 0);
		if (!ptr) {
			throw new Error('Failed to create connected-components session');
		}
		return new ConnectedComponentsSession(this.module, this, ptr, {
			mode,
			nodes: options.nodes ?? null,
			outNodeComponentAttribute: options.outNodeComponentAttribute ?? null,
		});
	}

	_extractComponentAsNetwork(nodeIndices, edgeIndices) {
		const network = HeliosNetwork._createWithModule(
			this.module,
			this.directed,
			nodeIndices.length >>> 0,
			edgeIndices.length >>> 0
		);
		if (nodeIndices.length === 0) {
			return network;
		}

		const nodeMap = new Map();
		for (let i = 0; i < nodeIndices.length; i += 1) {
			nodeMap.set(nodeIndices[i], i >>> 0);
		}

		const edges = [];
		this.withBufferAccess(() => {
			const edgesView = this.edgesView;
			for (let i = 0; i < edgeIndices.length; i += 1) {
				const edgeId = edgeIndices[i] >>> 0;
				const base = edgeId * 2;
				const from = edgesView[base];
				const to = edgesView[base + 1];
				const mappedFrom = nodeMap.get(from);
				const mappedTo = nodeMap.get(to);
				if (mappedFrom == null || mappedTo == null) {
					continue;
				}
				edges.push({ from: mappedFrom, to: mappedTo });
			}
		});
		if (edges.length) {
			network.addEdges(edges);
		}
		return network;
	}

	/**
	 * Extracts component partitions and optional per-component network objects.
	 *
	 * @param {object} [options]
	 * @param {(number|string)} [options.mode='weak'] - weak/strong
	 * @param {number} [options.minSize=1] - Minimum number of nodes per returned component.
	 * @param {boolean} [options.asNetworks=false] - Build one induced HeliosNetwork per component.
	 * @param {string|null} [options.outNodeComponentAttribute='component'] - Attribute to store component ids before extraction (set null to skip).
	 * @returns {Array<{componentId:number,size:number,nodeIndices:Uint32Array,edgeIndices:Uint32Array,network?:HeliosNetwork}>}
	 */
	extractConnectedComponents(options = {}) {
		this._ensureActive();
		const mode = this._normalizeConnectedComponentsMode(options.mode ?? 'weak');
		const minSize = Math.max(1, Number.isFinite(options.minSize) ? Math.trunc(options.minSize) : 1);
		const asNetworks = options.asNetworks === true;
		const outNodeComponentAttribute = Object.prototype.hasOwnProperty.call(options, 'outNodeComponentAttribute')
			? options.outNodeComponentAttribute
			: 'component';

		const measured = this.measureConnectedComponents({
			mode,
			outNodeComponentAttribute,
		});
		const valuesByNode = measured.valuesByNode;
		const activeNodes = this.withBufferAccess(() => this.nodeIndices.slice(), { nodeIndices: true });
		const groups = new Map();
		for (let i = 0; i < activeNodes.length; i += 1) {
			const node = activeNodes[i];
			const componentId = valuesByNode[node] >>> 0;
			if (!componentId) {
				continue;
			}
			let bucket = groups.get(componentId);
			if (!bucket) {
				bucket = [];
				groups.set(componentId, bucket);
			}
			bucket.push(node);
		}

		const components = [];
			for (const [componentId, nodes] of groups.entries()) {
				if (nodes.length < minSize) {
					continue;
				}
				const nodeIndices = Uint32Array.from(nodes);
				const filtered = this.filterSubgraph({ nodeSelection: nodeIndices });
				const edgeIndices = filtered.edgeIndices;
				const record = {
					componentId,
					size: nodeIndices.length,
					nodeIndices,
				edgeIndices,
			};
			if (asNetworks) {
				record.network = this._extractComponentAsNetwork(nodeIndices, edgeIndices);
			}
			components.push(record);
		}
		components.sort((a, b) => {
			if (b.size !== a.size) {
				return b.size - a.size;
			}
			return a.componentId - b.componentId;
		});
		return components;
	}

	/**
	 * Extracts only the largest connected component.
	 *
	 * @param {object} [options]
	 * @param {(number|string)} [options.mode='weak'] - weak/strong
	 * @param {boolean} [options.asNetwork=true] - Return an induced network object when true.
	 * @param {string|null} [options.outNodeComponentAttribute='component'] - Attribute to store component ids before extraction.
	 * @returns {{componentId:number,size:number,nodeIndices:Uint32Array,edgeIndices:Uint32Array,network?:HeliosNetwork}|null}
	 */
	extractLargestConnectedComponent(options = {}) {
		const {
			mode = 'weak',
			asNetwork = true,
			outNodeComponentAttribute,
		} = options;
		const components = this.extractConnectedComponents({
			mode,
			asNetworks: asNetwork,
			minSize: 1,
			outNodeComponentAttribute,
		});
		return components.length ? components[0] : null;
	}

	/**
	 * Measures local multiscale capacity and dimension around a single node.
	 *
	 * @param {number} node - Node index.
	 * @param {object} [options]
	 * @param {number} [options.maxLevel=8] - Maximum geodesic radius r.
	 * @param {(number|string)} [options.method='leastsquares'] - Derivative estimator: forward/backward/central/leastsquares.
	 * @param {number} [options.order=2] - Estimator order/window.
	 * @returns {{capacity: Uint32Array, dimension: Float32Array, maxLevel: number, method: number, order: number}}
	 */
	measureNodeDimension(node, options = {}) {
		this._ensureActive();
		this._assertCanAllocate('node dimension measurement');
		if (typeof this.module._CXNetworkMeasureNodeDimension !== 'function') {
			throw new Error('CXNetworkMeasureNodeDimension is not available in this WASM build. Rebuild the module to enable measureNodeDimension().');
		}
		if (!Number.isFinite(node) || node < 0) {
			throw new Error('node must be a non-negative integer');
		}

		const maxLevelInput = options.maxLevel ?? 8;
		const maxLevel = Math.max(0, (maxLevelInput | 0));
		const method = this._normalizeDimensionMethod(options.method ?? 'leastsquares');
		const orderInput = options.order ?? 2;
		const order = this._normalizeDimensionOrder(method, orderInput);
		const levels = maxLevel + 1;

		const capacityPtr = this.module._malloc(levels * Uint32Array.BYTES_PER_ELEMENT);
		const dimensionPtr = this.module._malloc(levels * Float32Array.BYTES_PER_ELEMENT);
		if (!capacityPtr || !dimensionPtr) {
			if (capacityPtr) this.module._free(capacityPtr);
			if (dimensionPtr) this.module._free(dimensionPtr);
			throw new Error('Failed to allocate WASM buffers for node dimension measurement');
		}

		let ok = 0;
		try {
			ok = this.module._CXNetworkMeasureNodeDimension(
				this.ptr,
				node >>> 0,
				maxLevel >>> 0,
				method >>> 0,
				order >>> 0,
				capacityPtr,
				dimensionPtr
			);
			if (!ok) {
				throw new Error(`Failed to measure dimension for node ${node}`);
			}
			const capacity = new Uint32Array(this.module.HEAPU32.buffer, capacityPtr, levels).slice();
			const dimension = new Float32Array(this.module.HEAPF32.buffer, dimensionPtr, levels).slice();
			return { capacity, dimension, maxLevel, method, order };
		} finally {
			this.module._free(capacityPtr);
			this.module._free(dimensionPtr);
		}
	}

	/**
	 * Measures global multiscale dimension statistics across a node set.
	 *
	 * @param {object} [options]
	 * @param {number} [options.maxLevel=8] - Maximum geodesic radius r.
	 * @param {(number|string)} [options.method='leastsquares'] - Derivative estimator: forward/backward/central/leastsquares.
	 * @param {number} [options.order=2] - Estimator order/window.
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional subset of node indices.
	 * @returns {{selectedCount:number, averageCapacity:Float32Array, globalDimension:Float32Array, averageNodeDimension:Float32Array, nodeDimensionStddev:Float32Array, maxLevel:number, method:number, order:number}}
	 */
	measureDimension(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('global dimension measurement');
		if (typeof this.module._CXNetworkMeasureDimension !== 'function') {
			throw new Error('CXNetworkMeasureDimension is not available in this WASM build. Rebuild the module to enable measureDimension().');
		}

		const maxLevelInput = options.maxLevel ?? 8;
		const maxLevel = Math.max(0, (maxLevelInput | 0));
		const method = this._normalizeDimensionMethod(options.method ?? 'leastsquares');
		const orderInput = options.order ?? 2;
		const order = this._normalizeDimensionOrder(method, orderInput);
		const levels = maxLevel + 1;

		const averageCapacityPtr = this.module._malloc(levels * Float32Array.BYTES_PER_ELEMENT);
		const globalDimensionPtr = this.module._malloc(levels * Float32Array.BYTES_PER_ELEMENT);
		const averageNodeDimensionPtr = this.module._malloc(levels * Float32Array.BYTES_PER_ELEMENT);
		const nodeDimensionStddevPtr = this.module._malloc(levels * Float32Array.BYTES_PER_ELEMENT);
		if (!averageCapacityPtr || !globalDimensionPtr || !averageNodeDimensionPtr || !nodeDimensionStddevPtr) {
			if (averageCapacityPtr) this.module._free(averageCapacityPtr);
			if (globalDimensionPtr) this.module._free(globalDimensionPtr);
			if (averageNodeDimensionPtr) this.module._free(averageNodeDimensionPtr);
			if (nodeDimensionStddevPtr) this.module._free(nodeDimensionStddevPtr);
			throw new Error('Failed to allocate WASM buffers for dimension measurement');
		}

		const nodesCopy = this._copyIndicesToWasm(options.nodes ?? null);
		try {
			const selectedCount = this.module._CXNetworkMeasureDimension(
				this.ptr,
				nodesCopy.ptr,
				nodesCopy.count,
				maxLevel >>> 0,
				method >>> 0,
				order >>> 0,
				averageCapacityPtr,
				globalDimensionPtr,
				averageNodeDimensionPtr,
				nodeDimensionStddevPtr
			) >>> 0;
			const averageCapacity = new Float32Array(this.module.HEAPF32.buffer, averageCapacityPtr, levels).slice();
			const globalDimension = new Float32Array(this.module.HEAPF32.buffer, globalDimensionPtr, levels).slice();
			const averageNodeDimension = new Float32Array(this.module.HEAPF32.buffer, averageNodeDimensionPtr, levels).slice();
			const nodeDimensionStddev = new Float32Array(this.module.HEAPF32.buffer, nodeDimensionStddevPtr, levels).slice();
			return {
				selectedCount,
				averageCapacity,
				globalDimension,
				averageNodeDimension,
				nodeDimensionStddev,
				maxLevel,
				method,
				order,
			};
		} finally {
			nodesCopy.dispose();
			this.module._free(averageCapacityPtr);
			this.module._free(globalDimensionPtr);
			this.module._free(averageNodeDimensionPtr);
			this.module._free(nodeDimensionStddevPtr);
		}
	}

	/**
	 * Creates a steppable dimension session for incremental local/global measurements.
	 *
	 * Run `session.step({budget})` until `phase` is `5` (done), then call
	 * `session.finalize()` to retrieve global curves and optionally write node attributes.
	 *
	 * @param {object} [options]
	 * @param {number} [options.maxLevel=8] - Maximum geodesic radius r.
	 * @param {(number|string)} [options.method='leastsquares'] - Derivative estimator.
	 * @param {number} [options.order=2] - Estimator order/window.
	 * @param {Array<number>|TypedArray|null} [options.nodes=null] - Optional subset of nodes.
	 * @param {boolean} [options.captureNodeDimensionProfiles=false] - Store per-level node dimensions for finalize() writes.
	 * @param {string|null} [options.outNodeMaxDimensionAttribute=null] - Default attribute name for max node dimension.
	 * @param {string|null} [options.outNodeDimensionLevelsAttribute=null] - Default attribute name for per-level node dimensions.
	 * @param {string} [options.dimensionLevelsEncoding='vector'] - 'vector' (Float dim=maxLevel+1) or 'string' (JSON array string).
	 * @param {number} [options.dimensionLevelsStringPrecision=6] - Decimal precision for string encoding.
	 * @returns {DimensionSession} Session handle.
	 */
	createDimensionSession(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('dimension session creation');
		return new DimensionSession(this, options);
	}

	/**
	 * Runs Leiden community detection optimizing (weighted) modularity.
	 *
	 * This method allocates inside WASM and may trigger memory growth, so it must
	 * not be invoked during a buffer access session.
	 *
	 * @param {object} [options]
	 * @param {number} [options.resolution=1] - Modularity resolution parameter (gamma).
	 * @param {string|null} [options.edgeWeightAttribute=null] - Edge weight attribute name (dimension 1).
	 * @param {string} [options.outNodeCommunityAttribute='community'] - Node attribute name to store the result.
	 * @param {boolean} [options.categoricalCommunities=true] - Store communities as categorical codes instead of integers.
	 * @param {number} [options.seed=0] - RNG seed (0 uses a default seed).
	 * @param {number} [options.maxLevels=32] - Maximum aggregation levels.
	 * @param {number} [options.maxPasses=8] - Max local-moving passes per phase.
	 * @param {number} [options.passes] - Alias for `maxPasses` (`passes` takes precedence when both are set).
	 * @returns {{communityCount:number, modularity:number}} Result summary.
	 */
	leidenModularity(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('Leiden community detection');
		const {
			resolution = 1,
			edgeWeightAttribute = null,
			outNodeCommunityAttribute = 'community',
			categoricalCommunities = true,
			seed = 0,
			maxLevels = 32,
			maxPasses,
			passes,
		} = options;
		const resolvedPasses = passes ?? maxPasses ?? 8;

		if (typeof this.module._CXNetworkLeidenModularity !== 'function') {
			throw new Error('CXNetworkLeidenModularity is not available in this WASM build. Rebuild the module to enable leidenModularity().');
		}
		if (!Number.isFinite(resolution) || resolution <= 0) {
			throw new Error('resolution must be a positive finite number');
		}
		if (!outNodeCommunityAttribute) {
			throw new Error('outNodeCommunityAttribute is required');
		}

		const weightName = edgeWeightAttribute ? new CString(this.module, edgeWeightAttribute) : null;
		const outName = new CString(this.module, outNodeCommunityAttribute);
		const modularityPtr = this.module._malloc(Float64Array.BYTES_PER_ELEMENT);
		if (!modularityPtr) {
			if (weightName) {
				weightName.dispose();
			}
			outName.dispose();
			throw new Error('Failed to allocate modularity output buffer');
		}
		this.module.HEAPF64[modularityPtr / Float64Array.BYTES_PER_ELEMENT] = 0;

		let communityCount = 0;
		let modularity = 0;
		try {
			communityCount = this.module._CXNetworkLeidenModularity(
				this.ptr,
				weightName ? weightName.ptr : 0,
				resolution,
				seed >>> 0,
				maxLevels >>> 0,
				resolvedPasses >>> 0,
				outName.ptr,
				modularityPtr
			);
			modularity = this.module.HEAPF64[modularityPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
		} finally {
			this.module._free(modularityPtr);
			if (weightName) {
				weightName.dispose();
			}
			outName.dispose();
		}

		if (!communityCount) {
			throw new Error('Leiden optimisation failed');
		}

		if (!this._nodeAttributes.has(outNodeCommunityAttribute)) {
			this._nodeAttributes.set(outNodeCommunityAttribute, {
				type: categoricalCommunities ? AttributeType.Category : AttributeType.UnsignedInteger,
				dimension: 1,
				complex: false,
				jsStore: new Map(),
				stringPointers: new Map(),
				nextHandle: 1,
			});
		} else {
			const meta = this._nodeAttributes.get(outNodeCommunityAttribute);
			if (meta) {
				if (categoricalCommunities && meta.type !== AttributeType.Category) {
					throw new Error(`Node attribute "${outNodeCommunityAttribute}" must be Category`);
				}
				if (!categoricalCommunities && meta.type !== AttributeType.UnsignedInteger) {
					throw new Error(`Node attribute "${outNodeCommunityAttribute}" must be UnsignedInteger`);
				}
			}
		}

		return { communityCount, modularity };
	}

		/**
		 * Creates a steppable Leiden session for incremental execution.
		 *
		 * Run `session.step({budget})` in a loop until `phase` becomes `5` (done),
		 * then call `session.finalize()` to write the output attribute.
		 *
		 * @param {object} [options]
		 * @param {number} [options.resolution=1] - Modularity resolution parameter (gamma).
		 * @param {string|null} [options.edgeWeightAttribute=null] - Edge weight attribute name (dimension 1).
		 * @param {number} [options.seed=0] - RNG seed (0 uses a default seed).
		 * @param {number} [options.maxLevels=32] - Maximum aggregation levels.
		 * @param {number} [options.maxPasses=8] - Max local-moving passes per phase.
		 * @param {number} [options.passes] - Alias for `maxPasses` (`passes` takes precedence when both are set).
		 * @param {string} [options.outNodeCommunityAttribute='community'] - Default output name for finalize().
		 * @param {boolean} [options.categoricalCommunities=true] - Store communities as categorical codes instead of integers.
		 * @returns {LeidenSession} Session handle.
		 */
		createLeidenSession(options = {}) {
			this._ensureActive();
			this._assertCanAllocate('Leiden session creation');
		const {
			resolution = 1,
			edgeWeightAttribute = null,
				seed = 0,
				maxLevels = 32,
				maxPasses,
				passes,
				outNodeCommunityAttribute = 'community',
				categoricalCommunities = true,
			} = options;
		const resolvedPasses = passes ?? maxPasses ?? 8;

		if (typeof this.module._CXLeidenSessionCreate !== 'function') {
			throw new Error('CXLeidenSessionCreate is not available in this WASM build. Rebuild the module to enable createLeidenSession().');
		}
			if (!Number.isFinite(resolution) || resolution <= 0) {
				throw new Error('resolution must be a positive finite number');
			}

			const weightName = edgeWeightAttribute ? new CString(this.module, edgeWeightAttribute) : null;
			let ptr = 0;
			try {
				ptr = this.module._CXLeidenSessionCreate(
				this.ptr,
				weightName ? weightName.ptr : 0,
				resolution,
				seed >>> 0,
				maxLevels >>> 0,
				resolvedPasses >>> 0
			);
		} finally {
			if (weightName) {
				weightName.dispose();
			}
		}

			if (!ptr) {
				throw new Error('Failed to create Leiden session');
			}
			return new LeidenSession(this.module, this, ptr, {
				outNodeCommunityAttribute,
				categoricalCommunities,
				edgeWeightAttribute,
				resolution,
				seed,
				maxLevels,
				maxPasses: resolvedPasses,
				passes: resolvedPasses,
			});
		}

	/**
	 * Internal helper that creates a network backed by the provided module.
	 * @private
	 */
	static _createWithModule(module, directed, initialNodes, initialEdges) {
		const networkPtr = module._CXNewNetworkWithCapacity(directed ? 1 : 0, initialNodes, initialEdges);
		if (!networkPtr) {
			throw new Error('Failed to allocate Helios network');
		}
		const instance = HeliosNetwork._wrapNative(module, networkPtr);
		instance.directed = Boolean(directed);
		if (initialNodes > 0) {
			instance.addNodes(initialNodes);
		}
		return instance;
	}

	static _wrapNative(module, ptr) {
		if (!ptr) {
			throw new Error('Invalid network pointer');
		}
		const directed = module._CXNetworkIsDirected ? module._CXNetworkIsDirected(ptr) !== 0 : false;
		return new HeliosNetwork(module, ptr, directed);
	}

	static async _fromSerialized(source, kind, options = {}) {
		const { module: providedModule } = options;
		const module = providedModule || await getModule();
		moduleInstance = module;

		let extension;
		let readFn;
		let funcLabel;
		let humanLabel;
		switch (kind) {
			case 'bxnet':
				extension = 'bxnet';
				readFn = module._CXNetworkReadBXNet;
				funcLabel = 'ReadBXNet';
				humanLabel = '.bxnet';
				break;
			case 'zxnet':
				extension = 'zxnet';
				readFn = module._CXNetworkReadZXNet;
				funcLabel = 'ReadZXNet';
				humanLabel = '.zxnet';
				break;
			case 'xnet':
				extension = 'xnet';
				readFn = module._CXNetworkReadXNet;
				funcLabel = 'ReadXNet';
				humanLabel = '.xnet';
				break;
			default:
				throw new Error(`Unsupported serialization kind: ${kind}`);
		}
		if (typeof readFn !== 'function') {
			throw new Error(`CXNetwork${funcLabel} is not available in this WASM build. Rebuild the artefacts to enable deserialization helpers.`);
		}
		const fsApi = getModuleFS(module);
		const canUseVirtualFS = Boolean(fsApi);
		let pathForNative = null;
		let fsModule = null;
		let tempHostPath = null;
		let bytes = null;

		if (typeof source === 'string' && !canUseVirtualFS) {
			if (!isNodeRuntime()) {
				throw new Error('Current WASM build lacks virtual FS support; supply bytes or rebuild the artefacts.');
			}
			pathForNative = await resolveNodePath(source);
		} else {
			bytes = await resolveInputBytes(source);
			if (canUseVirtualFS) {
				pathForNative = createVirtualPath(fsApi, extension);
				fsApi.writeFile(pathForNative, bytes);
			} else {
				if (!isNodeRuntime()) {
					throw new Error('Current WASM build lacks virtual FS support; rebuild the artefacts to use serialization in the browser.');
				}
				fsModule = await getNodeFsModule();
				tempHostPath = await createNodeTempPath(extension);
				await fsModule.writeFile(tempHostPath, bytes);
				pathForNative = tempHostPath;
			}
		}

		if (!pathForNative) {
			throw new Error('Unable to resolve input for deserialization');
		}

		let networkPtr = 0;
		const cPath = new CString(module, pathForNative);
		try {
			networkPtr = readFn.call(module, cPath.ptr);
		} finally {
			cPath.dispose();
			if (canUseVirtualFS) {
				try {
					fsApi.unlink(pathForNative);
				} catch (_) {
					/* no-op */
				}
			} else if (tempHostPath && fsModule) {
				await fsModule.rm(tempHostPath, { force: true }).catch(() => {});
			}
		}

		if (!networkPtr) {
			throw new Error(`Failed to read ${humanLabel} data`);
		}
		return HeliosNetwork._wrapNative(module, networkPtr);
	}

	_buildRemapFromIndices(indices) {
		const remap = new Map();
		for (let i = 0; i < indices.length; i += 1) {
			remap.set(indices[i], i);
		}
		return remap;
	}

	_remapAttributeStores(metaMap, remap) {
		for (const meta of metaMap.values()) {
			if (meta.jsStore?.size) {
				const entries = [];
				for (const [oldIndex, payload] of meta.jsStore.entries()) {
					const mapped = remap.get(oldIndex);
					if (mapped !== undefined) {
						entries.push([mapped, payload]);
					}
				}
				meta.jsStore.clear();
				for (const [index, payload] of entries) {
					meta.jsStore.set(index, payload);
				}
			}
			if (meta.stringPointers?.size) {
				const pointerEntries = [];
				for (const [oldIndex, pointer] of meta.stringPointers.entries()) {
					const mapped = remap.get(oldIndex);
					if (mapped !== undefined) {
						pointerEntries.push([mapped, pointer]);
					}
				}
				meta.stringPointers.clear();
				for (const [index, pointer] of pointerEntries) {
					meta.stringPointers.set(index, pointer);
				}
			}
		}
	}

	async _saveSerialized(kind, options) {
		this._ensureActive();
		const module = this.module;
		const extension = kind;
		const format = options?.format ?? 'uint8array';
		const hasAllowFilter = options && Object.prototype.hasOwnProperty.call(options, 'allowAttributes');
		const hasIgnoreFilter = options && Object.prototype.hasOwnProperty.call(options, 'ignoreAttributes');
		const allowFilters = normalizeAttributeFilter(options?.allowAttributes, 'allowAttributes');
		const ignoreFilters = normalizeAttributeFilter(options?.ignoreAttributes, 'ignoreAttributes');
		const useFilters = hasAllowFilter || hasIgnoreFilter;
		let writeFn;
		let filteredWriteFn;
		let funcLabel;
		let filteredLabel;
		let humanLabel;
		switch (kind) {
			case 'bxnet':
				writeFn = module._CXNetworkWriteBXNet;
				filteredWriteFn = module._CXNetworkWriteBXNetFiltered;
				funcLabel = 'WriteBXNet';
				filteredLabel = 'WriteBXNetFiltered';
				humanLabel = '.bxnet';
				break;
			case 'zxnet':
				writeFn = module._CXNetworkWriteZXNet;
				filteredWriteFn = module._CXNetworkWriteZXNetFiltered;
				funcLabel = 'WriteZXNet';
				filteredLabel = 'WriteZXNetFiltered';
				humanLabel = '.zxnet';
				break;
			case 'xnet':
				writeFn = module._CXNetworkWriteXNet;
				filteredWriteFn = module._CXNetworkWriteXNetFiltered;
				funcLabel = 'WriteXNet';
				filteredLabel = 'WriteXNetFiltered';
				humanLabel = '.xnet';
				break;
			default:
				throw new Error(`Unsupported serialization kind: ${kind}`);
		}
		const selectedWriteFn = useFilters ? filteredWriteFn : writeFn;
		const selectedLabel = useFilters ? filteredLabel : funcLabel;
		if (typeof selectedWriteFn !== 'function') {
			throw new Error(`CXNetwork${selectedLabel} is not available in this WASM build. Rebuild the artefacts to use serialization helpers.`);
		}
		const fsApi = getModuleFS(module);
		const canUseVirtualFS = Boolean(fsApi);
		const hasFormatOption = options && Object.prototype.hasOwnProperty.call(options, 'format');
		const needsResult = !options?.path || hasFormatOption;

		let fsModule = null;
		let pathModule = null;
		let resolvedTarget = null;
		let cleanupHostFile = false;
		let pathForNative = null;
		let virtualPath = null;

		if (options?.path) {
			resolvedTarget = await resolveNodePath(options.path);
		}

		if (canUseVirtualFS) {
			virtualPath = createVirtualPath(fsApi, extension);
			pathForNative = virtualPath;
		} else {
			if (!isNodeRuntime()) {
				throw new Error('Current WASM build lacks virtual FS support; rebuild the artefacts to enable serialization in the browser.');
			}
			fsModule = await getNodeFsModule();
			pathModule = await getNodePathModule();
			if (resolvedTarget) {
				const dirnameFn = typeof pathModule.dirname === 'function'
					? pathModule.dirname.bind(pathModule)
					: pathModule.default?.dirname?.bind(pathModule.default);
				if (dirnameFn) {
					const dir = dirnameFn(resolvedTarget);
					if (dir) {
						await fsModule.mkdir(dir, { recursive: true });
					}
				}
				pathForNative = resolvedTarget;
			} else {
				pathForNative = await createNodeTempPath(extension);
				cleanupHostFile = true;
			}
		}

		const cPath = new CString(module, pathForNative);
		let success = false;
		let filterArrays = null;
		const passthroughSnapshot = [];
		for (const [edgeName, entry] of this._nodeToEdgePassthrough.entries()) {
			passthroughSnapshot.push({
				edgeName,
				sourceName: entry.sourceName,
				endpointMode: entry.endpointMode,
				doubleWidth: entry.doubleWidth,
			});
		}
		try {
			if (useFilters) {
				filterArrays = {
					nodeAllow: new CStringArray(module, allowFilters?.node ?? []),
					nodeIgnore: new CStringArray(module, ignoreFilters?.node ?? []),
					edgeAllow: new CStringArray(module, allowFilters?.edge ?? []),
					edgeIgnore: new CStringArray(module, ignoreFilters?.edge ?? []),
					networkAllow: new CStringArray(module, allowFilters?.network ?? []),
					networkIgnore: new CStringArray(module, ignoreFilters?.network ?? []),
				};
			}
			if (passthroughSnapshot.length) {
				for (const entry of passthroughSnapshot) {
					this.removeEdgeAttribute(entry.edgeName);
				}
			}
			if (kind === 'zxnet') {
				const level = clampCompressionLevel(options?.compressionLevel ?? 6);
				if (useFilters) {
					success = selectedWriteFn.call(
						module,
						this.ptr,
						cPath.ptr,
						level,
						filterArrays.nodeAllow.ptr,
						filterArrays.nodeAllow.count,
						filterArrays.nodeIgnore.ptr,
						filterArrays.nodeIgnore.count,
						filterArrays.edgeAllow.ptr,
						filterArrays.edgeAllow.count,
						filterArrays.edgeIgnore.ptr,
						filterArrays.edgeIgnore.count,
						filterArrays.networkAllow.ptr,
						filterArrays.networkAllow.count,
						filterArrays.networkIgnore.ptr,
						filterArrays.networkIgnore.count
					);
				} else {
					success = selectedWriteFn.call(module, this.ptr, cPath.ptr, level);
				}
			} else {
				if (useFilters) {
					success = selectedWriteFn.call(
						module,
						this.ptr,
						cPath.ptr,
						filterArrays.nodeAllow.ptr,
						filterArrays.nodeAllow.count,
						filterArrays.nodeIgnore.ptr,
						filterArrays.nodeIgnore.count,
						filterArrays.edgeAllow.ptr,
						filterArrays.edgeAllow.count,
						filterArrays.edgeIgnore.ptr,
						filterArrays.edgeIgnore.count,
						filterArrays.networkAllow.ptr,
						filterArrays.networkAllow.count,
						filterArrays.networkIgnore.ptr,
						filterArrays.networkIgnore.count
					);
				} else {
					success = selectedWriteFn.call(module, this.ptr, cPath.ptr);
				}
			}
		} finally {
			cPath.dispose();
			if (filterArrays) {
				filterArrays.nodeAllow.dispose();
				filterArrays.nodeIgnore.dispose();
				filterArrays.edgeAllow.dispose();
				filterArrays.edgeIgnore.dispose();
				filterArrays.networkAllow.dispose();
				filterArrays.networkIgnore.dispose();
			}
			if (passthroughSnapshot.length) {
				for (const entry of passthroughSnapshot) {
					this.defineNodeToEdgeAttribute(entry.sourceName, entry.edgeName, entry.endpointMode, entry.doubleWidth);
				}
			}
		}

		if (!success) {
			if (canUseVirtualFS) {
				try {
					fsApi.unlink(virtualPath);
				} catch (_) {
					/* no-op */
				}
			} else if (cleanupHostFile && fsModule) {
				await fsModule.rm(pathForNative, { force: true }).catch(() => {});
			}
			throw new Error(`Failed to write ${humanLabel} data`);
		}

		let bytes = null;
		if (canUseVirtualFS) {
			try {
				bytes = fsApi.readFile(virtualPath);
			} finally {
				try {
					fsApi.unlink(virtualPath);
				} catch (_) {
					/* no-op */
				}
			}
		} else if (needsResult) {
			fsModule = fsModule || await getNodeFsModule();
			const buffer = await fsModule.readFile(pathForNative);
			bytes = buffer instanceof Uint8Array
				? new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength)
				: new Uint8Array(buffer);
		}

		if (!canUseVirtualFS && cleanupHostFile && fsModule) {
			await fsModule.rm(pathForNative, { force: true }).catch(() => {});
		}

		if (canUseVirtualFS && resolvedTarget) {
			fsModule = fsModule || await getNodeFsModule();
			pathModule = pathModule || await getNodePathModule();
			const dirnameFn = typeof pathModule.dirname === 'function'
				? pathModule.dirname.bind(pathModule)
				: pathModule.default?.dirname?.bind(pathModule.default);
			if (dirnameFn) {
				const dir = dirnameFn(resolvedTarget);
				if (dir) {
					await fsModule.mkdir(dir, { recursive: true });
				}
			}
			await fsModule.writeFile(resolvedTarget, bytes);
		}

		if (!needsResult) {
			return undefined;
		}
		if (!bytes) {
			const fs = await getNodeFsModule();
			const buffer = await fs.readFile(resolvedTarget ?? pathForNative);
			bytes = buffer instanceof Uint8Array
				? new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength)
				: new Uint8Array(buffer);
		}
		return bytesToFormat(bytes, format);
	}
}

HeliosNetwork.EVENTS = HELIOS_NETWORK_EVENTS;
HeliosNetwork.DimensionDifferenceMethod = DimensionDifferenceMethod;

function splitCommandTokens(line) {
	const tokens = [];
	let current = '';
	let depth = 0;
	let inQuote = false;
	for (let i = 0; i < line.length; i += 1) {
		const ch = line[i];
		if (ch === '"') {
			inQuote = !inQuote;
			current += ch;
			continue;
		}
		if (!inQuote) {
			if (ch === '[' || ch === '(') {
				depth += 1;
			} else if (ch === ']' || ch === ')') {
				depth = Math.max(0, depth - 1);
			} else if (/\s/.test(ch) && depth === 0) {
				if (current) {
					tokens.push(current);
					current = '';
				}
				continue;
			}
		}
		current += ch;
	}
	if (current) {
		tokens.push(current);
	}
	return tokens;
}

function parseKeyValueArgs(text) {
	const args = {};
	if (!text) {
		return args;
	}
	const tokens = splitCommandTokens(text.trim());
	for (const token of tokens) {
		const idx = token.indexOf('=');
		if (idx <= 0) {
			continue;
		}
		const key = token.slice(0, idx);
		let value = token.slice(idx + 1);
		if (value.startsWith('"') && value.endsWith('"')) {
			value = value.slice(1, -1);
		}
		args[key] = value;
	}
	return args;
}

function parseNumberList(value) {
	if (!value) {
		return [];
	}
	let text = value.trim();
	if (text.startsWith('[') && text.endsWith(']')) {
		text = text.slice(1, -1);
	}
	const matches = text.match(/-?\d+(?:\.\d+)?/g);
	if (!matches) {
		return [];
	}
	return matches.map(Number);
}

function parseValueList(value) {
	if (!value) {
		return { values: [], type: 'none' };
	}
	let text = value.trim();
	if (text.startsWith('[') && text.endsWith(']')) {
		text = text.slice(1, -1);
	}
	const values = [];
	const regex = /"([^"\\]*(?:\\.[^"\\]*)*)"|(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)/g;
	let match = null;
	let type = null;
	while ((match = regex.exec(text))) {
		if (match[1] != null) {
			if (type && type !== 'string') {
				throw new Error('Value list cannot mix strings and numbers');
			}
			type = 'string';
			values.push(match[1].replace(/\\n/g, '\n').replace(/\\t/g, '\t').replace(/\\"/g, '"').replace(/\\\\/g, '\\'));
		} else if (match[2] != null) {
			if (type && type !== 'number') {
				throw new Error('Value list cannot mix strings and numbers');
			}
			type = 'number';
			values.push(Number(match[2]));
		}
	}
	return { values, type: type || 'none' };
}

function parsePairs(value) {
	if (!value) {
		return [];
	}
	const pairs = [];
	let text = value.trim();
	if (text.startsWith('[') && text.endsWith(']')) {
		text = text.slice(1, -1);
	}
	const regex = /\(([^)]+)\)/g;
	let match = null;
	while ((match = regex.exec(text))) {
		const parts = match[1].split(',').map((part) => part.trim()).filter(Boolean);
		if (parts.length !== 2) {
			continue;
		}
		const a = Number(parts[0]);
		const b = Number(parts[1]);
		if (!Number.isFinite(a) || !Number.isFinite(b)) {
			continue;
		}
		pairs.push([a, b]);
	}
	return pairs;
}

function resolveRelativeSet(name, fallback, variables, scope) {
	const key = scope === 'edge' ? 'added_edge_ids' : 'added_node_ids';
	const set = name ? variables[name] : (fallback || variables[key]);
	if (!set || (typeof set.length !== 'number')) {
		throw new Error('Relative ids require a captured result set');
	}
	return set;
}

function resolveIdsRelative(ids, set) {
	return ids.map((idx) => {
		if (idx < 0 || idx >= set.length) {
			throw new Error('Relative id is out of range');
		}
		return set[idx];
	});
}

function resolvePairsRelative(pairs, set) {
	return pairs.map(([a, b]) => {
		if (a < 0 || a >= set.length || b < 0 || b >= set.length) {
			throw new Error('Relative edge endpoint is out of range');
		}
		return [set[a], set[b]];
	});
}

export {
	AttributeType,
	CategorySortOrder,
	DimensionDifferenceMethod,
	NeighborDirection,
	StrengthMeasure,
	ClusteringCoefficientVariant,
	MeasurementExecutionMode,
	ConnectedComponentsMode,
	NodeSelector,
	EdgeSelector,
	getModule as getHeliosModule,
};
export default HeliosNetwork;
