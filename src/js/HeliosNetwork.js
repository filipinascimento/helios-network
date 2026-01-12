import createHeliosModule from './moduleFactory.js';

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
	Unknown: 255,
});

const DenseColorEncodingFormat = Object.freeze({
	Uint8x4: 0,
	Uint32x4: 1,
});

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
	[AttributeType.Category]: Uint32Array,
	[AttributeType.BigInteger]: BigInt64Array,
	[AttributeType.UnsignedBigInteger]: BigUint64Array,
};

const DenseTypedArrayForType = {
	...TypedArrayForType,
	[AttributeType.String]: Uint32Array,
	[AttributeType.Data]: Uint8Array,
	[AttributeType.Javascript]: Uint32Array,
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
	 * @param {{scope?: 'node'|'edge', fullCoverage?: boolean}} [options] - Selector options.
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
			return this.network.nodeIndices;
		}
		if (this._scope === 'edge') {
			return this.network.edgeIndices;
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

		const collectOut = mode === 'out' || mode === 'both' || mode === 'all';
		const collectIn = mode === 'in' || mode === 'both' || mode === 'all';
		const nodeSet = new Set();
		const edgeSet = includeEdges ? new Set() : null;

		for (const node of this) {
			if (collectOut) {
				const { nodes, edges } = this.network.getOutNeighbors(node);
				for (const neighbor of nodes) {
					nodeSet.add(neighbor);
				}
				if (edgeSet) {
					for (const edge of edges) {
						edgeSet.add(edge);
					}
				}
			}
			if (collectIn) {
				const { nodes, edges } = this.network.getInNeighbors(node);
				for (const neighbor of nodes) {
					nodeSet.add(neighbor);
				}
				if (edgeSet) {
					for (const edge of edges) {
						edgeSet.add(edge);
					}
				}
			}
		}

		const nodesArray = setToUint32Array(nodeSet);
		if (!includeEdges) {
			return asSelector ? NodeSelector.fromIndices(this.network, nodesArray) : nodesArray;
		}

		const edgesArray = edgeSet ? setToUint32Array(edgeSet) : new Uint32Array();
		return {
			nodes: asSelector ? NodeSelector.fromIndices(this.network, nodesArray) : nodesArray,
			edges: asSelector ? EdgeSelector.fromIndices(this.network, edgesArray) : edgesArray,
		};
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
		const pairs = [];
		const edgesView = this.network.edgesView;
		for (const edge of this) {
			const base = edge * 2;
			pairs.push([edgesView[base], edgesView[base + 1]]);
		}
		return pairs;
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
		const nodeSet = new Set();
		const edgesView = this.network.edgesView;
		for (const edge of this) {
			const base = edge * 2;
			nodeSet.add(edgesView[base]);
			nodeSet.add(edgesView[base + 1]);
		}
		const indices = setToUint32Array(nodeSet);
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
		const edgesView = this.network.edgesView;
		if (unique) {
			const nodeSet = new Set();
			for (const edge of this) {
				nodeSet.add(edgesView[edge * 2 + offset]);
			}
			const indices = setToUint32Array(nodeSet);
			return asSelector ? NodeSelector.fromIndices(this.network, indices) : indices;
		}

		const indices = new Uint32Array(this.count);
		let position = 0;
		for (const edge of this) {
			indices[position] = edgesView[edge * 2 + offset];
			position += 1;
		}
		return asSelector ? NodeSelector.fromIndices(this.network, indices) : indices;
	}
	}

class LeidenSession {
	constructor(module, network, ptr, options) {
		this.module = module;
		this.network = network;
		this.ptr = ptr;
		this.options = options;
		this._scratch = 0;
	}

	_ensureActive() {
		if (!this.ptr) {
			throw new Error('LeidenSession has been disposed');
		}
		this.network._ensureActive();
	}

	_ensureScratch() {
		if (this._scratch) {
			return this._scratch;
		}
		this.network._assertCanAllocate('Leiden session scratch allocation');
		const ptr = this.module._malloc(80);
		if (!ptr) {
			throw new Error('Failed to allocate Leiden session scratch memory');
		}
		this._scratch = ptr;
		return ptr;
	}

	dispose() {
		if (this.ptr) {
			if (typeof this.module._CXLeidenSessionDestroy === 'function') {
				this.module._CXLeidenSessionDestroy(this.ptr);
			}
			this.ptr = 0;
		}
		if (this._scratch) {
			this.module._free(this._scratch);
			this._scratch = 0;
		}
	}

	getProgress() {
		this._ensureActive();
		if (typeof this.module._CXLeidenSessionGetProgress !== 'function') {
			throw new Error('CXLeidenSessionGetProgress is not available in this WASM build.');
		}

		const base = this._ensureScratch();
		const progressPtr = base + 0; // f64
		const phasePtr = base + 8; // u32
		const levelPtr = base + 12; // u32
		const maxLevelsPtr = base + 16; // u32
		const passPtr = base + 20; // u32
		const maxPassesPtr = base + 24; // u32
		const visitedPtr = base + 28; // u32
		const nodeCountPtr = base + 32; // u32
		const communityCountPtr = base + 36; // u32

		this.module._CXLeidenSessionGetProgress(
			this.ptr,
			progressPtr,
			phasePtr,
			levelPtr,
			maxLevelsPtr,
			passPtr,
			maxPassesPtr,
			visitedPtr,
			nodeCountPtr,
			communityCountPtr
		);

		const progress01 = this.module.HEAPF64[progressPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
		const phase = this.module.HEAPU32[phasePtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const level = this.module.HEAPU32[levelPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const maxLevels = this.module.HEAPU32[maxLevelsPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const pass = this.module.HEAPU32[passPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const maxPasses = this.module.HEAPU32[maxPassesPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const visitedThisPass = this.module.HEAPU32[visitedPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const nodeCount = this.module.HEAPU32[nodeCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		const communityCount = this.module.HEAPU32[communityCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;

		return {
			progress01,
			phase,
			level,
			maxLevels,
			pass,
			maxPasses,
			visitedThisPass,
			nodeCount,
			communityCount,
		};
	}

	step(options = {}) {
		this._ensureActive();
		this.network._assertCanAllocate('Leiden community detection step');
		const {
			budget = 5000,
			timeoutMs = null,
			chunkBudget = 5000,
		} = options;
		if (typeof this.module._CXLeidenSessionStep !== 'function') {
			throw new Error('CXLeidenSessionStep is not available in this WASM build.');
		}

		const now = typeof globalThis !== 'undefined' && globalThis.performance && typeof globalThis.performance.now === 'function'
			? globalThis.performance.now.bind(globalThis.performance)
			: Date.now;

		if (timeoutMs == null) {
			const phase = this.module._CXLeidenSessionStep(this.ptr, budget >>> 0);
			return { phase, ...this.getProgress() };
		}

		const deadline = now() + Math.max(0, Number(timeoutMs) || 0);
		const budgetPerChunk = Math.max(1, chunkBudget >>> 0);
		let phase = 0;
		do {
			phase = this.module._CXLeidenSessionStep(this.ptr, budgetPerChunk);
			if (phase === 5 || phase === 6) {
				break;
			}
		} while (now() < deadline);

		return { phase, ...this.getProgress() };
	}

	finalize(options = {}) {
		this._ensureActive();
		this.network._assertCanAllocate('Leiden community detection finalization');
		if (typeof this.module._CXLeidenSessionFinalize !== 'function') {
			throw new Error('CXLeidenSessionFinalize is not available in this WASM build.');
		}

		const outNodeCommunityAttribute = options.outNodeCommunityAttribute ?? this.options?.outNodeCommunityAttribute ?? 'community';
		if (!outNodeCommunityAttribute) {
			throw new Error('outNodeCommunityAttribute is required');
		}

		const outName = new CString(this.module, outNodeCommunityAttribute);
		const modularityPtr = this.module._malloc(Float64Array.BYTES_PER_ELEMENT);
		const communityCountPtr = this.module._malloc(Uint32Array.BYTES_PER_ELEMENT);
		if (!modularityPtr || !communityCountPtr) {
			outName.dispose();
			if (modularityPtr) this.module._free(modularityPtr);
			if (communityCountPtr) this.module._free(communityCountPtr);
			throw new Error('Failed to allocate Leiden finalize buffers');
		}
		this.module.HEAPF64[modularityPtr / Float64Array.BYTES_PER_ELEMENT] = 0;
		this.module.HEAPU32[communityCountPtr / Uint32Array.BYTES_PER_ELEMENT] = 0;

		let ok = 0;
		let modularity = 0;
		let communityCount = 0;
		try {
			ok = this.module._CXLeidenSessionFinalize(this.ptr, outName.ptr, modularityPtr, communityCountPtr);
			modularity = this.module.HEAPF64[modularityPtr / Float64Array.BYTES_PER_ELEMENT] ?? 0;
			communityCount = this.module.HEAPU32[communityCountPtr / Uint32Array.BYTES_PER_ELEMENT] ?? 0;
		} finally {
			outName.dispose();
			this.module._free(modularityPtr);
			this.module._free(communityCountPtr);
		}
		if (!ok) {
			throw new Error('Leiden session is not ready to finalize (run step() until done)');
		}

		if (!this.network._nodeAttributes.has(outNodeCommunityAttribute)) {
			this.network._nodeAttributes.set(outNodeCommunityAttribute, {
				type: AttributeType.UnsignedInteger,
				dimension: 1,
				complex: false,
				jsStore: new Map(),
				stringPointers: new Map(),
				nextHandle: 1,
			});
		}

		return { communityCount, modularity };
	}
}

/**
 * High-level JavaScript wrapper around the Helios WASM network implementation.
 * Manages lifetime, attribute registration, and buffer views.
 */
export class HeliosNetwork {
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
		this.module = module;
		this.ptr = ptr;
		this.directed = directed;
		this._disposed = false;

		this._nodeAttributes = new Map();
		this._edgeAttributes = new Map();
		this._networkAttributes = new Map();
		this._registeredDenseNodeAttributes = new Set();
		this._registeredDenseEdgeAttributes = new Set();
		this._denseNodeOrderActive = false;
		this._denseEdgeOrderActive = false;
		this._denseNodeAttributeDescriptors = new Map();
		this._denseEdgeAttributeDescriptors = new Map();
		this._denseNodeIndexDescriptor = null;
		this._denseEdgeIndexDescriptor = null;
		this._denseNodeIndexVirtualCache = null;
		this._denseEdgeIndexVirtualCache = null;
		this._denseColorNodeAttributes = new Map();
		this._denseColorEdgeAttributes = new Map();
		this._denseColorNodeDescriptors = new Map();
		this._denseColorEdgeDescriptors = new Map();
		this._bufferSessionDepth = 0;
		this._nodeToEdgePassthrough = new Map();
		this._nodeAttributeDependents = new Map();
		this._nodeTopologyVersion = 0;
		this._edgeTopologyVersion = 0;
		this._nodeIndexCache = null;
		this._edgeIndexCache = null;
		this._dirtyWarningKeys = new Set();
		this._localVersions = new Map();

		this._nodeValidRangeCache = null;
		this._edgeValidRangeCache = null;
		this._validRangeScratchPtr = 0;
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
	 * @param {() => T} fn - Callback to execute.
	 * @returns {T} Callback result.
	 */
	withBufferAccess(fn) {
		this.startBufferAccess();
		try {
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
			this._nodeIndexCache = null;
			this._denseNodeIndexVirtualCache = null;
			this._markAllColorEncodedDirty('node');
			if (cascadeEdges) {
				this._edgeIndexCache = null;
				this._denseEdgeIndexVirtualCache = null;
				this._markAllColorEncodedDirty('edge');
				this._refreshTopologyVersions('edge');
			}
		} else if (scope === 'edge') {
			this._edgeIndexCache = null;
			this._denseEdgeIndexVirtualCache = null;
			this._markAllColorEncodedDirty('edge');
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
	 * Returns a copy of active node indices (native order, independent of dense order).
	 * Not allowed during buffer access because it may allocate.
	 *
	 * @returns {Uint32Array}
	 */
	get nodeIndices() {
		this._assertCanAllocate('nodeIndices');
		return this._materializeActiveIndexCopy('node');
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
	 * Returns a copy of active edge indices (native order, independent of dense order).
	 * Not allowed during buffer access because it may allocate.
	 *
	 * @returns {Uint32Array}
	 */
	get edgeIndices() {
		this._assertCanAllocate('edgeIndices');
		return this._materializeActiveIndexCopy('edge');
	}

	/**
	 * @returns {Uint32Array} Flattened `[from, to]` edge pairs.
	 */
	get edgesView() {
		this._ensureActive();
		const ptr = this.module._CXNetworkEdgesBuffer(this.ptr);
		return new Uint32Array(this.module.HEAPU32.buffer, ptr, this.edgeCapacity * 2);
	}

	/**
	 * Returns a dictionary describing memory usage (in bytes) of the network's buffers.
	 * Sizes are derived from buffer capacities (not current active counts).
	 *
	 * Notes:
	 * - Sparse attribute buffers are computed as `capacity * strideBytes` using the native stride.
	 * - Dense buffers use the last known descriptor capacity. Pass `refreshDense:true` to repack
	 *   all registered dense buffers before reading sizes (may allocate and must not run during buffer access).
	 * - When dense buffers are aliased into sparse storage, `bytes` is reported as 0 (because no extra memory
	 *   is owned by the dense buffer). `viewBytes` still reports the logical span of the dense view.
	 *
	 * @param {{refreshDense?:boolean, includeJs?:boolean}} [options]
	 * @returns {object}
	 */
	getBufferMemoryUsage(options = {}) {
		this._ensureActive();
		const { refreshDense = false, includeJs = true } = options;
		if (refreshDense) {
			this._assertCanAllocate('getBufferMemoryUsage(refreshDense)');
			this.updateDenseNodeIndexBuffer();
			this.updateDenseEdgeIndexBuffer();
			for (const name of this._registeredDenseNodeAttributes) {
				this.updateDenseNodeAttributeBuffer(name);
			}
			for (const name of this._registeredDenseEdgeAttributes) {
				this.updateDenseEdgeAttributeBuffer(name);
			}
			for (const encodedName of this._denseColorNodeAttributes.keys()) {
				this.updateDenseColorEncodedNodeAttribute(encodedName);
			}
			for (const encodedName of this._denseColorEdgeAttributes.keys()) {
				this.updateDenseColorEncodedEdgeAttribute(encodedName);
			}
		}

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
				denseAttributeBytes: 0,
				colorEncodedDenseBytes: 0,
				totalBytes: 0,
			},
			edges: {
				activityBytes: 0,
				freeListBytes: 0,
				fromToBytes: 0,
				sparseAttributeBytes: 0,
				denseAttributeBytes: 0,
				colorEncodedDenseBytes: 0,
				totalBytes: 0,
			},
			network: {
				sparseAttributeBytes: 0,
				totalBytes: 0,
			},
			denseIndex: {
				wasmBytes: 0,
				jsBytes: 0,
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

		const isDenseDescriptorAliased = (scope, name, desc) => {
			if (!desc?.pointer || (scope !== 'node' && scope !== 'edge')) {
				return false;
			}
			let meta;
			try {
				meta = this._ensureAttributeMetadata(scope, name);
			} catch (_) {
				return false;
			}
			if (!meta) {
				return false;
			}
			let pointers;
			try {
				pointers = this._attributePointers(scope, name, meta);
			} catch (_) {
				return false;
			}
			const expected = (pointers.pointer + (Math.max(0, desc.validStart || 0) * pointers.stride)) >>> 0;
			const countMatchesRange = desc.validEnd != null && desc.validStart != null
				? desc.count === Math.max(0, desc.validEnd - desc.validStart)
				: true;
			return (
				(desc.pointer >>> 0) === expected
				&& desc.stride === pointers.stride
				&& countMatchesRange
				&& desc.capacity === desc.count * desc.stride
			);
		};

		const collectDenseAttribute = (scope, name, desc, kind) => {
			const viewBytes = desc?.capacity ?? 0;
			const aliased = kind === 'attribute' ? isDenseDescriptorAliased(scope, name, desc) : false;
			const bytes = aliased ? 0 : (desc?.pointer ? viewBytes : 0);
			return bytes;
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
					denseBytes: 0,
					colorEncodedDenseBytes: 0,
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

		const denseNodeNames = new Set([
			...this._registeredDenseNodeAttributes,
			...this._denseNodeAttributeDescriptors.keys(),
		]);
		for (const name of denseNodeNames) {
			const desc = this._denseNodeAttributeDescriptors.get(name) ?? this._readDenseBufferDescriptor(0);
			const bytes = collectDenseAttribute('node', name, desc, 'attribute');
			addBuffer(`dense.node.attribute.${name}`, bytes);
			totals.nodes.denseAttributeBytes += bytes;
			if (byAttribute.node[name]) {
				byAttribute.node[name].denseBytes = bytes;
				byAttribute.node[name].totalBytes += bytes;
			} else {
				byAttribute.node[name] = {
					sparseBytes: 0,
					denseBytes: bytes,
					colorEncodedDenseBytes: 0,
					totalBytes: bytes,
				};
			}
		}

		const denseEdgeNames = new Set([
			...this._registeredDenseEdgeAttributes,
			...this._denseEdgeAttributeDescriptors.keys(),
		]);
		for (const name of denseEdgeNames) {
			const desc = this._denseEdgeAttributeDescriptors.get(name) ?? this._readDenseBufferDescriptor(0);
			const bytes = collectDenseAttribute('edge', name, desc, 'attribute');
			addBuffer(`dense.edge.attribute.${name}`, bytes);
			totals.edges.denseAttributeBytes += bytes;
			if (byAttribute.edge[name]) {
				byAttribute.edge[name].denseBytes = bytes;
				byAttribute.edge[name].totalBytes += bytes;
			} else {
				byAttribute.edge[name] = {
					sparseBytes: 0,
					denseBytes: bytes,
					colorEncodedDenseBytes: 0,
					totalBytes: bytes,
				};
			}
		}

		const denseNodeIndexDesc = this._denseNodeIndexDescriptor ?? this._readDenseBufferDescriptor(0);
		const denseEdgeIndexDesc = this._denseEdgeIndexDescriptor ?? this._readDenseBufferDescriptor(0);
		const nodeIndexWasmBytes = denseNodeIndexDesc.pointer ? denseNodeIndexDesc.capacity : 0;
		const edgeIndexWasmBytes = denseEdgeIndexDesc.pointer ? denseEdgeIndexDesc.capacity : 0;
		const nodeIndexJsBytes = includeJs && denseNodeIndexDesc?.jsView instanceof Uint32Array ? denseNodeIndexDesc.jsView.byteLength : 0;
		const edgeIndexJsBytes = includeJs && denseEdgeIndexDesc?.jsView instanceof Uint32Array ? denseEdgeIndexDesc.jsView.byteLength : 0;
		totals.denseIndex.wasmBytes += nodeIndexWasmBytes + edgeIndexWasmBytes;
		totals.denseIndex.jsBytes += nodeIndexJsBytes + edgeIndexJsBytes;
		addBuffer('dense.node.index.wasm', nodeIndexWasmBytes);
		addBuffer('dense.node.index.js', nodeIndexJsBytes);
		addBuffer('dense.edge.index.wasm', edgeIndexWasmBytes);
		addBuffer('dense.edge.index.js', edgeIndexJsBytes);

		const denseColorNodeNames = new Set([
			...this._denseColorNodeAttributes.keys(),
			...this._denseColorNodeDescriptors.keys(),
		]);
		for (const encodedName of denseColorNodeNames) {
			const desc = this._denseColorNodeDescriptors.get(encodedName) ?? this._readDenseBufferDescriptor(0);
			const bytes = collectDenseAttribute('node', encodedName, desc, 'colorEncoded');
			addBuffer(`denseColor.node.${encodedName}`, bytes);
			totals.nodes.colorEncodedDenseBytes += bytes;
		}

		const denseColorEdgeNames = new Set([
			...this._denseColorEdgeAttributes.keys(),
			...this._denseColorEdgeDescriptors.keys(),
		]);
		for (const encodedName of denseColorEdgeNames) {
			const desc = this._denseColorEdgeDescriptors.get(encodedName) ?? this._readDenseBufferDescriptor(0);
			const bytes = collectDenseAttribute('edge', encodedName, desc, 'colorEncoded');
			addBuffer(`denseColor.edge.${encodedName}`, bytes);
			totals.edges.colorEncodedDenseBytes += bytes;
		}

		for (const [encodedName, meta] of this._denseColorNodeAttributes.entries()) {
			const sourceName = meta?.sourceName;
			if (typeof sourceName !== 'string' || sourceName === '$index') {
				continue;
			}
			if (!byAttribute.node[sourceName]) {
				continue;
			}
			const desc = this._denseColorNodeDescriptors.get(encodedName);
			if (!desc) {
				continue;
			}
			const bytes = collectDenseAttribute('node', encodedName, desc, 'colorEncoded');
			byAttribute.node[sourceName].colorEncodedDenseBytes += bytes;
			byAttribute.node[sourceName].totalBytes += bytes;
		}

		for (const [encodedName, meta] of this._denseColorEdgeAttributes.entries()) {
			const sourceName = meta?.sourceName;
			if (typeof sourceName !== 'string' || sourceName === '$index') {
				continue;
			}
			if (!byAttribute.edge[sourceName]) {
				continue;
			}
			const desc = this._denseColorEdgeDescriptors.get(encodedName);
			if (!desc) {
				continue;
			}
			const bytes = collectDenseAttribute('edge', encodedName, desc, 'colorEncoded');
			byAttribute.edge[sourceName].colorEncodedDenseBytes += bytes;
			byAttribute.edge[sourceName].totalBytes += bytes;
		}

		totals.denseIndex.totalBytes = totals.denseIndex.wasmBytes + totals.denseIndex.jsBytes;
		totals.nodes.totalBytes = totals.nodes.activityBytes
			+ totals.nodes.freeListBytes
			+ totals.nodes.sparseAttributeBytes
			+ totals.nodes.denseAttributeBytes
			+ totals.nodes.colorEncodedDenseBytes;
		totals.edges.totalBytes = totals.edges.activityBytes
			+ totals.edges.freeListBytes
			+ totals.edges.fromToBytes
			+ totals.edges.sparseAttributeBytes
			+ totals.edges.denseAttributeBytes
			+ totals.edges.colorEncodedDenseBytes;
		totals.network.totalBytes = totals.network.sparseAttributeBytes;

		const attributes = {
			node: {
				sparseBytes: totals.nodes.sparseAttributeBytes,
				denseBytes: totals.nodes.denseAttributeBytes,
				colorEncodedDenseBytes: totals.nodes.colorEncodedDenseBytes,
				totalBytes: totals.nodes.sparseAttributeBytes + totals.nodes.denseAttributeBytes + totals.nodes.colorEncodedDenseBytes,
			},
			edge: {
				fromToBytes: totals.edges.fromToBytes,
				sparseBytes: totals.edges.sparseAttributeBytes,
				denseBytes: totals.edges.denseAttributeBytes,
				colorEncodedDenseBytes: totals.edges.colorEncodedDenseBytes,
				totalBytes: totals.edges.fromToBytes + totals.edges.sparseAttributeBytes + totals.edges.denseAttributeBytes + totals.edges.colorEncodedDenseBytes,
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
	 * @param {{refreshDense?:boolean}} [options]
	 * @returns {object}
	 */
	getBufferVersions(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('getBufferVersions');
		const { refreshDense = false } = options;

		if (refreshDense) {
			this._assertCanAllocate('getBufferVersions(refreshDense)');
			this.updateDenseNodeIndexBuffer();
			this.updateDenseEdgeIndexBuffer();
			for (const name of this._registeredDenseNodeAttributes) {
				this.updateDenseNodeAttributeBuffer(name);
			}
			for (const name of this._registeredDenseEdgeAttributes) {
				this.updateDenseEdgeAttributeBuffer(name);
			}
			for (const encodedName of this._denseColorNodeAttributes.keys()) {
				this.updateDenseColorEncodedNodeAttribute(encodedName);
			}
			for (const encodedName of this._denseColorEdgeAttributes.keys()) {
				this.updateDenseColorEncodedEdgeAttribute(encodedName);
			}
		}

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

		const dense = {
			node: {
				index: (this._denseNodeIndexDescriptor?.version ?? 0) >>> 0,
				attributes: Object.create(null),
			},
			edge: {
				index: (this._denseEdgeIndexDescriptor?.version ?? 0) >>> 0,
				attributes: Object.create(null),
			},
		};
		for (const name of new Set([...this._registeredDenseNodeAttributes, ...this._denseNodeAttributeDescriptors.keys()])) {
			dense.node.attributes[name] = (this._denseNodeAttributeDescriptors.get(name)?.version ?? 0) >>> 0;
		}
		for (const name of new Set([...this._registeredDenseEdgeAttributes, ...this._denseEdgeAttributeDescriptors.keys()])) {
			dense.edge.attributes[name] = (this._denseEdgeAttributeDescriptors.get(name)?.version ?? 0) >>> 0;
		}

		const colorEncodedDense = {
			node: Object.create(null),
			edge: Object.create(null),
		};
		for (const name of new Set([...this._denseColorNodeAttributes.keys(), ...this._denseColorNodeDescriptors.keys()])) {
			colorEncodedDense.node[name] = (this._denseColorNodeDescriptors.get(name)?.version ?? 0) >>> 0;
		}
		for (const name of new Set([...this._denseColorEdgeAttributes.keys(), ...this._denseColorEdgeDescriptors.keys()])) {
			colorEncodedDense.edge[name] = (this._denseColorEdgeDescriptors.get(name)?.version ?? 0) >>> 0;
		}

		return {
			topology,
			attributes,
			dense,
			colorEncodedDense,
		};
	}

	/**
	 * Registers a dense node attribute buffer that can be refreshed on demand.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} [initialCapacityBytes=0] - Optional initial dense buffer capacity in bytes.
	 */
	addDenseNodeAttributeBuffer(name, initialCapacityBytes = 0) {
		this._assertCanAllocate('addDenseNodeAttributeBuffer');
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkAddDenseNodeAttribute(this.ptr, cstr.ptr, initialCapacityBytes);
		} finally {
			cstr.dispose();
		}
		this._registeredDenseNodeAttributes.add(name);
	}

	/**
	/**
	 * Registers a dense edge attribute buffer.
	 */
	addDenseEdgeAttributeBuffer(name, initialCapacityBytes = 0) {
		this._assertCanAllocate('addDenseEdgeAttributeBuffer');
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkAddDenseEdgeAttribute(this.ptr, cstr.ptr, initialCapacityBytes);
		} finally {
			cstr.dispose();
		}
		this._registeredDenseEdgeAttributes.add(name);
	}

	/**
	 * Registers an edge attribute that is derived from a node attribute.
	 * The edge sparse buffer remains empty until packed; dense packing is done via
	 * {@link updateDenseEdgeAttributeBuffer}, which will pull from the source node attribute.
	 *
	 * @param {string} sourceName - Node attribute identifier.
	 * @param {string} edgeName - Edge attribute identifier that will expose the derived values.
	 * @param {'source'|'destination'|'both'|0|1|-1} [endpoints='both'] - Which endpoint to propagate.
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
		this.addDenseEdgeAttributeBuffer(edgeName);
		this._nodeToEdgePassthrough.set(edgeName, {
			sourceName,
			endpointMode,
			doubleWidth,
			dirty: true,
		});
		this._registerNodeToEdgeDependency(sourceName, edgeName);
	}

	/**
	 * Returns a snapshot of node-to-edge passthrough registrations.
	 * Each entry describes the node source, the derived edge attribute, and the endpoint policy.
	 * @returns {Array<{edgeName:string,sourceName:string,endpoints:'source'|'destination'|'both',doubleWidth:boolean}>}
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
		this._markDenseAttributeDirtySilently('edge', edgeName);
	}

	/**
	 * Removes a dense node attribute buffer.
	 */
	removeDenseNodeAttributeBuffer(name) {
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkRemoveDenseNodeAttribute(this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		this._registeredDenseNodeAttributes.delete(name);
		this._denseNodeAttributeDescriptors.delete(name);
	}

	/**
	 * Removes a dense edge attribute buffer.
	 */
	removeDenseEdgeAttributeBuffer(name) {
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkRemoveDenseEdgeAttribute(this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		this._registeredDenseEdgeAttributes.delete(name);
		this._denseEdgeAttributeDescriptors.delete(name);
	}

	/**
	 * Removes a node attribute and its storage (sparse + dense buffers).
	 */
	removeNodeAttribute(name) {
		this._removeAttribute('node', name);
	}

	/**
	 * Removes an edge attribute and its storage (sparse + dense buffers).
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
	 * Marks a dense node attribute buffer dirty, forcing a repack on next update.
	 */
	markDenseNodeAttributeDirty(name) {
		this._warnDirtyDeprecated('markDenseNodeAttributeDirty');
		this._ensureActive();
		this._markDenseAttributeDirtySilently('node', name);
		this._markPassthroughEdgesDirtyForNode(name);
		this._markColorEncodedDirtyForSource('node', name);
	}

	/**
	 * Marks a dense edge attribute buffer dirty.
	 */
	markDenseEdgeAttributeDirty(name) {
		this._warnDirtyDeprecated('markDenseEdgeAttributeDirty');
		this._ensureActive();
		this._markDenseAttributeDirtySilently('edge', name);
		this._markColorEncodedDirtyForSource('edge', name);
	}

	/**
	 * Refreshes a dense node attribute buffer and returns its view metadata.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {{view:Uint8Array,count:number,capacity:number,stride:number,validStart:number,validEnd:number,pointer:number}}
	 */
	updateDenseNodeAttributeBuffer(name) {
		if (!this.hasNodeAttribute(name)) {
			throw new Error(`Unknown node attribute "${name}"`);
		}
		this._assertCanAllocate('updateDenseNodeAttributeBuffer');
		this._ensureActive();
		if (this._registeredDenseNodeAttributes.has(name) && this._canAliasDenseAttributeBuffer('node')) {
			this._refreshAliasedDenseAttributeDescriptor('node', name);
			return;
		}
		const cstr = new CString(this.module, name);
		let ptr = 0;
		try {
			// console.log('TRYING: updateDenseNodeAttributeBuffer', name, ptr);
			ptr = this.module._CXNetworkUpdateDenseNodeAttribute(this.ptr, cstr.ptr);
			// console.log('SUCCESS: updateDenseNodeAttributeBuffer', name, ptr);
		} finally {
			cstr.dispose();
		}
		const desc = this._readDenseBufferDescriptor(ptr);
		const version = (desc.sourceVersion ?? desc.version) ?? this._nextLocalVersion(`dense:node:${name}`);
		this._denseNodeAttributeDescriptors.set(name, { ...desc, version });
	}

	/**
	 * Refreshes a dense edge attribute buffer.
	 */
	updateDenseEdgeAttributeBuffer(name) {
		if (!this.hasEdgeAttribute(name)) {
			throw new Error(`Unknown edge attribute "${name}"`);
		}
		this._assertCanAllocate('updateDenseEdgeAttributeBuffer');
		this._ensureActive();
		const passthrough = this._nodeToEdgePassthrough.get(name);
		if (passthrough) {
			if (passthrough.dirty) {
				this._copyNodeToEdgeAttribute(passthrough.sourceName, name, passthrough.endpointMode, passthrough.doubleWidth);
				this._markDenseAttributeDirtySilently('edge', name);
				passthrough.dirty = false;
			}
		}
		if (this._registeredDenseEdgeAttributes.has(name) && this._canAliasDenseAttributeBuffer('edge')) {
			this._refreshAliasedDenseAttributeDescriptor('edge', name);
			return;
		}
		const cstr = new CString(this.module, name);
		let ptr = 0;
		try {
			ptr = this.module._CXNetworkUpdateDenseEdgeAttribute(this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		const desc = this._readDenseBufferDescriptor(ptr);
		const version = (desc.sourceVersion ?? desc.version) ?? this._nextLocalVersion(`dense:edge:${name}`);
		this._denseEdgeAttributeDescriptors.set(name, { ...desc, version });
	}

	/**
	 * Returns a dense buffer of active node indices using the stored dense node order.
	 */
	updateDenseNodeIndexBuffer() {
		this._assertCanAllocate('updateDenseNodeIndexBuffer');
		this._ensureActive();
		this._refreshTopologyVersions('node');
		if (!this._denseNodeOrderActive) {
			const { start, end } = this.nodeValidRange;
			const count = Math.max(0, end - start);
			if (count && this.nodeCount === count) {
				const version = this._nodeTopologyVersion;
				const cached = this._denseNodeIndexVirtualCache;
				if (!cached || cached.version !== version || cached.start !== start || cached.end !== end) {
					const view = new Uint32Array(count);
					for (let i = 0; i < count; i += 1) {
						view[i] = (start + i) >>> 0;
					}
					this._denseNodeIndexVirtualCache = { version, start, end, view };
				}
				this._denseNodeIndexDescriptor = {
					pointer: 0,
					count,
					capacity: count * Uint32Array.BYTES_PER_ELEMENT,
					stride: Uint32Array.BYTES_PER_ELEMENT,
					validStart: start,
					validEnd: end,
					dirty: false,
					version,
					topologyVersion: version,
					jsView: this._denseNodeIndexVirtualCache.view,
				};
				return;
			}
		}
		let ptr = 0;
		try {
			ptr = this.module._CXNetworkUpdateDenseNodeIndexBuffer(this.ptr);
		} finally {
		}
		this._refreshTopologyVersions('node');
		const desc = this._readDenseBufferDescriptor(ptr);
		const topologyVersion = this._nodeTopologyVersion;
		let version = (desc.sourceVersion ?? topologyVersion ?? desc.version);
		if (version === null || version === undefined) {
			version = this._nextLocalVersion('dense:index:node');
		}
		this._denseNodeIndexDescriptor = { ...desc, version, topologyVersion };
	}

	/**
	 * Returns a dense buffer of active edge indices using the stored dense edge order.
	 */
	updateDenseEdgeIndexBuffer() {
		this._assertCanAllocate('updateDenseEdgeIndexBuffer');
		this._ensureActive();
		this._refreshTopologyVersions('edge');
		if (!this._denseEdgeOrderActive) {
			const { start, end } = this.edgeValidRange;
			const count = Math.max(0, end - start);
			if (count && this.edgeCount === count) {
				const version = this._edgeTopologyVersion;
				const cached = this._denseEdgeIndexVirtualCache;
				if (!cached || cached.version !== version || cached.start !== start || cached.end !== end) {
					const view = new Uint32Array(count);
					for (let i = 0; i < count; i += 1) {
						view[i] = (start + i) >>> 0;
					}
					this._denseEdgeIndexVirtualCache = { version, start, end, view };
				}
				this._denseEdgeIndexDescriptor = {
					pointer: 0,
					count,
					capacity: count * Uint32Array.BYTES_PER_ELEMENT,
					stride: Uint32Array.BYTES_PER_ELEMENT,
					validStart: start,
					validEnd: end,
					dirty: false,
					version,
					topologyVersion: version,
					jsView: this._denseEdgeIndexVirtualCache.view,
				};
				return;
			}
		}
		let ptr = 0;
		try {
			ptr = this.module._CXNetworkUpdateDenseEdgeIndexBuffer(this.ptr);
		} finally {
		}
		this._refreshTopologyVersions('edge');
		const desc = this._readDenseBufferDescriptor(ptr);
		const topologyVersion = this._edgeTopologyVersion;
		let version = (desc.sourceVersion ?? topologyVersion ?? desc.version);
		if (version === null || version === undefined) {
			version = this._nextLocalVersion('dense:index:edge');
		}
		this._denseEdgeIndexDescriptor = { ...desc, version, topologyVersion };
	}

	/**
	 * Returns a typed view over the last packed dense node attribute buffer.
	 */
	getDenseNodeAttributeView(name) {
		this._ensureActive();
		const meta = this._ensureAttributeMetadata('node', name);
		const desc = this._denseNodeAttributeDescriptors.get(name) ?? this._readDenseBufferDescriptor(0);
		return this._materializeDenseAttributeView(desc, meta);
	}

	/**
	 * Returns a typed view over the last packed dense edge attribute buffer.
	 */
	getDenseEdgeAttributeView(name) {
		this._ensureActive();
		const meta = this._ensureAttributeMetadata('edge', name);
		const desc = this._denseEdgeAttributeDescriptors.get(name) ?? this._readDenseBufferDescriptor(0);
		return this._materializeDenseAttributeView(desc, meta);
	}

	/**
	 * Returns a typed view over the last packed dense node index buffer.
	 */
	getDenseNodeIndexView() {
		this._ensureActive();
		const desc = this._denseNodeIndexDescriptor ?? this._readDenseBufferDescriptor(0);
		return this._materializeDenseIndexView(desc);
	}

	/**
	 * Returns a typed view over the last packed dense edge index buffer.
	 */
	getDenseEdgeIndexView() {
		this._ensureActive();
		const desc = this._denseEdgeIndexDescriptor ?? this._readDenseBufferDescriptor(0);
		return this._materializeDenseIndexView(desc);
	}

	/**
	 * Registers a color-encoded dense node attribute derived from an integer attribute or the node index.
	 *
	 * @param {string} sourceName - Source attribute identifier or "index".
	 * @param {string} encodedName - Name of the packed buffer to register.
	 * @param {{format?: DenseColorEncodingFormat|string}} [options] - Encoding options.
	 */
	defineDenseColorEncodedNodeAttribute(sourceName, encodedName, options = {}) {
		this._defineDenseColorEncodedAttribute('node', sourceName, encodedName, options);
	}

	/**
	 * Registers a color-encoded dense edge attribute derived from an integer attribute or edge index.
	 *
	 * @param {string} sourceName - Source attribute identifier or "index".
	 * @param {string} encodedName - Name of the packed buffer to register.
	 * @param {{format?: DenseColorEncodingFormat|string}} [options] - Encoding options.
	 */
	defineDenseColorEncodedEdgeAttribute(sourceName, encodedName, options = {}) {
		this._defineDenseColorEncodedAttribute('edge', sourceName, encodedName, options);
	}

	/**
	 * Removes a color-encoded dense node attribute registration.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 */
	removeDenseColorEncodedNodeAttribute(encodedName) {
		this._removeDenseColorEncodedAttribute('node', encodedName);
	}

	/**
	 * Removes a color-encoded dense edge attribute registration.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 */
	removeDenseColorEncodedEdgeAttribute(encodedName) {
		this._removeDenseColorEncodedAttribute('edge', encodedName);
	}

	/**
	 * Marks a color-encoded dense node buffer dirty.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 */
	markDenseColorEncodedNodeAttributeDirty(encodedName) {
		this._warnDirtyDeprecated('markDenseColorEncodedNodeAttributeDirty');
		this._markDenseColorEncodedAttributeDirty('node', encodedName);
	}

	/**
	 * Marks a color-encoded dense edge buffer dirty.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 */
	markDenseColorEncodedEdgeAttributeDirty(encodedName) {
		this._warnDirtyDeprecated('markDenseColorEncodedEdgeAttributeDirty');
		this._markDenseColorEncodedAttributeDirty('edge', encodedName);
	}

	/**
	 * Refreshes a color-encoded dense node buffer if dirty and returns its typed view.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 * @returns {{view: TypedArray, count: number, dimension: number, type: Function}}
	 */
	updateDenseColorEncodedNodeAttribute(encodedName) {
		this._assertCanAllocate('updateDenseColorEncodedNodeAttribute');
		this._ensureActive();
		this._ensureColorEncodedMetadata('node', encodedName);
		const updateFn = this.module._CXNetworkUpdateDenseColorEncodedNodeAttribute;
		if (typeof updateFn !== 'function') {
			throw new Error('Dense color-encoded attributes are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, encodedName);
		let ptr = 0;
		try {
			ptr = updateFn.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		const desc = this._readDenseBufferDescriptor(ptr);
		const version = (desc.sourceVersion ?? desc.version) ?? this._nextLocalVersion(`dense:nodeColor:${encodedName}`);
		this._denseColorNodeDescriptors.set(encodedName, { ...desc, version });
		return this.getDenseColorEncodedNodeAttributeView(encodedName);
	}

	/**
	 * Refreshes a color-encoded dense edge buffer if dirty and returns its typed view.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 * @returns {{view: TypedArray, count: number, dimension: number, type: Function}}
	 */
	updateDenseColorEncodedEdgeAttribute(encodedName) {
		this._assertCanAllocate('updateDenseColorEncodedEdgeAttribute');
		this._ensureActive();
		this._ensureColorEncodedMetadata('edge', encodedName);
		const updateFn = this.module._CXNetworkUpdateDenseColorEncodedEdgeAttribute;
		if (typeof updateFn !== 'function') {
			throw new Error('Dense color-encoded attributes are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, encodedName);
		let ptr = 0;
		try {
			ptr = updateFn.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		const desc = this._readDenseBufferDescriptor(ptr);
		const version = (desc.sourceVersion ?? desc.version) ?? this._nextLocalVersion(`dense:edgeColor:${encodedName}`);
		this._denseColorEdgeDescriptors.set(encodedName, { ...desc, version });
		return this.getDenseColorEncodedEdgeAttributeView(encodedName);
	}

	/**
	 * Returns a typed view over the last packed color-encoded dense node buffer.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 * @returns {{view: TypedArray, count: number, dimension: number, type: Function}}
	 */
	getDenseColorEncodedNodeAttributeView(encodedName) {
		this._ensureActive();
		const meta = this._ensureColorEncodedMetadata('node', encodedName);
		const desc = this._denseColorNodeDescriptors.get(encodedName) ?? this._readDenseBufferDescriptor(0);
		return this._materializeColorEncodedView(desc, meta);
	}

	/**
	 * Returns a typed view over the last packed color-encoded dense edge buffer.
	 *
	 * @param {string} encodedName - Encoded buffer identifier.
	 * @returns {{view: TypedArray, count: number, dimension: number, type: Function}}
	 */
	getDenseColorEncodedEdgeAttributeView(encodedName) {
		this._ensureActive();
		const meta = this._ensureColorEncodedMetadata('edge', encodedName);
		const desc = this._denseColorEdgeDescriptors.get(encodedName) ?? this._readDenseBufferDescriptor(0);
		return this._materializeColorEncodedView(desc, meta);
	}

	/**
	 * Provides typed dense views inside a buffer access session.
	 *
	 * @param {Array<[string, string]>} requests - List of scope/name pairs; use name "index" to request the index buffer (scope must be "node" or "edge").
	 * @param {(buffers: {node: Record<string, any>, edge: Record<string, any>}) => any} fn - Callback receiving typed views.
	 * @returns {*} Callback return value.
	 */
	withDenseBufferViews(requests, fn) {
		return this.withBufferAccess(() => {
			const result = { node: Object.create(null), edge: Object.create(null) };
			for (const [scope, name] of requests || []) {
				if (scope !== 'node' && scope !== 'edge') {
					throw new Error(`Unknown dense buffer scope "${scope}"`);
				}
				if (name === 'index') {
					result[scope].index = scope === 'node'
						? this.getDenseNodeIndexView()
						: this.getDenseEdgeIndexView();
					continue;
				}
				result[scope][name] = scope === 'node'
					? this.getDenseNodeAttributeView(name)
					: this.getDenseEdgeAttributeView(name);
			}
			return fn(result);
		});
	}

	/**
	 * Updates all requested dense buffers (may allocate) and returns typed views in one call.
	 *
	 * @param {Array<[string,string]>} requests - List of scope/name pairs; use name "index" for the index buffer.
	 * @returns {{node: Record<string, any>, edge: Record<string, any>}} Typed dense views grouped by scope.
	 */
	updateAndGetDenseBufferViews(requests) {
		this._ensureActive();
		const pending = requests || [];
		for (const [scope, name] of pending) {
			if (scope !== 'node' && scope !== 'edge') {
				throw new Error(`Unknown dense buffer scope "${scope}"`);
			}
			if (name === 'index') {
				if (scope === 'node') {
					this.updateDenseNodeIndexBuffer();
				} else {
					this.updateDenseEdgeIndexBuffer();
				}
			} else {
				if (scope === 'node') {
					this.updateDenseNodeAttributeBuffer(name);
				} else {
					this.updateDenseEdgeAttributeBuffer(name);
				}
			}
		}
		return this.withDenseBufferViews(pending, (views) => views);
	}

	/**
	 * Sets the default dense order for nodes. All dense node buffers and the node index buffer
	 * will use this order when none is provided explicitly.
	 *
	 * @param {Uint32Array|number[]} order - Desired node order.
	 */
	setDenseNodeOrder(order) {
		this._assertCanAllocate('setDenseNodeOrder');
		this._ensureActive();
		const orderInfo = this._copyIndicesToWasm(order);
		try {
			this.module._CXNetworkSetDenseNodeOrder(this.ptr, orderInfo.ptr, orderInfo.count);
		} finally {
			orderInfo.dispose();
		}
		this._denseNodeOrderActive = orderInfo.count > 0;
		this._markAllColorEncodedDirty('node');
	}

	/**
	 * Sets the default dense order for edges (applies to edge attributes and edge index buffer).
	 */
	setDenseEdgeOrder(order) {
		this._assertCanAllocate('setDenseEdgeOrder');
		this._ensureActive();
		const orderInfo = this._copyIndicesToWasm(order);
		try {
			this.module._CXNetworkSetDenseEdgeOrder(this.ptr, orderInfo.ptr, orderInfo.count);
		} finally {
			orderInfo.dispose();
		}
		this._denseEdgeOrderActive = orderInfo.count > 0;
		this._markAllColorEncodedDirty('edge');
	}

	/**
	 * Returns information about whether dense node buffers can be treated as contiguous/identity-mapped.
	 * Useful for renderers that want to skip index indirection when possible.
	 *
	 * @returns {{orderActive:boolean,contiguousActive:boolean,canAliasDenseAttributes:boolean,indicesAreIdentity:boolean,validStart:number,validEnd:number,count:number}}
	 */
	getDenseNodePackingInfo() {
		this._assertCanAllocate('getDenseNodePackingInfo');
		this._ensureActive();
		const orderActive = Boolean(this._denseNodeOrderActive);
		const { start: validStart, end: validEnd } = this.nodeValidRange;
		const count = this.nodeCount;
		const contiguousActive = count === Math.max(0, validEnd - validStart);
		const canAliasDenseAttributes = !orderActive && contiguousActive;
		const indicesAreIdentity = canAliasDenseAttributes && validStart === 0;
		return { orderActive, contiguousActive, canAliasDenseAttributes, indicesAreIdentity, validStart, validEnd, count };
	}

	/**
	 * Returns information about whether dense edge buffers can be treated as contiguous/identity-mapped.
	 *
	 * @returns {{orderActive:boolean,contiguousActive:boolean,canAliasDenseAttributes:boolean,indicesAreIdentity:boolean,validStart:number,validEnd:number,count:number}}
	 */
	getDenseEdgePackingInfo() {
		this._assertCanAllocate('getDenseEdgePackingInfo');
		this._ensureActive();
		const orderActive = Boolean(this._denseEdgeOrderActive);
		const { start: validStart, end: validEnd } = this.edgeValidRange;
		const count = this.edgeCount;
		const contiguousActive = count === Math.max(0, validEnd - validStart);
		const canAliasDenseAttributes = !orderActive && contiguousActive;
		const indicesAreIdentity = canAliasDenseAttributes && validStart === 0;
		return { orderActive, contiguousActive, canAliasDenseAttributes, indicesAreIdentity, validStart, validEnd, count };
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
		return { view, start, end, stride, version: this._getAttributeVersion('node', name), bumpVersion: () => this._bumpAttributeVersion('node', name) };
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
		return { view, start, end, stride, version: this._getAttributeVersion('edge', name), bumpVersion: () => this._bumpAttributeVersion('edge', name) };
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
	 * Legacy node-to-edge dense packing has been removed.
	 */
	updateDenseNodeToEdgeAttributeBuffer() {
		throw new Error('updateDenseNodeToEdgeAttributeBuffer has been removed; use defineNodeToEdgeAttribute + updateDenseEdgeAttributeBuffer or copyNodeAttributeToEdgeAttribute.');
	}

	/**
	 * Serializes the network into the `.bxnet` container format.
	 *
	 * @param {object} [options]
	 * @param {string} [options.path] - Node-only destination path. When omitted the bytes are returned.
	 * @param {'uint8array'|'arraybuffer'|'base64'|'blob'} [options.format='uint8array'] - Desired return representation.
	 * @returns {Promise<Uint8Array|ArrayBuffer|string|Blob|undefined>} Serialized payload or void when writing directly to disk.
	 */
	async saveBXNet(options = {}) {
		return this._saveSerialized('bxnet', options);
	}

	/**
	 * Serializes the network into the human-readable `.xnet` container format.
	 *
	 * @param {object} [options]
	 * @param {string} [options.path] - Node-only destination path. When omitted the bytes are returned.
	 * @param {'uint8array'|'arraybuffer'|'base64'|'blob'} [options.format='uint8array'] - Desired return representation.
	 * @returns {Promise<Uint8Array|ArrayBuffer|string|Blob|undefined>} Serialized payload or void when writing directly to disk.
	 */
	async saveXNet(options = {}) {
		return this._saveSerialized('xnet', options);
	}

	/**
	 * Serializes the network into the BGZF-compressed `.zxnet` container format.
	 *
	 * @param {object} [options]
	 * @param {string} [options.path] - Node-only destination path. When omitted the bytes are returned.
	 * @param {number} [options.compressionLevel=6] - BGZF compression level (0-9).
	 * @param {'uint8array'|'arraybuffer'|'base64'|'blob'} [options.format='uint8array'] - Desired return representation.
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
	 * Hydrates a native neighbor container into JavaScript typed arrays.
	 * @private
	 *
	 * @param {number} containerPtr - Pointer to the native container.
	 * @returns {{nodes: Uint32Array, edges: Uint32Array}} Neighbor information.
	 */
	_readNeighborContainer(containerPtr) {
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
		return this._bumpAttributeVersion('node', name);
	}

	bumpEdgeAttributeVersion(name) {
		return this._bumpAttributeVersion('edge', name);
	}

	bumpNetworkAttributeVersion(name) {
		return this._bumpAttributeVersion('network', name);
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
		return this._getAttributeBuffer('network', name);
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

	_bumpAttributeVersion(scope, name) {
		const bumpFn = scope === 'node'
			? this.module._CXNetworkBumpNodeAttributeVersion
			: scope === 'edge'
				? this.module._CXNetworkBumpEdgeAttributeVersion
				: this.module._CXNetworkBumpNetworkAttributeVersion;
		this._ensureActive();
		if (typeof bumpFn !== 'function') {
			if (scope === 'node' || scope === 'edge') {
				this._markDenseAttributeDirtySilently(scope, name);
			}
			if (scope === 'node') {
				this._markPassthroughEdgesDirtyForNode(name);
				this._markColorEncodedDirtyForSource('node', name);
			} else if (scope === 'edge') {
				this._markColorEncodedDirtyForSource('edge', name);
			}
			return this._nextLocalVersion(`attr:${scope}:${name}`);
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
			this._markColorEncodedDirtyForSource('node', name);
		} else if (scope === 'edge') {
			this._markColorEncodedDirtyForSource('edge', name);
		}
		return bumped;
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
				bumpVersion: () => this._bumpAttributeVersion(scope, name),
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
				bumpVersion: () => this._bumpAttributeVersion(scope, name),
			};
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
			bumpVersion: () => this._bumpAttributeVersion(scope, name),
		};
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
		} else {
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
		this._bumpAttributeVersion(scope, name);
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
		this._bumpAttributeVersion(scope, name);
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
		return Array.from(this._attributeMap(scope).keys());
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

	_colorEncodedRegistry(scope) {
		if (scope === 'node') return this._denseColorNodeAttributes;
		if (scope === 'edge') return this._denseColorEdgeAttributes;
		throw new Error(`Unknown color-encoded scope "${scope}"`);
	}

	_colorEncodedDescriptors(scope) {
		if (scope === 'node') return this._denseColorNodeDescriptors;
		if (scope === 'edge') return this._denseColorEdgeDescriptors;
		throw new Error(`Unknown color-encoded scope "${scope}"`);
	}

	_ensureColorEncodedMetadata(scope, encodedName) {
		const registry = this._colorEncodedRegistry(scope);
		const meta = registry.get(encodedName);
		if (!meta) {
			throw new Error(`Color-encoded ${scope} attribute "${encodedName}" is not defined`);
		}
		return meta;
	}

	_normalizeColorEncodingFormat(format) {
		if (format === DenseColorEncodingFormat.Uint32x4 || format === 'u32' || format === 'u32x4' || format === 'uint32' || format === 'rgba32') {
			return DenseColorEncodingFormat.Uint32x4;
		}
		if (format === DenseColorEncodingFormat.Uint8x4 || format === 'u8' || format === 'u8x4' || format === 'uint8' || format === 'rgba8') {
			return DenseColorEncodingFormat.Uint8x4;
		}
		return DenseColorEncodingFormat.Uint8x4;
	}

	_defineDenseColorEncodedAttribute(scope, sourceName, encodedName, options) {
		this._assertCanAllocate(`define dense color-encoded ${scope} attribute`);
		this._ensureActive();
		const registry = this._colorEncodedRegistry(scope);
		const format = this._normalizeColorEncodingFormat(options?.format);
		const existing = registry.get(encodedName);
		if (existing) {
			if (existing.sourceName !== sourceName || existing.format !== format) {
				throw new Error(`Color-encoded ${scope} attribute "${encodedName}" already exists with a different configuration`);
			}
			return;
		}
		const useIndex = sourceName === '$index';
		if (!useIndex) {
			const meta = this._ensureAttributeMetadata(scope, sourceName);
			if (meta.type !== AttributeType.Integer && meta.type !== AttributeType.UnsignedInteger) {
				throw new Error('Color encoding only supports 32-bit integer attributes');
			}
			const dimension = Math.max(1, meta.dimension || 1);
			if (dimension !== 1) {
				throw new Error('Color encoding only supports scalar attributes');
			}
		}
		const defineFn = scope === 'node'
			? this.module._CXNetworkDefineDenseColorEncodedNodeAttribute
			: this.module._CXNetworkDefineDenseColorEncodedEdgeAttribute;
		if (typeof defineFn !== 'function') {
			throw new Error('Dense color-encoded attributes are unavailable in this WASM build');
		}
		const sourceCstr = new CString(this.module, sourceName);
		const encodedCstr = new CString(this.module, encodedName);
		let ok = false;
		try {
			ok = defineFn.call(this.module, this.ptr, sourceCstr.ptr, encodedCstr.ptr, format);
		} finally {
			sourceCstr.dispose();
			encodedCstr.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to define color-encoded ${scope} attribute "${encodedName}"`);
		}
		registry.set(encodedName, { sourceName, format, useIndex });
	}

	_removeDenseColorEncodedAttribute(scope, encodedName) {
		this._assertCanAllocate(`remove dense color-encoded ${scope} attribute`);
		this._ensureActive();
		const registry = this._colorEncodedRegistry(scope);
		if (!registry.has(encodedName)) {
			return;
		}
		const removeFn = scope === 'node'
			? this.module._CXNetworkRemoveDenseColorEncodedNodeAttribute
			: this.module._CXNetworkRemoveDenseColorEncodedEdgeAttribute;
		if (typeof removeFn !== 'function') {
			throw new Error('Dense color-encoded attributes are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, encodedName);
		let ok = false;
		try {
			ok = removeFn.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		if (!ok) {
			throw new Error(`Failed to remove color-encoded ${scope} attribute "${encodedName}"`);
		}
		registry.delete(encodedName);
		this._colorEncodedDescriptors(scope).delete(encodedName);
	}

	_markDenseColorEncodedAttributeDirty(scope, encodedName) {
		this._ensureActive();
		const registry = this._colorEncodedRegistry(scope);
		if (!registry.has(encodedName)) {
			throw new Error(`Color-encoded ${scope} attribute "${encodedName}" is not defined`);
		}
		const markFn = scope === 'node'
			? this.module._CXNetworkMarkDenseColorEncodedNodeAttributeDirty
			: this.module._CXNetworkMarkDenseColorEncodedEdgeAttributeDirty;
		if (typeof markFn !== 'function') {
			throw new Error('Dense color-encoded attributes are unavailable in this WASM build');
		}
		const cstr = new CString(this.module, encodedName);
		try {
			markFn.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		const descriptors = this._colorEncodedDescriptors(scope);
		const desc = descriptors.get(encodedName);
		if (desc) {
			descriptors.set(encodedName, { ...desc, dirty: true });
		}
	}

	_markColorEncodedDirtyForSource(scope, sourceName) {
		const registry = this._colorEncodedRegistry(scope);
		const descriptors = this._colorEncodedDescriptors(scope);
		for (const [name, meta] of registry.entries()) {
			if (meta.sourceName !== sourceName) {
				continue;
			}
			const markFn = scope === 'node'
				? this.module._CXNetworkMarkDenseColorEncodedNodeAttributeDirty
				: this.module._CXNetworkMarkDenseColorEncodedEdgeAttributeDirty;
			if (typeof markFn === 'function') {
				const cstr = new CString(this.module, name);
				try {
					markFn.call(this.module, this.ptr, cstr.ptr);
				} finally {
					cstr.dispose();
				}
			}
			const desc = descriptors.get(name);
			if (desc) {
				descriptors.set(name, { ...desc, dirty: true, sourceVersion: 0 });
			}
		}
	}

	_markDenseAttributeDirtySilently(scope, name) {
		this._ensureActive();
		const fn = scope === 'node'
			? this.module._CXNetworkMarkDenseNodeAttributeDirty
			: this.module._CXNetworkMarkDenseEdgeAttributeDirty;
		if (typeof fn !== 'function') {
			return;
		}
		const cstr = new CString(this.module, name);
		try {
			fn.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
	}

	_markAllColorEncodedDirty(scope) {
		const descriptors = this._colorEncodedDescriptors(scope);
		for (const [name, desc] of descriptors.entries()) {
			descriptors.set(name, { ...desc, dirty: true, sourceVersion: 0 });
		}
	}

	_warnDirtyDeprecated(method) {
		if (this._dirtyWarningKeys.has(method)) {
			return;
		}
		this._dirtyWarningKeys.add(method);
		// eslint-disable-next-line no-console
		console.warn(`[HeliosNetwork] ${method} is deprecated: rely on versioned buffers instead of dirty flags.`);
	}

	_removeColorEncodedAttributesForSource(scope, sourceName) {
		const registry = this._colorEncodedRegistry(scope);
		const names = [];
		for (const [name, meta] of registry.entries()) {
			if (meta.sourceName === sourceName) {
				names.push(name);
			}
		}
		for (const name of names) {
			this._removeDenseColorEncodedAttribute(scope, name);
		}
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

	_canAliasDenseAttributeBuffer(scope) {
		if (scope === 'node') {
			if (this._denseNodeOrderActive) {
				return false;
			}
			const { start, end } = this.nodeValidRange;
			return this.nodeCount === (end - start);
		}
		if (scope === 'edge') {
			if (this._denseEdgeOrderActive) {
				return false;
			}
			const { start, end } = this.edgeValidRange;
			return this.edgeCount === (end - start);
		}
		return false;
	}

	_refreshAliasedDenseAttributeDescriptor(scope, name) {
		const descriptors = scope === 'node' ? this._denseNodeAttributeDescriptors : this._denseEdgeAttributeDescriptors;
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta) {
			descriptors.set(name, this._readDenseBufferDescriptor(0));
			return;
		}
		const version = this._getAttributeVersion(scope, name);
		const { start, end } = scope === 'node' ? this.nodeValidRange : this.edgeValidRange;
		const count = Math.max(0, end - start);
		const stride = meta.stride || (meta.attributePtr ? this.module._CXAttributeStride(meta.attributePtr) : 0);
		const previous = descriptors.get(name);

		if (
			previous
			&& previous.version === version
			&& previous.count === count
			&& previous.stride === stride
			&& previous.capacity === (count * stride)
			&& (count === 0 || previous.pointer)
			&& (count === 0 || (previous.validStart === start && previous.validEnd === end))
		) {
			return;
		}

		if (!count) {
			descriptors.set(name, {
				pointer: 0,
				count: 0,
				capacity: 0,
				stride,
				validStart: 0,
				validEnd: 0,
				dirty: false,
				version,
			});
			return;
		}

		let pointers = null;
		try {
			pointers = this._attributePointers(scope, name, meta);
		} catch (_) {
			descriptors.set(name, this._readDenseBufferDescriptor(0));
			return;
		}
		descriptors.set(name, {
			pointer: (pointers.pointer + (start * pointers.stride)) >>> 0,
			count,
			capacity: count * pointers.stride,
			stride: pointers.stride,
			validStart: start,
			validEnd: end,
			dirty: false,
			version,
		});
	}

	_buildAliasedDenseAttributeDescriptor(scope, name) {
		const meta = this._ensureAttributeMetadata(scope, name);
		if (!meta) {
			return this._readDenseBufferDescriptor(0);
		}
		let pointers = null;
		try {
			pointers = this._attributePointers(scope, name, meta);
		} catch (_) {
			return this._readDenseBufferDescriptor(0);
		}
		const { start, end } = scope === 'node' ? this.nodeValidRange : this.edgeValidRange;
		const count = Math.max(0, end - start);
		if (!count) {
			return {
				pointer: 0,
				count: 0,
				capacity: 0,
				stride: pointers.stride,
				validStart: 0,
				validEnd: 0,
				dirty: false,
				version: this._getAttributeVersion(scope, name),
			};
		}
		const pointer = (pointers.pointer + (start * pointers.stride)) >>> 0;
		return {
			pointer,
			count,
			capacity: count * pointers.stride,
			stride: pointers.stride,
			validStart: start,
			validEnd: end,
			dirty: false,
			version: this._getAttributeVersion(scope, name),
		};
	}

	_readDenseBufferDescriptor(ptr) {
		if (!ptr) {
			return {
				pointer: 0,
				count: 0,
				capacity: 0,
				stride: 0,
				validStart: 0,
				validEnd: 0,
				dirty: false,
				version: 0,
				topologyVersion: 0,
				sourceVersion: 0,
			};
		}
		const base = ptr >>> 2;
		const dataPtr = this.module.HEAPU32[base + 1];
		const count = this.module.HEAPU32[base + 2];
		const capacityBytes = this.module.HEAPU32[base + 3];
		const stride = this.module.HEAPU32[base + 4];
		const validStart = this.module.HEAPU32[base + 5];
		const validEnd = this.module.HEAPU32[base + 6];
		const dirty = !!this.module.HEAPU8[(base + 7) * 4];
		const versionFn = this.module._CXDenseAttributeBufferVersion;
		const sourceVersionFn = this.module._CXDenseAttributeBufferSourceVersion;
		const version = typeof versionFn === 'function' ? (this._normalizeVersion(versionFn.call(this.module, ptr)) ?? 0) : 0;
		const sourceVersion = typeof sourceVersionFn === 'function' ? (this._normalizeVersion(sourceVersionFn.call(this.module, ptr)) ?? 0) : 0;
		return {
			pointer: dataPtr,
			count,
			capacity: capacityBytes,
			stride,
			validStart,
			validEnd,
			dirty,
			version,
			topologyVersion: 0,
			sourceVersion,
		};
	}

	_materializeColorEncodedView(descriptor, meta) {
		const TypedArrayCtor = meta.format === DenseColorEncodingFormat.Uint32x4 ? Uint32Array : Uint8Array;
		const length = descriptor.count * 4;
		const view = descriptor.pointer
			? new TypedArrayCtor(this.module.HEAPU8.buffer, descriptor.pointer, length)
			: new TypedArrayCtor(0);
		return { ...descriptor, view, count: descriptor.count, dimension: 4, type: TypedArrayCtor };
	}

	_materializeDenseAttributeView(descriptor, meta) {
		const TypedArrayCtor = DenseTypedArrayForType[meta.type] || Uint8Array;
		const strideElements = descriptor.stride && TypedArrayCtor.BYTES_PER_ELEMENT
			? descriptor.stride / TypedArrayCtor.BYTES_PER_ELEMENT
			: 0;
		const perEntry = strideElements || Math.max(1, Math.floor((meta.dimension * TYPE_ELEMENT_SIZE[meta.type]) / TypedArrayCtor.BYTES_PER_ELEMENT));
		const length = descriptor.count * perEntry;
		const view = descriptor.pointer
			? new TypedArrayCtor(this.module.HEAPU8.buffer, descriptor.pointer, length)
			: new TypedArrayCtor(0);
		return {
			...descriptor,
			view,
		};
	}

	_materializeDenseIndexView(descriptor) {
		if (descriptor?.jsView && descriptor.jsView instanceof Uint32Array) {
			return { ...descriptor, view: descriptor.jsView };
		}
		const view = descriptor.pointer
			? new Uint32Array(this.module.HEAPU8.buffer, descriptor.pointer, descriptor.count)
			: EMPTY_UINT32;
		return { ...descriptor, view };
	}

	_materializeActiveIndexCopy(scope) {
		this._refreshTopologyVersions(scope);
		const cache = scope === 'node' ? this._nodeIndexCache : this._edgeIndexCache;
		const version = scope === 'node' ? this._nodeTopologyVersion : this._edgeTopologyVersion;
		if (cache && cache.version === version) {
			return cache.data;
		}

		this._ensureActive();
		const count = scope === 'node' ? this.nodeCount : this.edgeCount;
		if (count === 0) {
			const empty = new Uint32Array();
			if (scope === 'node') {
				this._nodeIndexCache = { version, data: empty };
			} else {
				this._edgeIndexCache = { version, data: empty };
			}
			return empty;
		}
		const bytes = count * Uint32Array.BYTES_PER_ELEMENT;
		const ptr = this.module._malloc(bytes);
		if (!ptr) {
			throw new Error('Failed to allocate WASM memory for indices');
		}
		try {
			const view = new Uint32Array(this.module.HEAPU32.buffer, ptr, count);
			const writer = scope === 'node' ? this.writeActiveNodes.bind(this) : this.writeActiveEdges.bind(this);
			let written = writer(view);
			if (written > count) {
				// structure changed between count and write; retry with larger buffer
				this.module._free(ptr);
				const retryPtr = this.module._malloc(written * Uint32Array.BYTES_PER_ELEMENT);
				if (!retryPtr) {
					throw new Error('Failed to allocate WASM memory for indices');
				}
				try {
					const retryView = new Uint32Array(this.module.HEAPU32.buffer, retryPtr, written);
					written = writer(retryView);
					const data = retryView.slice(0, written);
					if (scope === 'node') {
						this._nodeIndexCache = { version: this._nodeTopologyVersion, data };
					} else {
						this._edgeIndexCache = { version: this._edgeTopologyVersion, data };
					}
					return data;
				} finally {
					this.module._free(retryPtr);
				}
			}
			const data = view.slice(0, written);
			if (scope === 'node') {
				this._nodeIndexCache = { version: this._nodeTopologyVersion, data };
			} else {
				this._edgeIndexCache = { version: this._edgeTopologyVersion, data };
			}
			return data;
		} finally {
			if (ptr) {
				this.module._free(ptr);
			}
		}
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
			this._bumpAttributeVersion(scope, name);
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
		this._bumpAttributeVersion(scope, name);
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
	 * Copies a node attribute into an edge attribute buffer (sparse) with endpoint control.
	 * Useful when you want to start from a passthrough and then tweak edge values manually.
	 *
	 * @param {string} sourceName - Node attribute identifier.
	 * @param {string} destinationName - Edge attribute identifier.
	 * @param {'source'|'destination'|'both'|0|1|-1} [endpoints='both'] - Which endpoints to copy.
	 * @param {boolean} [doubleWidth=true] - When copying a single endpoint, duplicate it to fill double width.
	 */
	copyNodeAttributeToEdgeAttribute(sourceName, destinationName, endpoints = 'both', doubleWidth = true) {
		this._copyNodeToEdgeAttribute(sourceName, destinationName, this._normalizeEndpointMode(endpoints), doubleWidth);
		this._markDenseAttributeDirtySilently('edge', destinationName);
		const passthrough = this._nodeToEdgePassthrough.get(destinationName);
		if (passthrough) {
			passthrough.dirty = false;
		}
	}

	_removeAttribute(scope, name) {
		this._ensureActive();
		const metaMap = this._attributeMap(scope);
		const meta = metaMap.get(name);
		if (!meta) {
			throw new Error(`Attribute "${name}" on ${scope} is not defined`);
		}
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
		if (scope === 'node') {
			const dependents = this._nodeAttributeDependents.get(name);
			if (dependents) {
				for (const edgeName of dependents) {
					this._nodeToEdgePassthrough.delete(edgeName);
					this._markDenseAttributeDirtySilently('edge', edgeName);
				}
				this._nodeAttributeDependents.delete(name);
			}
			this._denseNodeAttributeDescriptors.delete(name);
			this._registeredDenseNodeAttributes.delete(name);
		} else if (scope === 'edge') {
			if (this._nodeToEdgePassthrough.has(name)) {
				const entry = this._nodeToEdgePassthrough.get(name);
				this._unregisterNodeToEdgeDependency(entry.sourceName, name);
				this._nodeToEdgePassthrough.delete(name);
			}
			for (const dependents of this._nodeAttributeDependents.values()) {
				dependents.delete(name);
			}
			this._denseEdgeAttributeDescriptors.delete(name);
			this._registeredDenseEdgeAttributes.delete(name);
		}
		if (scope === 'node' || scope === 'edge') {
			this._removeColorEncodedAttributesForSource(scope, name);
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
		this._bumpAttributeVersion('edge', destinationName);
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
			this._markDenseAttributeDirtySilently('edge', edgeName);
			const entry = this._nodeToEdgePassthrough.get(edgeName);
			if (entry) {
				entry.dirty = true;
			}
		}
	}

	_markAllPassthroughEdgesDirty() {
		for (const edgeName of this._nodeToEdgePassthrough.keys()) {
			this._markDenseAttributeDirtySilently('edge', edgeName);
			const entry = this._nodeToEdgePassthrough.get(edgeName);
			if (entry) {
				entry.dirty = true;
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
		const {
			nodeOriginalIndexAttribute = null,
			edgeOriginalIndexAttribute = null,
		} = options;

		if (typeof this.module._CXNetworkCompact !== 'function') {
			throw new Error('CXNetworkCompact is not available in this WASM build. Rebuild the module to enable compact().');
		}

		const nodeIndices = this.nodeIndices;
		const edgeIndices = this.edgeIndices;
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
		this._invalidateAttributePointerCache();
		this._nodeValidRangeCache = null;
		this._edgeValidRangeCache = null;
		return this;
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
	 * @param {number} [options.seed=0] - RNG seed (0 uses a default seed).
	 * @param {number} [options.maxLevels=32] - Maximum aggregation levels.
	 * @param {number} [options.maxPasses=8] - Max local-moving passes per phase.
	 * @returns {{communityCount:number, modularity:number}} Result summary.
	 */
	leidenModularity(options = {}) {
		this._ensureActive();
		this._assertCanAllocate('Leiden community detection');
		const {
			resolution = 1,
			edgeWeightAttribute = null,
			outNodeCommunityAttribute = 'community',
			seed = 0,
			maxLevels = 32,
			maxPasses = 8,
		} = options;

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
				maxPasses >>> 0,
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
				type: AttributeType.UnsignedInteger,
				dimension: 1,
				complex: false,
				jsStore: new Map(),
				stringPointers: new Map(),
				nextHandle: 1,
			});
		} else {
			const meta = this._nodeAttributes.get(outNodeCommunityAttribute);
			if (meta && meta.type !== AttributeType.UnsignedInteger) {
				throw new Error(`Node attribute "${outNodeCommunityAttribute}" must be UnsignedInteger`);
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
	 * @param {string} [options.outNodeCommunityAttribute='community'] - Default output name for finalize().
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
			maxPasses = 8,
			outNodeCommunityAttribute = 'community',
		} = options;

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
				maxPasses >>> 0
			);
		} finally {
			if (weightName) {
				weightName.dispose();
			}
		}

		if (!ptr) {
			throw new Error('Failed to create Leiden session');
		}
		return new LeidenSession(this.module, this, ptr, { outNodeCommunityAttribute });
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
		let writeFn;
		let funcLabel;
		let humanLabel;
		switch (kind) {
			case 'bxnet':
				writeFn = module._CXNetworkWriteBXNet;
				funcLabel = 'WriteBXNet';
				humanLabel = '.bxnet';
				break;
			case 'zxnet':
				writeFn = module._CXNetworkWriteZXNet;
				funcLabel = 'WriteZXNet';
				humanLabel = '.zxnet';
				break;
			case 'xnet':
				writeFn = module._CXNetworkWriteXNet;
				funcLabel = 'WriteXNet';
				humanLabel = '.xnet';
				break;
			default:
				throw new Error(`Unsupported serialization kind: ${kind}`);
		}
		if (typeof writeFn !== 'function') {
			throw new Error(`CXNetwork${funcLabel} is not available in this WASM build. Rebuild the artefacts to use serialization helpers.`);
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
			if (passthroughSnapshot.length) {
				for (const entry of passthroughSnapshot) {
					this.removeEdgeAttribute(entry.edgeName);
				}
			}
			if (kind === 'zxnet') {
				const level = clampCompressionLevel(options?.compressionLevel ?? 6);
				success = writeFn.call(module, this.ptr, cPath.ptr, level);
			} else {
				success = writeFn.call(module, this.ptr, cPath.ptr);
			}
		} finally {
			cPath.dispose();
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

export { AttributeType, DenseColorEncodingFormat, NodeSelector, EdgeSelector, getModule as getHeliosModule };
export default HeliosNetwork;
