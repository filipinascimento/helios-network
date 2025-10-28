import createHeliosModule from '../../compiled/CXNetwork.mjs';

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

/**
 * Projects attribute values for the provided selection scope into JavaScript data structures.
 *
 * @param {HeliosNetwork} network - Owning network instance.
 * @param {'node'|'edge'} scope - Attribute scope to read.
 * @param {string} name - Attribute identifier.
 * @param {Uint32Array} indices - Selection indices.
 * @param {{raw?:boolean}} [options] - Projection options.
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
		return Boolean(this.network?._nodeAttributes?.has(name));
	}

	/**
	 * Resolves attribute values for the selected nodes.
	 *
	 * @param {string} name - Attribute identifier.
	 * @param {{raw?:boolean}} [options] - Projection options.
	 * @returns {Array|TypedArray} Attribute values aligned with the selector indices.
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
	 * @param {{mode?:'out'|'in'|'both'|'all',includeEdges?:boolean,asSelector?:boolean}} [options] - Query options.
	 * @returns {Uint32Array|{nodes:Uint32Array|NodeSelector,edges?:Uint32Array|EdgeSelector}} Neighbor information.
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
	 * @param {{mode?:'out'|'in'|'both'|'all'}} [options] - Degree configuration.
	 * @returns {number[]} Degree values aligned with the selector order.
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
	 * @param {{mode?:'out'|'in'|'both'|'all',asSelector?:boolean}} [options] - Incident edge options.
	 * @returns {Uint32Array|EdgeSelector} Edge indices or selector proxy.
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
	 * @param {{raw?:boolean}} [options] - Projection options.
	 * @returns {Array|TypedArray} Attribute values aligned with the selector indices.
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
	 * @param {{asSelector?:boolean,unique?:boolean}} [options] - Projection options.
	 * @returns {Uint32Array|NodeSelector} Source node indices or selector proxy.
	 */
	sources(options = {}) {
		const { asSelector = false, unique = false } = options;
		return this._projectEndpoint(0, { asSelector, unique });
	}

	/**
	 * Retrieves the target node for each selected edge.
	 *
	 * @param {{asSelector?:boolean,unique?:boolean}} [options] - Projection options.
	 * @returns {Uint32Array|NodeSelector} Target node indices or selector proxy.
	 */
	targets(options = {}) {
		const { asSelector = false, unique = false } = options;
		return this._projectEndpoint(1, { asSelector, unique });
	}

	/**
	 * Returns the [source, target] pairs for the selection.
	 *
	 * @returns {Array<[number, number]>} Edge endpoint tuples.
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
	 * @param {{asSelector?:boolean}} [options] - Projection options.
	 * @returns {Uint32Array|NodeSelector} Node indices or selector proxy.
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
	 * @param {{asSelector?:boolean,unique?:boolean}} options - Projection options.
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
	 * Adds nodes to the network, returning their indices.
	 *
	 * @param {number} count - Number of nodes to add.
	 * @returns {Uint32Array} Copy of the inserted node indices.
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
		return indices;
	}

	/**
	 * Removes nodes from the network and clears related attributes.
	 *
	 * @param {Iterable<number>|Uint32Array} indices - Node indices slated for removal.
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
		return indices;
	}

	/**
	 * Removes edges from the network and clears related attributes.
	 *
	 * @param {Iterable<number>|Uint32Array} indices - Edge indices slated for removal.
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
	}

	/**
	 * Returns the outgoing neighbors of the provided node.
	 *
	 * @param {number} node - Source node index.
	 * @returns {{nodes: Uint32Array, edges: Uint32Array}} Neighbor node and edge indices.
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
	 */
	defineNetworkAttribute(name, type, dimension = 1) {
		this._defineAttribute('network', name, type, dimension, this.module._CXNetworkDefineNetworkAttribute);
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
	 */
	getNodeAttributeBuffer(name) {
		return this._getAttributeBuffer('node', name);
	}

	/**
	 * Retrieves a wrapper around the edge attribute buffer.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {object} Wrapper providing type information and buffer helpers.
	 */
	getEdgeAttributeBuffer(name) {
		return this._getAttributeBuffer('edge', name);
	}

	/**
	 * Retrieves a wrapper around the network attribute buffer.
	 *
	 * @param {string} name - Attribute identifier.
	 * @returns {object} Wrapper providing type information and buffer helpers.
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
		const meta = this._attributeMap(scope).get(name);
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
		const meta = this._attributeMap(scope).get(name);
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
		const meta = this._attributeMap(scope).get(name);
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
	 * Internal helper that creates a network backed by the provided module.
	 * @private
	 */
	static _createWithModule(module, directed, initialNodes, initialEdges) {
		const networkPtr = module._CXNewNetworkWithCapacity(directed ? 1 : 0, initialNodes, initialEdges);
		if (!networkPtr) {
			throw new Error('Failed to allocate Helios network');
		}
		const instance = new HeliosNetwork(module, networkPtr, directed);
		if (initialNodes > 0) {
			instance.addNodes(initialNodes);
		}
		return instance;
	}
}

export { AttributeType, NodeSelector, EdgeSelector, getModule as getHeliosModule };
export default HeliosNetwork;
