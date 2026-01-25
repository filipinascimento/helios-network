import { expect, test } from 'vitest';
import { getHeliosModule } from '../src/helios-network.js';

const ORDER_ASC = -1;
const ORDER_DESC = 1;

function totalOrderFloat(a, b) {
	const aNaN = Number.isNaN(a);
	const bNaN = Number.isNaN(b);
	if (aNaN && bNaN) {
		return 0;
	}
	if (aNaN) {
		return 1;
	}
	if (bNaN) {
		return -1;
	}
	if (a < b) {
		return -1;
	}
	if (a > b) {
		return 1;
	}
	return 0;
}

function assertOrdered(values, order, compare) {
	for (let i = 1; i < values.length; i++) {
		const result = compare(values[i - 1], values[i]);
		if (order === ORDER_ASC) {
			expect(result).toBeLessThanOrEqual(0);
		} else {
			expect(result).toBeGreaterThanOrEqual(0);
		}
	}
}

test('wasm sorts integer arrays', async () => {
	const module = await getHeliosModule();
	const values = Int32Array.from([3, -2, 7, 7, 0, 1]);
	const count = values.length;
	const ptr = module._malloc(count * Int32Array.BYTES_PER_ELEMENT);
	const view = new Int32Array(module.HEAP32.buffer, ptr, count);
	view.set(values);
	module._CXTestSortIntegers(ptr, count, ORDER_ASC);
	assertOrdered(view, ORDER_ASC, (a, b) => a - b);
	module._free(ptr);
});

test('wasm sorts unsigned integer arrays', async () => {
	const module = await getHeliosModule();
	const values = Uint32Array.from([9, 3, 3, 0, 42]);
	const count = values.length;
	const ptr = module._malloc(count * Uint32Array.BYTES_PER_ELEMENT);
	const view = new Uint32Array(module.HEAPU32.buffer, ptr, count);
	view.set(values);
	module._CXTestSortUIntegers(ptr, count, ORDER_ASC);
	assertOrdered(view, ORDER_ASC, (a, b) => a - b);
	module._free(ptr);
});

test('wasm sorts float arrays with NaN ordering', async () => {
	const module = await getHeliosModule();
	const values = Float32Array.from([3.5, Number.NaN, -1, 3.5, 2]);
	const count = values.length;
	const ptr = module._malloc(count * Float32Array.BYTES_PER_ELEMENT);
	const view = new Float32Array(module.HEAPF32.buffer, ptr, count);
	view.set(values);
	module._CXTestSortFloats(ptr, count, ORDER_ASC);
	assertOrdered(view, ORDER_ASC, totalOrderFloat);
	module._free(ptr);
});

test('wasm sorts double arrays with NaN ordering', async () => {
	const module = await getHeliosModule();
	const values = Float64Array.from([3.5, Number.NaN, -1, 9, 2]);
	const count = values.length;
	const ptr = module._malloc(count * Float64Array.BYTES_PER_ELEMENT);
	const view = new Float64Array(module.HEAPF64.buffer, ptr, count);
	view.set(values);
	module._CXTestSortDoubles(ptr, count, ORDER_DESC);
	assertOrdered(view, ORDER_DESC, totalOrderFloat);
	module._free(ptr);
});

test('wasm sorts float arrays with indices', async () => {
	const module = await getHeliosModule();
	const values = Float32Array.from([4, 1, 3, 2]);
	const original = Array.from(values);
	const indices = Uint32Array.from([0, 1, 2, 3]);
	const count = values.length;
	const valuesPtr = module._malloc(count * Float32Array.BYTES_PER_ELEMENT);
	const indicesPtr = module._malloc(count * Uint32Array.BYTES_PER_ELEMENT);
	const valuesView = new Float32Array(module.HEAPF32.buffer, valuesPtr, count);
	const indicesView = new Uint32Array(module.HEAPU32.buffer, indicesPtr, count);
	valuesView.set(values);
	indicesView.set(indices);
	module._CXTestSortFloatsWithIndices(valuesPtr, indicesPtr, count);
	assertOrdered(valuesView, ORDER_ASC, totalOrderFloat);
	for (let i = 0; i < count; i++) {
		expect(valuesView[i]).toBe(original[indicesView[i]]);
	}
	module._free(valuesPtr);
	module._free(indicesPtr);
});

test('wasm sorts indices with float payloads', async () => {
	const module = await getHeliosModule();
	const indices = Int32Array.from([4, 2, 3, 1]);
	const originalIndices = Array.from(indices);
	const values = Float32Array.from([0.1, 0.2, 0.3, 0.4]);
	const originalValues = Array.from(values);
	const count = indices.length;
	const indicesPtr = module._malloc(count * Int32Array.BYTES_PER_ELEMENT);
	const valuesPtr = module._malloc(count * Float32Array.BYTES_PER_ELEMENT);
	const indicesView = new Int32Array(module.HEAP32.buffer, indicesPtr, count);
	const valuesView = new Float32Array(module.HEAPF32.buffer, valuesPtr, count);
	indicesView.set(indices);
	valuesView.set(values);
	module._CXTestSortIndicesWithFloats(indicesPtr, valuesPtr, count);
	assertOrdered(indicesView, ORDER_ASC, (a, b) => a - b);
	for (let i = 0; i < count; i++) {
		const idx = indicesView[i];
		const originalIndex = originalIndices.indexOf(idx);
		expect(valuesView[i]).toBe(originalValues[originalIndex]);
	}
	module._free(indicesPtr);
	module._free(valuesPtr);
});

test('wasm sorts double arrays with indices', async () => {
	const module = await getHeliosModule();
	const values = Float64Array.from([4, 1, 3, 2]);
	const original = Array.from(values);
	const indices = Uint32Array.from([0, 1, 2, 3]);
	const count = values.length;
	const valuesPtr = module._malloc(count * Float64Array.BYTES_PER_ELEMENT);
	const indicesPtr = module._malloc(count * Uint32Array.BYTES_PER_ELEMENT);
	const valuesView = new Float64Array(module.HEAPF64.buffer, valuesPtr, count);
	const indicesView = new Uint32Array(module.HEAPU32.buffer, indicesPtr, count);
	valuesView.set(values);
	indicesView.set(indices);
	module._CXTestSortDoublesWithIndices(valuesPtr, indicesPtr, count);
	assertOrdered(valuesView, ORDER_ASC, totalOrderFloat);
	for (let i = 0; i < count; i++) {
		expect(valuesView[i]).toBe(original[indicesView[i]]);
	}
	module._free(valuesPtr);
	module._free(indicesPtr);
});

test('wasm sorts indices with double payloads', async () => {
	const module = await getHeliosModule();
	const indices = Int32Array.from([4, 2, 3, 1]);
	const originalIndices = Array.from(indices);
	const values = Float64Array.from([0.1, 0.2, 0.3, 0.4]);
	const originalValues = Array.from(values);
	const count = indices.length;
	const indicesPtr = module._malloc(count * Int32Array.BYTES_PER_ELEMENT);
	const valuesPtr = module._malloc(count * Float64Array.BYTES_PER_ELEMENT);
	const indicesView = new Int32Array(module.HEAP32.buffer, indicesPtr, count);
	const valuesView = new Float64Array(module.HEAPF64.buffer, valuesPtr, count);
	indicesView.set(indices);
	valuesView.set(values);
	module._CXTestSortIndicesWithDoubles(indicesPtr, valuesPtr, count);
	assertOrdered(indicesView, ORDER_ASC, (a, b) => a - b);
	for (let i = 0; i < count; i++) {
		const idx = indicesView[i];
		const originalIndex = originalIndices.indexOf(idx);
		expect(valuesView[i]).toBe(originalValues[originalIndex]);
	}
	module._free(indicesPtr);
	module._free(valuesPtr);
});
