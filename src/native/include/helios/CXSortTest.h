#ifndef CXNetwork_CXSortTest_h
#define CXNetwork_CXSortTest_h

#include "CXBasicArrays.h"

CX_EXTERN void CXTestSortIntegers(CXInteger* data, CXSize count, CXComparisonResult order);
CX_EXTERN void CXTestSortUIntegers(CXUInteger* data, CXSize count, CXComparisonResult order);
CX_EXTERN void CXTestSortFloats(CXFloat* data, CXSize count, CXComparisonResult order);
CX_EXTERN void CXTestSortDoubles(CXDouble* data, CXSize count, CXComparisonResult order);
CX_EXTERN void CXTestSortFloatsWithIndices(CXFloat* data, CXUInteger* indices, CXSize count);
CX_EXTERN void CXTestSortDoublesWithIndices(CXDouble* data, CXUInteger* indices, CXSize count);
CX_EXTERN void CXTestSortIndicesWithFloats(CXInteger* indices, CXFloat* data, CXSize count);
CX_EXTERN void CXTestSortIndicesWithDoubles(CXInteger* indices, CXDouble* data, CXSize count);

#endif
