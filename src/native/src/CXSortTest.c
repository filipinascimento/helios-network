#include "CXSortTest.h"

void CXTestSortIntegers(CXInteger* data, CXSize count, CXComparisonResult order){
	CXIntegerArray array = { .data = data, .count = count, ._capacity = count };
	CXIntegerArraySort(&array, order);
}

void CXTestSortUIntegers(CXUInteger* data, CXSize count, CXComparisonResult order){
	CXUIntegerArray array = { .data = data, .count = count, ._capacity = count };
	CXUIntegerArraySort(&array, order);
}

void CXTestSortFloats(CXFloat* data, CXSize count, CXComparisonResult order){
	CXFloatArray array = { .data = data, .count = count, ._capacity = count };
	CXFloatArraySort(&array, order);
}

void CXTestSortDoubles(CXDouble* data, CXSize count, CXComparisonResult order){
	CXDoubleArray array = { .data = data, .count = count, ._capacity = count };
	CXDoubleArraySort(&array, order);
}

void CXTestSortFloatsWithIndices(CXFloat* data, CXUInteger* indices, CXSize count){
	CXFloatArray array = { .data = data, .count = count, ._capacity = count };
	CXUIntegerArray indexArray = { .data = indices, .count = count, ._capacity = count };
	CXFloatArraySortWithIndices(&array, &indexArray, CXOrderedAscending);
}

void CXTestSortDoublesWithIndices(CXDouble* data, CXUInteger* indices, CXSize count){
	CXDoubleArray array = { .data = data, .count = count, ._capacity = count };
	CXUIntegerArray indexArray = { .data = indices, .count = count, ._capacity = count };
	CXDoubleArraySortWithIndices(&array, &indexArray, CXOrderedAscending);
}

void CXTestSortIndicesWithFloats(CXInteger* indices, CXFloat* data, CXSize count){
	CXIntegerArray indexArray = { .data = indices, .count = count, ._capacity = count };
	CXFloatArray valueArray = { .data = data, .count = count, ._capacity = count };
	CXIntegerArraySortWithFloat(&indexArray, &valueArray, CXOrderedAscending);
}

void CXTestSortIndicesWithDoubles(CXInteger* indices, CXDouble* data, CXSize count){
	CXIntegerArray indexArray = { .data = indices, .count = count, ._capacity = count };
	CXDoubleArray valueArray = { .data = data, .count = count, ._capacity = count };
	CXIntegerArraySortWithDouble(&indexArray, &valueArray, CXOrderedAscending);
}
