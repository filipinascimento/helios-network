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
	Unknown: 255,
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
	[AttributeType.Integer]: BigInt64Array,
	[AttributeType.UnsignedInteger]: BigUint64Array,
	[AttributeType.Category]: Uint32Array,
};

/**
 * Size in bytes of each attribute type element.
 * @const {Record<number, number>}
 */
const TYPE_ELEMENT_SIZE = {
	[AttributeType.Boolean]: 1,
	[AttributeType.Float]: 4,
	[AttributeType.Double]: 8,
	[AttributeType.Integer]: 8,
	[AttributeType.UnsignedInteger]: 8,
	[AttributeType.Category]: 4,
	[AttributeType.String]: 4,
	[AttributeType.Data]: 4,
	[AttributeType.Javascript]: 4,
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
const EMPTY_UINT8 = new Uint8Array(0);

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
	 */
	constructor(module, network, ptr, fns) {
		this.module = module;
		this.network = network;
		this.ptr = ptr;
		this._destroyFn = fns.destroyFn;
		this._countFn = fns.countFn;
		this._dataFn = fns.dataFn;
	}

	/**
	 * @returns {number} Number of stored indices.
	 */
	get count() {
		return this._countFn(this.ptr);
	}

	/**
	 * @returns {number} Pointer to the selector data buffer.
	 */
	get dataPointer() {
		return this._dataFn(this.ptr);
	}

	/**
	 * Copies the selector's indices into a new `Uint32Array`.
	 *
	 * @returns {Uint32Array} Copied selection.
	 */
	toTypedArray() {
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
		});
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
		});
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
		this._denseNodeAttributeCache = new Map();
		this._denseEdgeAttributeCache = new Map();
		this._denseNodeIndexCache = null;
		this._denseEdgeIndexCache = null;
		this._nodeToEdgePassthrough = new Map();
		this._nodeAttributeDependents = new Map();
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

	/**
	 * @returns {number} Total number of active nodes.
	 */
	get nodeCount() {
		this._ensureActive();
		return this.module._CXNetworkNodeCount(this.ptr);
	}

	/**
	 * @returns {number} Total number of active edges.
	 */
	get edgeCount() {
		this._ensureActive();
		return this.module._CXNetworkEdgeCount(this.ptr);
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
	 * @returns {Uint8Array} Bitmask view describing active nodes.
	 */
	get nodeActivityView() {
		this._ensureActive();
		const ptr = this.module._CXNetworkNodeActivityBuffer(this.ptr);
		return new Uint8Array(this.module.HEAPU8.buffer, ptr, this.nodeCapacity);
	}

	/**
	 * @returns {Uint8Array} Bitmask view describing active edges.
	 */
	get edgeActivityView() {
		this._ensureActive();
		const ptr = this.module._CXNetworkEdgeActivityBuffer(this.ptr);
		return new Uint8Array(this.module.HEAPU8.buffer, ptr, this.edgeCapacity);
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
	 * Registers a dense node attribute buffer that can be refreshed on demand.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {number} [initialCapacityBytes=0] - Optional initial dense buffer capacity in bytes.
	 */
	addDenseNodeAttributeBuffer(name, initialCapacityBytes = 0) {
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkAddDenseNodeAttribute(this.ptr, cstr.ptr, initialCapacityBytes);
		} finally {
			cstr.dispose();
		}
	}

	/**
	/**
	 * Registers a dense edge attribute buffer.
	 */
	addDenseEdgeAttributeBuffer(name, initialCapacityBytes = 0) {
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkAddDenseEdgeAttribute(this.ptr, cstr.ptr, initialCapacityBytes);
		} finally {
			cstr.dispose();
		}
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
		this.markDenseEdgeAttributeDirty(edgeName);
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
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkMarkDenseNodeAttributeDirty(this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		this._markPassthroughEdgesDirtyForNode(name);
	}

	/**
	 * Marks a dense edge attribute buffer dirty.
	 */
	markDenseEdgeAttributeDirty(name) {
		this._ensureActive();
		const cstr = new CString(this.module, name);
		try {
			this.module._CXNetworkMarkDenseEdgeAttributeDirty(this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
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
		this._ensureActive();
		const cstr = new CString(this.module, name);
		let ptr = 0;
		try {
			// console.log('TRYING: updateDenseNodeAttributeBuffer', name, ptr);
			ptr = this.module._CXNetworkUpdateDenseNodeAttribute(this.ptr, cstr.ptr);
			// console.log('SUCCESS: updateDenseNodeAttributeBuffer', name, ptr);
		}finally {
			cstr.dispose();
		}
		const parsed = this._parseDenseBuffer(ptr);
		this._denseNodeAttributeCache.set(name, parsed);
		return parsed;
	}

	/**
	 * Refreshes a dense edge attribute buffer.
	 */
	updateDenseEdgeAttributeBuffer(name) {
		if (!this.hasEdgeAttribute(name)) {
			throw new Error(`Unknown edge attribute "${name}"`);
		}
		this._ensureActive();
		const passthrough = this._nodeToEdgePassthrough.get(name);
		if (passthrough) {
			if (passthrough.dirty) {
				this._copyNodeToEdgeAttribute(passthrough.sourceName, name, passthrough.endpointMode, passthrough.doubleWidth);
				this.markDenseEdgeAttributeDirty(name);
				passthrough.dirty = false;
			}
		}
		const cstr = new CString(this.module, name);
		let ptr = 0;
		try {
			ptr = this.module._CXNetworkUpdateDenseEdgeAttribute(this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		const parsed = this._parseDenseBuffer(ptr);
		this._denseEdgeAttributeCache.set(name, parsed);
		return parsed;
	}

	/**
	 * Returns a dense buffer of active node indices using the stored dense node order.
	 */
	updateDenseNodeIndexBuffer() {
		this._ensureActive();
		let ptr = 0;
		try {
			ptr = this.module._CXNetworkUpdateDenseNodeIndexBuffer(this.ptr);
		} finally {
		}
		const parsed = this._parseDenseBuffer(ptr);
		this._denseNodeIndexCache = parsed;
		return parsed;
	}

	/**
	 * Returns a dense buffer of active edge indices using the stored dense edge order.
	 */
	updateDenseEdgeIndexBuffer() {
		this._ensureActive();
		let ptr = 0;
		try {
			ptr = this.module._CXNetworkUpdateDenseEdgeIndexBuffer(this.ptr);
		} finally {
		}
		const parsed = this._parseDenseBuffer(ptr);
		this._denseEdgeIndexCache = parsed;
		return parsed;
	}

	/**
	 * Returns the most recent dense node attribute buffer descriptor without triggering an update.
	 */
	peekDenseNodeAttributeBuffer(name) {
		return this._denseNodeAttributeCache.get(name) ?? this._parseDenseBuffer(0);
	}

	/**
	 * Returns the most recent dense edge attribute buffer descriptor without triggering an update.
	 */
	peekDenseEdgeAttributeBuffer(name) {
		return this._denseEdgeAttributeCache.get(name) ?? this._parseDenseBuffer(0);
	}

	/**
	 * Returns the most recent dense node index buffer descriptor without triggering an update.
	 */
	peekDenseNodeIndexBuffer() {
		return this._denseNodeIndexCache ?? this._parseDenseBuffer(0);
	}

	/**
	 * Returns the most recent dense edge index buffer descriptor without triggering an update.
	 */
	peekDenseEdgeIndexBuffer() {
		return this._denseEdgeIndexCache ?? this._parseDenseBuffer(0);
	}

	/**
	 * Sets the default dense order for nodes. All dense node buffers and the node index buffer
	 * will use this order when none is provided explicitly.
	 *
	 * @param {Uint32Array|number[]} order - Desired node order.
	 */
	setDenseNodeOrder(order) {
		this._ensureActive();
		const orderInfo = this._copyIndicesToWasm(order);
		try {
			this.module._CXNetworkSetDenseNodeOrder(this.ptr, orderInfo.ptr, orderInfo.count);
		} finally {
			orderInfo.dispose();
		}
	}

	/**
	 * Sets the default dense order for edges (applies to edge attributes and edge index buffer).
	 */
	setDenseEdgeOrder(order) {
		this._ensureActive();
		const orderInfo = this._copyIndicesToWasm(order);
		try {
			this.module._CXNetworkSetDenseEdgeOrder(this.ptr, orderInfo.ptr, orderInfo.count);
		} finally {
			orderInfo.dispose();
		}
	}

	/**
	 * Returns the min/max active node indices as {start,end}.
	 */
	get nodeValidRange() {
		this._ensureActive();
		const startPtr = this.module._malloc(8);
		const endPtr = this.module._malloc(8);
		try {
			this.module._CXNetworkGetNodeValidRange(this.ptr, startPtr, endPtr);
			const start = Number(this.module.HEAPU32[startPtr >>> 2]);
			const end = Number(this.module.HEAPU32[endPtr >>> 2]);
			return { start, end };
		} finally {
			this.module._free(startPtr);
			this.module._free(endPtr);
		}
	}

	/**
	 * Returns the min/max active edge indices as {start,end}.
	 */
	get edgeValidRange() {
		this._ensureActive();
		const startPtr = this.module._malloc(8);
		const endPtr = this.module._malloc(8);
		try {
			this.module._CXNetworkGetEdgeValidRange(this.ptr, startPtr, endPtr);
			const start = Number(this.module.HEAPU32[startPtr >>> 2]);
			const end = Number(this.module.HEAPU32[endPtr >>> 2]);
			return { start, end };
		} finally {
			this.module._free(startPtr);
			this.module._free(endPtr);
		}
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
		return { view, start, end, stride };
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
		return { view, start, end, stride };
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
		const capacity = scope === 'node' ? this.nodeCapacity : scope === 'edge' ? this.edgeCapacity : 1;

		if (meta.complex) {
			return {
				type: meta.type,
				dimension: meta.dimension,
				get: (index = 0) => meta.jsStore.get(index)?.value ?? null,
				set: (index, value) => this._setComplexAttribute(scope, name, meta, index, value, pointer),
				delete: (index) => this._deleteComplexAttribute(scope, meta, index, pointer),
				store: meta.jsStore,
			};
		}

		if (meta.type === AttributeType.String) {
			return {
				type: meta.type,
				dimension: meta.dimension,
				getView: () => new Uint32Array(this.module.HEAPU32.buffer, pointer, capacity * meta.dimension),
				getString: (index) => {
					const view = new Uint32Array(this.module.HEAPU32.buffer, pointer, capacity * meta.dimension);
					return this.module.UTF8ToString(view[index]);
				},
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
		const getter = scope === 'node'
			? this.module._CXNetworkGetNodeAttribute
			: scope === 'edge'
				? this.module._CXNetworkGetEdgeAttribute
				: this.module._CXNetworkGetNetworkAttribute;
		const bufferGetter = scope === 'node'
			? this.module._CXNetworkGetNodeAttributeBuffer
			: scope === 'edge'
				? this.module._CXNetworkGetEdgeAttributeBuffer
				: this.module._CXNetworkGetNetworkAttributeBuffer;

		const cstr = new CString(this.module, name);
		let attributePtr = 0;
		let bufferPtr = 0;
		try {
			attributePtr = getter.call(this.module, this.ptr, cstr.ptr);
			bufferPtr = bufferGetter.call(this.module, this.ptr, cstr.ptr);
		} finally {
			cstr.dispose();
		}
		if (!bufferPtr) {
			throw new Error(`Attribute buffer for "${name}" is not available`);
		}
		const stride = this.module._CXAttributeStride(attributePtr);
		return { pointer: bufferPtr, stride };
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
	_deleteComplexAttribute(scope, meta, index, pointer) {
		const buffer = new Uint32Array(this.module.HEAPU32.buffer, pointer, this._capacityForScope(scope));
		meta.jsStore.delete(index);
		buffer[index] = 0;
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

	_copyIndicesToWasm(indices) {
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

	_parseDenseBuffer(ptr) {
		if (!ptr) {
			return {
				view: EMPTY_UINT8,
				count: 0,
				capacity: 0,
				stride: 0,
				validStart: 0,
				validEnd: 0,
				dirty: false,
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
		const view = dataPtr ? new Uint8Array(this.module.HEAPU8.buffer, dataPtr, capacityBytes) : EMPTY_UINT8;
		return {
			view,
			count,
			capacity: capacityBytes,
			stride,
			validStart,
			validEnd,
			pointer: dataPtr,
			dirty,
		};
	}

	_ensureAttributeMetadata(scope, name) {
		const metaMap = this._attributeMap(scope);
		let meta = metaMap.get(name);
		if (meta) {
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
		this.markDenseEdgeAttributeDirty(destinationName);
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
					this.markDenseEdgeAttributeDirty(edgeName);
				}
				this._nodeAttributeDependents.delete(name);
			}
			this._denseNodeAttributeCache.delete(name);
		} else if (scope === 'edge') {
			if (this._nodeToEdgePassthrough.has(name)) {
				const entry = this._nodeToEdgePassthrough.get(name);
				this._unregisterNodeToEdgeDependency(entry.sourceName, name);
				this._nodeToEdgePassthrough.delete(name);
			}
			for (const dependents of this._nodeAttributeDependents.values()) {
				dependents.delete(name);
			}
			this._denseEdgeAttributeCache.delete(name);
		}
	}

	_isNumericType(type) {
		return type === AttributeType.Float
			|| type === AttributeType.Double
			|| type === AttributeType.Integer
			|| type === AttributeType.UnsignedInteger;
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
			this.markDenseEdgeAttributeDirty(edgeName);
			const entry = this._nodeToEdgePassthrough.get(edgeName);
			if (entry) {
				entry.dirty = true;
			}
		}
	}

	_markAllPassthroughEdgesDirty() {
		for (const edgeName of this._nodeToEdgePassthrough.keys()) {
			this.markDenseEdgeAttributeDirty(edgeName);
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

		const nodeActivity = this.nodeActivityView.slice();
		const edgeActivity = this.edgeActivityView.slice();
		const nodeRemap = this._buildRemap(nodeActivity);
		const edgeRemap = this._buildRemap(edgeActivity);

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
		return this;
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

	_buildRemap(activity) {
		const remap = new Map();
		let next = 0;
		for (let idx = 0; idx < activity.length; idx += 1) {
			if (activity[idx]) {
				remap.set(idx, next);
				next += 1;
			}
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
		try {
			if (kind === 'zxnet') {
				const level = clampCompressionLevel(options?.compressionLevel ?? 6);
				success = writeFn.call(module, this.ptr, cPath.ptr, level);
			} else {
				success = writeFn.call(module, this.ptr, cPath.ptr);
			}
		} finally {
			cPath.dispose();
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

export { AttributeType, NodeSelector, EdgeSelector, getModule as getHeliosModule };
export default HeliosNetwork;
