#include <assert.h>
#include <math.h>
#include "CXBasicArrays.h"

static void assert_integer_sorted(const CXInteger *values, CXSize count, CXComparisonResult order) {
	for (CXSize i = 1; i < count; i++) {
		assert(!CXIntegerArrayLess(values[i], values[i - 1], order));
	}
}

static void assert_uinteger_sorted(const CXUInteger *values, CXSize count, CXComparisonResult order) {
	for (CXSize i = 1; i < count; i++) {
		assert(!CXUIntegerArrayLess(values[i], values[i - 1], order));
	}
}

static void assert_float_sorted(const CXFloat *values, CXSize count, CXComparisonResult order) {
	for (CXSize i = 1; i < count; i++) {
		assert(!CXFloatArrayLess(values[i], values[i - 1], order));
	}
}

static void assert_double_sorted(const CXDouble *values, CXSize count, CXComparisonResult order) {
	for (CXSize i = 1; i < count; i++) {
		assert(!CXDoubleArrayLess(values[i], values[i - 1], order));
	}
}

static void assert_float_nan_order(const CXFloat *values, CXSize count, CXComparisonResult order) {
	CXBool seen_nan = CXFalse;
	CXBool seen_non_nan = CXFalse;
	for (CXSize i = 0; i < count; i++) {
		CXBool is_nan = isnan(values[i]);
		if (order == CXOrderedAscending) {
			if (is_nan) {
				seen_nan = CXTrue;
			} else {
				assert(!seen_nan);
			}
		} else if (order == CXOrderedDescending) {
			if (!is_nan) {
				seen_non_nan = CXTrue;
			} else {
				assert(!seen_non_nan);
			}
		}
	}
}

static void assert_double_nan_order(const CXDouble *values, CXSize count, CXComparisonResult order) {
	CXBool seen_nan = CXFalse;
	CXBool seen_non_nan = CXFalse;
	for (CXSize i = 0; i < count; i++) {
		CXBool is_nan = isnan(values[i]);
		if (order == CXOrderedAscending) {
			if (is_nan) {
				seen_nan = CXTrue;
			} else {
				assert(!seen_nan);
			}
		} else if (order == CXOrderedDescending) {
			if (!is_nan) {
				seen_non_nan = CXTrue;
			} else {
				assert(!seen_non_nan);
			}
		}
	}
}

static void test_integer_sorts(void) {
	CXInteger values[] = {3, -2, 7, 7, 0, 1};
	CXIntegerArray array = { .data = values, .count = 6, ._capacity = 6 };
	CXIntegerArraySort(&array, CXOrderedAscending);
	assert_integer_sorted(values, array.count, CXOrderedAscending);

	CXInteger values_desc[] = {4, -1, 9, 0, 2};
	CXIntegerArray array_desc = { .data = values_desc, .count = 5, ._capacity = 5 };
	CXIntegerArrayQuickSort3(&array_desc);
	assert_integer_sorted(values_desc, array_desc.count, CXOrderedAscending);

	CXInteger values_wrapper[] = {9, 1, 5, 3};
	CXIntegerArray array_wrapper = { .data = values_wrapper, .count = 4, ._capacity = 4 };
	CXIntegerArraySort(&array_wrapper, CXOrderedDescending);
	assert_integer_sorted(values_wrapper, array_wrapper.count, CXOrderedDescending);
}

static void test_uinteger_sorts(void) {
	CXUInteger values[] = {9, 3, 3, 0, 42};
	CXUIntegerArray array = { .data = values, .count = 5, ._capacity = 5 };
	CXUIntegerArraySort(&array, CXOrderedAscending);
	assert_uinteger_sorted(values, array.count, CXOrderedAscending);

	CXUInteger values_quick[] = {8, 1, 6, 2};
	CXUIntegerArray array_quick = { .data = values_quick, .count = 4, ._capacity = 4 };
	CXQuickSortUIntegerArray(array_quick);
	assert_uinteger_sorted(values_quick, array_quick.count, CXOrderedAscending);
}

static void test_float_sorts(void) {
	CXFloat values[] = {3.5f, NAN, -1.0f, 3.5f, 2.0f};
	CXFloatArray array = { .data = values, .count = 5, ._capacity = 5 };
	CXFloatArraySort(&array, CXOrderedAscending);
	assert_float_sorted(values, array.count, CXOrderedAscending);
	assert_float_nan_order(values, array.count, CXOrderedAscending);

	CXFloat values_desc[] = {NAN, 4.0f, -2.0f, 1.0f};
	CXFloatArray array_desc = { .data = values_desc, .count = 4, ._capacity = 4 };
	CXFloatArrayQuickSort3(&array_desc, CXOrderedDescending);
	assert_float_sorted(values_desc, array_desc.count, CXOrderedDescending);
	assert_float_nan_order(values_desc, array_desc.count, CXOrderedDescending);
}

static void test_double_sorts(void) {
	CXDouble values[] = {3.5, NAN, -1.0, 9.0, 2.0};
	CXDoubleArray array = { .data = values, .count = 5, ._capacity = 5 };
	CXDoubleArraySort(&array, CXOrderedAscending);
	assert_double_sorted(values, array.count, CXOrderedAscending);
	assert_double_nan_order(values, array.count, CXOrderedAscending);

	CXDouble values_desc[] = {NAN, 4.0, -2.0, 1.0};
	CXDoubleArray array_desc = { .data = values_desc, .count = 4, ._capacity = 4 };
	CXDoubleArrayQuickSort3(&array_desc, CXOrderedDescending);
	assert_double_sorted(values_desc, array_desc.count, CXOrderedDescending);
	assert_double_nan_order(values_desc, array_desc.count, CXOrderedDescending);
}

static void test_float_with_indices(void) {
	CXFloat values[] = {4.0f, 1.0f, 3.0f, 2.0f};
	CXFloat original[] = {4.0f, 1.0f, 3.0f, 2.0f};
	CXUInteger indices[] = {0, 1, 2, 3};
	CXFloatArray array = { .data = values, .count = 4, ._capacity = 4 };
	CXUIntegerArray indexArray = { .data = indices, .count = 4, ._capacity = 4 };
	CXQuickSortFloatArrayWithIndices(array, indexArray);
	assert_float_sorted(values, array.count, CXOrderedAscending);
	for (CXSize i = 0; i < array.count; i++) {
		assert(values[i] == original[indices[i]]);
	}
}

static void test_double_with_indices(void) {
	CXDouble values[] = {4.0, 1.0, 3.0, 2.0};
	CXDouble original[] = {4.0, 1.0, 3.0, 2.0};
	CXUInteger indices[] = {0, 1, 2, 3};
	CXDoubleArray array = { .data = values, .count = 4, ._capacity = 4 };
	CXUIntegerArray indexArray = { .data = indices, .count = 4, ._capacity = 4 };
	CXQuickSortDoubleArrayWithIndices(array, indexArray);
	assert_double_sorted(values, array.count, CXOrderedAscending);
	for (CXSize i = 0; i < array.count; i++) {
		assert(values[i] == original[indices[i]]);
	}
}

static void test_indices_with_float(void) {
	CXInteger indices[] = {4, 2, 3, 1};
	CXInteger original_indices[] = {4, 2, 3, 1};
	CXFloat values[] = {0.1f, 0.2f, 0.3f, 0.4f};
	CXFloat original_values[] = {0.1f, 0.2f, 0.3f, 0.4f};
	CXIntegerArray indexArray = { .data = indices, .count = 4, ._capacity = 4 };
	CXFloatArray valueArray = { .data = values, .count = 4, ._capacity = 4 };
	CXQuickSortIndicesArrayWithFloat(indexArray, valueArray);
	assert_integer_sorted(indices, indexArray.count, CXOrderedAscending);
	for (CXSize i = 0; i < indexArray.count; i++) {
		CXInteger idx = indices[i];
		CXFloat expected = 0.0f;
		for (CXSize j = 0; j < indexArray.count; j++) {
			if (original_indices[j] == idx) {
				expected = original_values[j];
				break;
			}
		}
		assert(values[i] == expected);
	}
}

static void test_indices_with_double(void) {
	CXInteger indices[] = {4, 2, 3, 1};
	CXInteger original_indices[] = {4, 2, 3, 1};
	CXDouble values[] = {0.1, 0.2, 0.3, 0.4};
	CXDouble original_values[] = {0.1, 0.2, 0.3, 0.4};
	CXIntegerArray indexArray = { .data = indices, .count = 4, ._capacity = 4 };
	CXDoubleArray valueArray = { .data = values, .count = 4, ._capacity = 4 };
	CXQuickSortIndicesArrayWithDouble(indexArray, valueArray);
	assert_integer_sorted(indices, indexArray.count, CXOrderedAscending);
	for (CXSize i = 0; i < indexArray.count; i++) {
		CXInteger idx = indices[i];
		CXDouble expected = 0.0;
		for (CXSize j = 0; j < indexArray.count; j++) {
			if (original_indices[j] == idx) {
				expected = original_values[j];
				break;
			}
		}
		assert(values[i] == expected);
	}
}

static void test_indices_only(void) {
	CXInteger values[] = {5, 2, 7, 1};
	CXIntegerArray array = { .data = values, .count = 4, ._capacity = 4 };
	CXQuickSortIndicesArray(array);
	assert_integer_sorted(values, array.count, CXOrderedAscending);
}

static void sort_strings_natural(CXString *values, CXSize count) {
	for (CXSize i = 1; i < count; i++) {
		CXString value = values[i];
		CXSize j = i;
		while (j > 0 && CXStringCompareNatural(values[j - 1], value) > 0) {
			values[j] = values[j - 1];
			j--;
		}
		values[j] = value;
	}
}

static void test_string_natural_compare(void) {
	assert(CXStringCompareNatural("file2", "file10") < 0);
	assert(CXStringCompareNatural("file02", "file2") > 0);
	assert(CXStringCompareNatural("file1", "file1") == 0);
	assert(CXStringCompareNatural("file10", "file2") > 0);

	CXString values[] = {
		"file10",
		"file2",
		"file1",
		"file02",
		"file20",
		"file3",
	};
	CXString expected[] = {
		"file1",
		"file2",
		"file02",
		"file3",
		"file10",
		"file20",
	};
	sort_strings_natural(values, sizeof(values) / sizeof(values[0]));
	for (CXSize i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		assert(strcmp(values[i], expected[i]) == 0);
	}
}

int main(void) {
	test_integer_sorts();
	test_uinteger_sorts();
	test_float_sorts();
	test_double_sorts();
	test_float_with_indices();
	test_double_with_indices();
	test_indices_with_float();
	test_indices_with_double();
	test_indices_only();
	test_string_natural_compare();
	return 0;
}
