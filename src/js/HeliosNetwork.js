import createHeliosModule from '../../compiled/CXNetwork.mjs';

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

let modulePromise = null;

async function getModule(options = {}) {
	if (!modulePromise) {
		modulePromise = (async () => {
			const instance = await createHeliosModule(options);
			if (instance.ready) {
				await instance.ready;
			}
			return instance;
		})();
	}
	return modulePromise;
}

const COMPLEX_ATTRIBUTE_TYPES = new Set([AttributeType.Data, AttributeType.Javascript]);
const POINTER_ATTRIBUTE_TYPES = new Set([AttributeType.String, AttributeType.Data, AttributeType.Javascript]);

const TypedArrayForType = {
	[AttributeType.Boolean]: Uint8Array,
	[AttributeType.Float]: Float32Array,
	[AttributeType.Double]: Float64Array,
	[AttributeType.Integer]: BigInt64Array,
	[AttributeType.UnsignedInteger]: BigUint64Array,
	[AttributeType.Category]: Uint32Array,
};

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

class CString {
	constructor(module, value) {
		this.module = module;
		this.ptr = module._malloc(module.lengthBytesUTF8(value) + 1);
		if (!this.ptr) {
			throw new Error('Failed to allocate string memory');
		}
		module.stringToUTF8(value, this.ptr, module.lengthBytesUTF8(value) + 1);
	}

	dispose() {
		if (this.ptr) {
			this.module._free(this.ptr);
			this.ptr = 0;
		}
	}
}

class Selector {
	constructor(module, ptr, fns) {
		this.module = module;
		this.ptr = ptr;
		this._destroyFn = fns.destroyFn;
		this._countFn = fns.countFn;
		this._dataFn = fns.dataFn;
	}

	get count() {
		return this._countFn(this.ptr);
	}

	get dataPointer() {
		return this._dataFn(this.ptr);
	}

	toTypedArray() {
		const count = this.count;
		const ptr = this.dataPointer;
		if (!ptr || count === 0) {
			return new Uint32Array();
		}
		return new Uint32Array(this.module.HEAPU32.buffer, ptr, count).slice();
	}

	dispose() {
		if (this.ptr && this._destroyFn) {
			this._destroyFn(this.ptr);
			this.ptr = 0;
		}
	}
}

class NodeSelector extends Selector {
	static create(module) {
		const ptr = module._CXNodeSelectorCreate(0);
		if (!ptr) {
			throw new Error('Failed to create node selector');
		}
		return new NodeSelector(module, ptr);
	}

	constructor(module, ptr) {
		super(module, ptr, {
			destroyFn: module._CXNodeSelectorDestroy,
			countFn: module._CXNodeSelectorCount,
			dataFn: module._CXNodeSelectorData,
		});
	}

	fillAll(network) {
		this.module._CXNodeSelectorFillAll(this.ptr, network.ptr);
		return this;
	}

	fillFromArray(network, indices) {
		const array = Array.isArray(indices) ? Uint32Array.from(indices) : new Uint32Array(indices);
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
}

class EdgeSelector extends Selector {
	static create(module) {
		const ptr = module._CXEdgeSelectorCreate(0);
		if (!ptr) {
			throw new Error('Failed to create edge selector');
		}
		return new EdgeSelector(module, ptr);
	}

	constructor(module, ptr) {
		super(module, ptr, {
			destroyFn: module._CXEdgeSelectorDestroy,
			countFn: module._CXEdgeSelectorCount,
			dataFn: module._CXEdgeSelectorData,
		});
	}

	fillAll(network) {
		this.module._CXEdgeSelectorFillAll(this.ptr, network.ptr);
		return this;
	}

	fillFromArray(network, indices) {
		const array = Array.isArray(indices) ? Uint32Array.from(indices) : new Uint32Array(indices);
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
}

export class HeliosNetwork {
	static async create(options = {}) {
		const {
			directed = false,
			initialNodes = 0,
			initialEdges = 0,
			module: providedModule,
		} = options;
		const module = providedModule || await getModule();
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

	constructor(module, ptr, directed) {
		this.module = module;
		this.ptr = ptr;
		this.directed = directed;
		this._disposed = false;

		this._nodeAttributes = new Map();
		this._edgeAttributes = new Map();
		this._networkAttributes = new Map();
	}

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

	_ensureActive() {
		if (this._disposed || !this.ptr) {
			throw new Error('HeliosNetwork has been disposed');
		}
	}

	get nodeCount() {
		this._ensureActive();
		return this.module._CXNetworkNodeCount(this.ptr);
	}

	get edgeCount() {
		this._ensureActive();
		return this.module._CXNetworkEdgeCount(this.ptr);
	}

	get nodeCapacity() {
		this._ensureActive();
		return this.module._CXNetworkNodeCapacity(this.ptr);
	}

	get edgeCapacity() {
		this._ensureActive();
		return this.module._CXNetworkEdgeCapacity(this.ptr);
	}

	get nodeActivityView() {
		this._ensureActive();
		const ptr = this.module._CXNetworkNodeActivityBuffer(this.ptr);
		return new Uint8Array(this.module.HEAPU8.buffer, ptr, this.nodeCapacity);
	}

	get edgeActivityView() {
		this._ensureActive();
		const ptr = this.module._CXNetworkEdgeActivityBuffer(this.ptr);
		return new Uint8Array(this.module.HEAPU8.buffer, ptr, this.edgeCapacity);
	}

	get edgesView() {
		this._ensureActive();
		const ptr = this.module._CXNetworkEdgesBuffer(this.ptr);
		return new Uint32Array(this.module.HEAPU32.buffer, ptr, this.edgeCapacity * 2);
	}

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
	 * Add a batch of edges to the network.
	 *
	 * Supported input shapes:
	 *   - `Uint32Array` / typed array with flattened `[from0, to0, from1, to1, ...]`
	 *   - `number[]` flattened pairs (same layout as above)
	 *   - `Array<[number, number]>`
	 *   - `Array<{ from: number, to: number }>`
	 *
	 * @param {Uint32Array|number[]|Array<[number,number]>|Array<{from:number,to:number}>} edgeList
	 * @returns {Uint32Array} copy of the created edge indices
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

	getOutNeighbors(node) {
		this._ensureActive();
		const container = this.module._CXNetworkOutNeighbors(this.ptr, node);
		return this._readNeighborContainer(container);
	}

	getInNeighbors(node) {
		this._ensureActive();
		const container = this.module._CXNetworkInNeighbors(this.ptr, node);
		return this._readNeighborContainer(container);
	}

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

	defineNodeAttribute(name, type, dimension = 1) {
		this._defineAttribute('node', name, type, dimension, this.module._CXNetworkDefineNodeAttribute);
	}

	defineEdgeAttribute(name, type, dimension = 1) {
		this._defineAttribute('edge', name, type, dimension, this.module._CXNetworkDefineEdgeAttribute);
	}

	/**
	 * Define a graph-level attribute stored in linear memory.
	 *
	 * @param {string} name attribute identifier
	 * @param {AttributeType} type data category
	 * @param {number} [dimension=1] number of elements per entry (graph-level attributes always have capacity 1)
	 */
	defineNetworkAttribute(name, type, dimension = 1) {
		this._defineAttribute('network', name, type, dimension, this.module._CXNetworkDefineNetworkAttribute);
	}

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

	getNodeAttributeBuffer(name) {
		return this._getAttributeBuffer('node', name);
	}

	getEdgeAttributeBuffer(name) {
		return this._getAttributeBuffer('edge', name);
	}

	getNetworkAttributeBuffer(name) {
		return this._getAttributeBuffer('network', name);
	}

	setNodeStringAttribute(name, index, value) {
		this._setStringAttribute('node', name, index, value);
	}

	getNodeStringAttribute(name, index) {
		return this._getStringAttribute('node', name, index);
	}

	setEdgeStringAttribute(name, index, value) {
		this._setStringAttribute('edge', name, index, value);
	}

	getEdgeStringAttribute(name, index) {
		return this._getStringAttribute('edge', name, index);
	}

	setNetworkStringAttribute(name, value) {
		this._setStringAttribute('network', name, 0, value);
	}

	getNetworkStringAttribute(name) {
		return this._getStringAttribute('network', name, 0);
	}

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

	_deleteComplexAttribute(scope, meta, index, pointer) {
		const buffer = new Uint32Array(this.module.HEAPU32.buffer, pointer, this._capacityForScope(scope));
		meta.jsStore.delete(index);
		buffer[index] = 0;
	}

	_capacityForScope(scope) {
		if (scope === 'node') return this.nodeCapacity;
		if (scope === 'edge') return this.edgeCapacity;
		return 1;
	}

	_attributeMap(scope) {
		switch (scope) {
			case 'node': return this._nodeAttributes;
			case 'edge': return this._edgeAttributes;
			case 'network': return this._networkAttributes;
			default: throw new Error(`Unknown attribute scope "${scope}"`);
		}
	}

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

	createNodeSelector(indices) {
		const selector = NodeSelector.create(this.module);
		if (indices) {
			selector.fillFromArray(this, indices);
		} else {
			selector.fillAll(this);
		}
		return selector;
	}

	createEdgeSelector(indices) {
		const selector = EdgeSelector.create(this.module);
		if (indices) {
			selector.fillFromArray(this, indices);
		} else {
			selector.fillAll(this);
		}
		return selector;
	}
}

export { AttributeType, NodeSelector, EdgeSelector, getModule as getHeliosModule };
export default HeliosNetwork;
