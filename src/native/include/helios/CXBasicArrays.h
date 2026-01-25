//
//  CXBasicArrays.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/22/12.
//  Copyright (c) 2012 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXNetwork_CXBasicArrays_h
#define CXNetwork_CXBasicArrays_h

#include "CXCommons.h"

/** Plain-old-data container representing a dynamically sized float array. */
typedef struct{
	CXFloat* data;
	CXSize count;
	CXSize _capacity;
} CXFloatArray;

typedef CXFloatArray CXFloatStack;

/** Dynamic array of doubles with manual capacity tracking. */
typedef struct{
	CXDouble* data;
	CXSize count;
	CXSize _capacity;
} CXDoubleArray;

typedef CXDoubleArray CXDoubleStack;

/** Dynamic array of signed integers with manual capacity tracking. */
typedef struct{
	CXInteger* data;
	CXSize count;
	CXSize _capacity;
} CXIntegerArray;

typedef CXIntegerArray CXIntegerStack;

/** Specialized array used for OpenGL short values. */
typedef struct{
	CXInteger* data;
	CXSize count;
	CXSize _capacity;
} CXGLShortArray;

/** Dynamic array of unsigned integers with manual capacity tracking. */
typedef struct{
	CXUInteger* data;
	CXSize count;
	CXSize _capacity;
} CXUIntegerArray;



/** Dynamic array of pointers storing opaque payloads. */
typedef struct{
	void** data;
	CXSize count;
	CXSize _capacity;
} CXGenericArray;


typedef CXUIntegerArray CXUIntegerStack;
typedef CXGenericArray CXGenericStack;


/** Initializes the float array with the requested capacity (zeroed). */
CX_INLINE CXFloatArray* CXFloatArrayInitWithCapacity(CXSize capacity, CXFloatArray* theArray){
	theArray->count=0;
	theArray->_capacity=capacity;
	if (capacity==0) {
		theArray->data = NULL;
	}else{
		theArray->data = calloc(theArray->_capacity, sizeof(CXFloat));
	}
	return theArray;
}

/** Releases the memory owned by the float array without freeing the struct. */
CX_INLINE void CXFloatArrayDestroy(CXFloatArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

/** Grows or shrinks the backing storage to exactly `newCapacity`. */
CX_INLINE void CXFloatArrayReallocToCapacity(CXSize newCapacity, CXFloatArray* theArray){
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(CXFloat));
	}else{
		theArray->data = calloc(newCapacity, sizeof(CXFloat));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count)
		theArray->count = theArray->_capacity;
}

/** Appends an element to the end of the float array. */
CX_INLINE void CXFloatArrayAdd(CXFloat value, CXFloatArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXFloatArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

/** Resizes the logical count, expanding or shrinking the buffer as needed. */
CX_INLINE void CXFloatArraySetCount(CXUInteger count, CXFloatArray* theArray){
	if(theArray->_capacity < count){
		CXFloatArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXFloatArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

/** Creates a float stack with an initial capacity of one element. */
CX_INLINE CXFloatStack CXFloatStackMake(){
	CXFloatStack floatStack;
	CXFloatArrayInitWithCapacity(1, &floatStack);
	return floatStack;
}

/** Pops and returns the last pushed value. Returns 0 when empty. */
CX_INLINE CXFloat CXFloatStackPop(CXFloatStack* floatStack){
	if (floatStack->count>0){
		floatStack->count--;
		return floatStack->data[floatStack->count];
	}else{
		return 0.0f;
	}
}

/** Pushes a value onto the stack, growing storage as needed. */
CX_INLINE void CXFloatStackPush(CXFloat value, CXFloatStack* floatStack){
	CXFloatArrayAdd(value, floatStack);
}

/** Returns the value on the top of the stack without removing it. */
CX_INLINE CXFloat CXFloatStackTop(CXFloatStack* floatStack){
	if (floatStack->count>0){
		return floatStack->data[floatStack->count-1];
	}else{
		return 0.0f;
	}
}

/** Returns CXTrue when the stack contains no elements. */
CX_INLINE CXBool CXFloatStackIsEmpty(CXFloatStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

/** Creates a deep copy of the provided float array. */
CX_INLINE CXFloatArray CXFloatArrayCopy(CXFloatArray* theArray){
	CXFloatArray newArray;
	CXFloatArrayInitWithCapacity(theArray->count, &newArray);
	CXFloatArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXFloat)*theArray->count);
	return newArray;
}


/** Initializes the double array with the requested capacity (zeroed). */
CX_INLINE CXDoubleArray* CXDoubleArrayInitWithCapacity(CXSize capacity, CXDoubleArray* theArray){
	theArray->count=0;
	theArray->_capacity=capacity;
	if (capacity==0) {
		theArray->data = NULL;
	}else{
		theArray->data = calloc(theArray->_capacity, sizeof(CXDouble));
	}
	return theArray;
}

/** Releases the memory owned by the double array without freeing the struct. */
CX_INLINE void CXDoubleArrayDestroy(CXDoubleArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

/** Adjusts the backing storage to exactly `newCapacity`. */
CX_INLINE void CXDoubleArrayReallocToCapacity(CXSize newCapacity, CXDoubleArray* theArray){
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(CXDouble));
	}else{
		theArray->data = calloc(newCapacity, sizeof(CXDouble));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count)
		theArray->count = theArray->_capacity;
}

/** Appends a value to the end of the double array. */
CX_INLINE void CXDoubleArrayAdd(CXDouble value, CXDoubleArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXDoubleArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

/** Resizes the logical count, expanding or shrinking the buffer as needed. */
CX_INLINE void CXDoubleArraySetCount(CXUInteger count, CXDoubleArray* theArray){
	if(theArray->_capacity < count){
		CXDoubleArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXDoubleArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

/** Creates a double stack with an initial capacity of one element. */
CX_INLINE CXDoubleStack CXDoubleStackMake(){
	CXDoubleStack floatStack;
	CXDoubleArrayInitWithCapacity(1, &floatStack);
	return floatStack;
}

/** Pops and returns the last pushed double. Returns 0.0 when empty. */
CX_INLINE CXDouble CXDoubleStackPop(CXDoubleStack* floatStack){
	if (floatStack->count>0){
		floatStack->count--;
		return floatStack->data[floatStack->count];
	}else{
		return 0.0;
	}
}

/** Pushes a double onto the stack, growing storage as needed. */
CX_INLINE void CXDoubleStackPush(CXDouble value, CXDoubleStack* floatStack){
	CXDoubleArrayAdd(value, floatStack);
}

/** Returns the double at the top of the stack without removing it. */
CX_INLINE CXDouble CXDoubleStackTop(CXDoubleStack* floatStack){
	if (floatStack->count>0){
		return floatStack->data[floatStack->count-1];
	}else{
		return 0.0;
	}
}

/** Returns CXTrue when the double stack contains no elements. */
CX_INLINE CXBool CXDoubleStackIsEmpty(CXDoubleStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

/** Creates a deep copy of the provided double array. */
/** Creates a deep copy of the provided double array. */
CX_INLINE CXDoubleArray CXDoubleArrayCopy(CXDoubleArray* theArray){
	CXDoubleArray newArray;
	CXDoubleArrayInitWithCapacity(theArray->count, &newArray);
	CXDoubleArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXDouble)*theArray->count);
	return newArray;
}



/** Initializes the integer array with the requested capacity (zeroed). */
CX_INLINE CXIntegerArray* CXIntegerArrayInitWithCapacity(CXUInteger capacity, CXIntegerArray* theArray){
	theArray->count=0;
	theArray->_capacity=capacity;
	if (capacity==0) {
		theArray->data = NULL;
	}else{
		theArray->data = calloc(theArray->_capacity, sizeof(CXInteger));
	}
	return theArray;
}

/** Releases the memory owned by the integer array without freeing the struct. */
CX_INLINE void CXIntegerArrayDestroy(CXIntegerArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

/** Adjusts the backing storage for the integer array. */
CX_INLINE void CXIntegerArrayReallocToCapacity(CXUInteger newCapacity, CXIntegerArray* theArray){
	if(theArray->_capacity==newCapacity){
		return;
	}
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(CXInteger));
	}else{
		theArray->data = calloc(newCapacity, sizeof(CXInteger));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count) theArray->count = theArray->_capacity;
}

/** Resizes the logical item count, keeping the buffer balanced. */
CX_INLINE void CXIntegerArraySetCount(CXUInteger count, CXIntegerArray* theArray){
	if(theArray->_capacity < count){
		CXIntegerArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXIntegerArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}


/** Appends a value to the end of the integer array. */
CX_INLINE void CXIntegerArrayAdd(CXInteger value, CXIntegerArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXIntegerArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

/** Creates an integer stack with an initial capacity of one element. */
CX_INLINE CXIntegerStack CXIntegerStackMake(){
	CXIntegerStack stack;
	CXIntegerArrayInitWithCapacity(1, &stack);
	return stack;
}

/** Pops and returns the last pushed integer. Returns 0 when empty. */
CX_INLINE CXInteger CXIntegerStackPop(CXIntegerStack* stack){
	if(stack->count>0){
		stack->count--;
		return stack->data[stack->count];
	}else{
		return 0;
	}
}

/** Pushes an integer onto the stack, growing storage as needed. */
CX_INLINE void CXIntegerStackPush(CXInteger value, CXIntegerStack* stack){
	CXIntegerArrayAdd(value, stack);
}

/** Returns the integer at the top of the stack without removing it. */
CX_INLINE CXInteger CXIntegerStackTop(CXIntegerStack* stack){
	if(stack->count>0){
		return stack->data[stack->count-1];
	}else{
		return 0;
	}
}

/** Returns CXTrue when the integer stack contains no elements. */
CX_INLINE CXBool CXIntegerStackIsEmpty(CXIntegerStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

/** Creates a deep copy of the provided integer array. */
CX_INLINE CXIntegerArray CXIntegerArrayCopy(CXIntegerArray* theArray){
	CXIntegerArray newArray;
	CXIntegerArrayInitWithCapacity(theArray->count, &newArray);
	CXIntegerArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXInteger)*theArray->count);
	return newArray;
}



/** Initializes the unsigned integer array with the requested capacity. */
CX_INLINE CXUIntegerArray* CXUIntegerArrayInitWithCapacity(CXUInteger capacity, CXUIntegerArray* theArray){
	theArray->count=0;
	theArray->_capacity=capacity;
	if(capacity==0) {
		theArray->data = NULL;
	}else{
		theArray->data = calloc(theArray->_capacity, sizeof(CXUInteger));
	}
	return theArray;
}

/** Releases the memory owned by the unsigned integer array. */
CX_INLINE void CXUIntegerArrayDestroy(CXUIntegerArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

/** Adjusts the backing storage for the unsigned integer array. */
CX_INLINE void CXUIntegerArrayReallocToCapacity(CXUInteger newCapacity, CXUIntegerArray* theArray){
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(CXUInteger));
	}else{
		theArray->data = calloc(newCapacity, sizeof(CXUInteger));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count) theArray->count = theArray->_capacity;
}

/** Appends a value to the end of the unsigned integer array. */
CX_INLINE void CXUIntegerArrayAdd(CXUInteger value, CXUIntegerArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXUIntegerArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}


/** Resizes the logical count for the unsigned integer array. */
CX_INLINE void CXUIntegerArraySetCount(CXSize count, CXUIntegerArray* theArray){
	if(theArray->_capacity < count){
		CXUIntegerArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXUIntegerArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

/** Creates an unsigned integer stack with an initial capacity of one element. */
CX_INLINE CXUIntegerStack CXUIntegerStackMake(){
	CXUIntegerStack stack;
	CXUIntegerArrayInitWithCapacity(1, &stack);
	return stack;
}

/** Pops and returns the last pushed unsigned integer. Returns 0 when empty. */
CX_INLINE CXUInteger CXUIntegerStackPop(CXUIntegerStack* stack){
	if(stack->count>0){
		stack->count--;
		return stack->data[stack->count];
	}else{
		return 0;
	}
}

/** Pushes an unsigned integer onto the stack, growing storage as needed. */
CX_INLINE void CXUIntegerStackPush(CXUInteger value, CXUIntegerStack* stack){
	CXUIntegerArrayAdd(value, stack);
}

/** Returns the unsigned integer at the top of the stack without removing it. */
CX_INLINE CXUInteger CXUIntegerStackTop(CXUIntegerStack* stack){
	if(stack->count>0){
		return stack->data[stack->count-1];
	}else{
		return 0;
	}
}

/** Returns CXTrue when the unsigned integer stack contains no elements. */
CX_INLINE CXBool CXUIntegerStackIsEmpty(CXUIntegerStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

/** Creates a deep copy of the provided unsigned integer array. */
CX_INLINE CXUIntegerArray CXUIntegerArrayCopy(CXUIntegerArray* theArray){
	CXUIntegerArray newArray;
	CXUIntegerArrayInitWithCapacity(theArray->count, &newArray);
	CXUIntegerArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXUInteger)*theArray->count);
	return newArray;
}


/** Initializes the generic pointer array with the requested capacity. */
CX_INLINE CXGenericArray* CXGenericArrayInitWithCapacity(CXUInteger capacity, CXGenericArray* theArray){
	theArray->count=0;
	theArray->_capacity=capacity;
	if(capacity==0) {
		theArray->data = NULL;
	}else{
		theArray->data = calloc(theArray->_capacity, sizeof(void*));
	}
	return theArray;
}

/** Releases the memory owned by the generic array without freeing elements. */
CX_INLINE void CXGenericArrayDestroy(CXGenericArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}



/** Adjusts the backing storage for the generic array. */
CX_INLINE void CXGenericArrayReallocToCapacity(CXUInteger newCapacity, CXGenericArray* theArray){
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(void*));
	}else{
		theArray->data = calloc(newCapacity, sizeof(void*));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count) theArray->count = theArray->_capacity;
}

/** Appends a pointer value to the generic array. */
CX_INLINE void CXGenericArrayAdd(void* value, CXGenericArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXGenericArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

/** Resizes the logical count for the generic array. */
CX_INLINE void CXGenericArraySetCount(CXUInteger count, CXGenericArray* theArray){
	if(theArray->_capacity < count){
		CXGenericArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXGenericArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

/** Creates a generic pointer stack with an initial capacity of one element. */
CX_INLINE CXGenericStack CXGenericStackMake(){
	CXGenericStack stack;
	CXGenericArrayInitWithCapacity(1, &stack);
	return stack;
}

/** Pops and returns the last pushed pointer. Returns NULL when empty. */
CX_INLINE void* CXGenericStackPop(CXGenericStack* stack){
	if(stack->count>0){
		stack->count--;
		return stack->data[stack->count];
	}else{
		return 0;
	}
}

/** Pushes a pointer onto the stack, growing storage as needed. */
CX_INLINE void CXGenericStackPush(void* value, CXGenericStack* stack){
	CXGenericArrayAdd(value, stack);
}

/** Returns the pointer at the top of the stack without removing it. */
CX_INLINE void* CXGenericStackTop(CXGenericStack* stack){
	if(stack->count>0){
		return stack->data[stack->count-1];
	}else{
		return 0;
	}
}

/** Returns CXTrue when the generic stack contains no elements. */
CX_INLINE CXBool CXGenericStackIsEmpty(CXGenericStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}


/** Creates a shallow copy of the provided generic array. */
CX_INLINE CXGenericArray CXGenericArrayCopy(CXGenericArray* theArray){
	CXGenericArray newArray;
	CXGenericArrayInitWithCapacity(theArray->count, &newArray);
	CXGenericArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(void*)*theArray->count);
	return newArray;
}

enum {
	CXOrderedAscending = -1,
	CXOrderedSame,
	CXOrderedDescending
};
typedef CXInteger CXComparisonResult;

#if !defined(CX_ARRAY_COMPARE_FUNCTION)
#define CX_ARRAY_COMPARE_FUNCTION(leftValue, rightValue) (((leftValue)>(rightValue))?CXOrderedDescending:(((leftValue)<(rightValue))?CXOrderedAscending:CXOrderedSame))
#endif

#if !defined(CX_INTROSORT_INSERTION_THRESHOLD)
#define CX_INTROSORT_INSERTION_THRESHOLD 24
#endif

CX_STATIC_INLINE CXUInteger CXFloorLog2(CXUInteger value){
	CXUInteger log2 = 0;
	while(value > 1){
		value >>= 1;
		log2++;
	}
	return log2;
}

CX_STATIC_INLINE CXComparisonResult CXFloatCompareTotalOrder(CXFloat leftValue, CXFloat rightValue){
	if(isnan(leftValue)){
		return isnan(rightValue) ? CXOrderedSame : CXOrderedDescending;
	}
	if(isnan(rightValue)){
		return CXOrderedAscending;
	}
	return CX_ARRAY_COMPARE_FUNCTION(leftValue, rightValue);
}

CX_STATIC_INLINE CXComparisonResult CXDoubleCompareTotalOrder(CXDouble leftValue, CXDouble rightValue){
	if(isnan(leftValue)){
		return isnan(rightValue) ? CXOrderedSame : CXOrderedDescending;
	}
	if(isnan(rightValue)){
		return CXOrderedAscending;
	}
	return CX_ARRAY_COMPARE_FUNCTION(leftValue, rightValue);
}

CX_STATIC_INLINE CXBool CXIntegerArrayLess(CXInteger leftValue, CXInteger rightValue, CXComparisonResult order){
	return CX_ARRAY_COMPARE_FUNCTION(leftValue, rightValue) == order;
}

CX_STATIC_INLINE CXBool CXUIntegerArrayLess(CXUInteger leftValue, CXUInteger rightValue, CXComparisonResult order){
	return CX_ARRAY_COMPARE_FUNCTION(leftValue, rightValue) == order;
}

CX_STATIC_INLINE CXBool CXFloatArrayLess(CXFloat leftValue, CXFloat rightValue, CXComparisonResult order){
	return CXFloatCompareTotalOrder(leftValue, rightValue) == order;
}

CX_STATIC_INLINE CXBool CXDoubleArrayLess(CXDouble leftValue, CXDouble rightValue, CXComparisonResult order){
	return CXDoubleCompareTotalOrder(leftValue, rightValue) == order;
}

CX_INLINE void CXIntegerArraySort(CXIntegerArray* theArray, CXComparisonResult order);
CX_INLINE void CXUIntegerArraySort(CXUIntegerArray* theArray, CXComparisonResult order);
CX_INLINE void CXFloatArraySort(CXFloatArray* theArray, CXComparisonResult order);
CX_INLINE void CXDoubleArraySort(CXDoubleArray* theArray, CXComparisonResult order);

CX_STATIC_INLINE void CXFloatArraySwapWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXUInteger left, CXUInteger right){
	CXFloat tmpValue = floatArray->data[left];
	CXUInteger tmpIndex = indicesArray->data[left];
	floatArray->data[left] = floatArray->data[right];
	indicesArray->data[left] = indicesArray->data[right];
	floatArray->data[right] = tmpValue;
	indicesArray->data[right] = tmpIndex;
}

CX_STATIC_INLINE CXFloat CXFloatArrayMedianOfThree(CXFloat leftValue, CXFloat middleValue, CXFloat rightValue, CXComparisonResult order){
	if(CXFloatArrayLess(leftValue, middleValue, order)){
		if(CXFloatArrayLess(middleValue, rightValue, order)){
			return middleValue;
		}
		if(CXFloatArrayLess(leftValue, rightValue, order)){
			return rightValue;
		}
		return leftValue;
	}
	if(CXFloatArrayLess(leftValue, rightValue, order)){
		return leftValue;
	}
	if(CXFloatArrayLess(middleValue, rightValue, order)){
		return rightValue;
	}
	return middleValue;
}

CX_STATIC_INLINE void CXFloatArrayPartition3RangeWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXFloat pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXFloat* values = floatArray->data;
	while(i <= gt){
		if(CXFloatArrayLess(values[i], pivot, order)){
			CXFloatArraySwapWithIndices(floatArray, indicesArray, (CXUInteger)lt, (CXUInteger)i);
			lt++;
			i++;
		}else if(CXFloatArrayLess(pivot, values[i], order)){
			CXFloatArraySwapWithIndices(floatArray, indicesArray, (CXUInteger)i, (CXUInteger)gt);
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXFloatArrayInsertionSortRangeWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXFloat* values = floatArray->data;
	CXUInteger* indices = indicesArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXFloat value = values[i];
		CXUInteger indexValue = indices[i];
		CXUInteger j = i;
		while(j > lo && CXFloatArrayLess(value, values[j - 1], order)){
			values[j] = values[j - 1];
			indices[j] = indices[j - 1];
			j--;
		}
		values[j] = value;
		indices[j] = indexValue;
	}
}

CX_STATIC_INLINE void CXFloatArraySiftDownWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXFloat* values = floatArray->data + base;
	CXUInteger* indices = indicesArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXFloatArrayLess(values[swapIndex], values[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXFloatArrayLess(values[swapIndex], values[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXFloat tmpValue = values[root];
		CXUInteger tmpIndex = indices[root];
		values[root] = values[swapIndex];
		indices[root] = indices[swapIndex];
		values[swapIndex] = tmpValue;
		indices[swapIndex] = tmpIndex;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXFloatArrayHeapSortRangeWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXFloatArraySiftDownWithIndices(floatArray, indicesArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXFloatArraySwapWithIndices(floatArray, indicesArray, lo, lo + end);
		end--;
		CXFloatArraySiftDownWithIndices(floatArray, indicesArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXFloatArrayIntroSortRangeWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXFloatArrayInsertionSortRangeWithIndices(floatArray, indicesArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXFloatArrayHeapSortRangeWithIndices(floatArray, indicesArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXFloat pivot = CXFloatArrayMedianOfThree(floatArray->data[lo], floatArray->data[mid], floatArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXFloatArrayPartition3RangeWithIndices(floatArray, indicesArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXFloatArrayIntroSortRangeWithIndices(floatArray, indicesArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXFloatArrayIntroSortRangeWithIndices(floatArray, indicesArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_STATIC_INLINE CXBool CXFloatArraySortWithIndices(CXFloatArray* floatArray, CXUIntegerArray* indicesArray, CXComparisonResult order){
	if(floatArray->count != indicesArray->count){
		return CXFalse;
	}
	if(floatArray->count < 2){
		return CXTrue;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(floatArray->count);
	CXFloatArrayIntroSortRangeWithIndices(floatArray, indicesArray, 0, floatArray->count - 1, order, depthLimit);
	return CXTrue;
}

/** Sorts a float array in ascending order while reordering the parallel index array. */
CX_INLINE CXBool CXQuickSortFloatArrayWithIndices(CXFloatArray floatArray, CXUIntegerArray indicesArray){
	return CXFloatArraySortWithIndices(&floatArray, &indicesArray, CXOrderedAscending);
}

CX_STATIC_INLINE void CXDoubleArraySwapWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXUInteger left, CXUInteger right){
	CXDouble tmpValue = doubleArray->data[left];
	CXUInteger tmpIndex = indicesArray->data[left];
	doubleArray->data[left] = doubleArray->data[right];
	indicesArray->data[left] = indicesArray->data[right];
	doubleArray->data[right] = tmpValue;
	indicesArray->data[right] = tmpIndex;
}

CX_STATIC_INLINE CXDouble CXDoubleArrayMedianOfThree(CXDouble leftValue, CXDouble middleValue, CXDouble rightValue, CXComparisonResult order){
	if(CXDoubleArrayLess(leftValue, middleValue, order)){
		if(CXDoubleArrayLess(middleValue, rightValue, order)){
			return middleValue;
		}
		if(CXDoubleArrayLess(leftValue, rightValue, order)){
			return rightValue;
		}
		return leftValue;
	}
	if(CXDoubleArrayLess(leftValue, rightValue, order)){
		return leftValue;
	}
	if(CXDoubleArrayLess(middleValue, rightValue, order)){
		return rightValue;
	}
	return middleValue;
}

CX_STATIC_INLINE void CXDoubleArrayPartition3RangeWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXDouble pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXDouble* values = doubleArray->data;
	while(i <= gt){
		if(CXDoubleArrayLess(values[i], pivot, order)){
			CXDoubleArraySwapWithIndices(doubleArray, indicesArray, (CXUInteger)lt, (CXUInteger)i);
			lt++;
			i++;
		}else if(CXDoubleArrayLess(pivot, values[i], order)){
			CXDoubleArraySwapWithIndices(doubleArray, indicesArray, (CXUInteger)i, (CXUInteger)gt);
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXDoubleArrayInsertionSortRangeWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXDouble* values = doubleArray->data;
	CXUInteger* indices = indicesArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXDouble value = values[i];
		CXUInteger indexValue = indices[i];
		CXUInteger j = i;
		while(j > lo && CXDoubleArrayLess(value, values[j - 1], order)){
			values[j] = values[j - 1];
			indices[j] = indices[j - 1];
			j--;
		}
		values[j] = value;
		indices[j] = indexValue;
	}
}

CX_STATIC_INLINE void CXDoubleArraySiftDownWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXDouble* values = doubleArray->data + base;
	CXUInteger* indices = indicesArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXDoubleArrayLess(values[swapIndex], values[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXDoubleArrayLess(values[swapIndex], values[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXDouble tmpValue = values[root];
		CXUInteger tmpIndex = indices[root];
		values[root] = values[swapIndex];
		indices[root] = indices[swapIndex];
		values[swapIndex] = tmpValue;
		indices[swapIndex] = tmpIndex;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXDoubleArrayHeapSortRangeWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXDoubleArraySiftDownWithIndices(doubleArray, indicesArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXDoubleArraySwapWithIndices(doubleArray, indicesArray, lo, lo + end);
		end--;
		CXDoubleArraySiftDownWithIndices(doubleArray, indicesArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXDoubleArrayIntroSortRangeWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXDoubleArrayInsertionSortRangeWithIndices(doubleArray, indicesArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXDoubleArrayHeapSortRangeWithIndices(doubleArray, indicesArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXDouble pivot = CXDoubleArrayMedianOfThree(doubleArray->data[lo], doubleArray->data[mid], doubleArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXDoubleArrayPartition3RangeWithIndices(doubleArray, indicesArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXDoubleArrayIntroSortRangeWithIndices(doubleArray, indicesArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXDoubleArrayIntroSortRangeWithIndices(doubleArray, indicesArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_STATIC_INLINE CXBool CXDoubleArraySortWithIndices(CXDoubleArray* doubleArray, CXUIntegerArray* indicesArray, CXComparisonResult order){
	if(doubleArray->count != indicesArray->count){
		return CXFalse;
	}
	if(doubleArray->count < 2){
		return CXTrue;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(doubleArray->count);
	CXDoubleArrayIntroSortRangeWithIndices(doubleArray, indicesArray, 0, doubleArray->count - 1, order, depthLimit);
	return CXTrue;
}

/** Sorts a double array in ascending order while reordering the parallel index array. */
CX_INLINE CXBool CXQuickSortDoubleArrayWithIndices(CXDoubleArray doubleArray, CXUIntegerArray indicesArray){
	return CXDoubleArraySortWithIndices(&doubleArray, &indicesArray, CXOrderedAscending);
}

CX_STATIC_INLINE void CXIntegerArraySwapWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXUInteger left, CXUInteger right){
	CXInteger tmpIndex = indicesArray->data[left];
	CXFloat tmpValue = floatArray->data[left];
	indicesArray->data[left] = indicesArray->data[right];
	floatArray->data[left] = floatArray->data[right];
	indicesArray->data[right] = tmpIndex;
	floatArray->data[right] = tmpValue;
}

CX_STATIC_INLINE CXInteger CXIntegerArrayMedianOfThree(CXInteger leftValue, CXInteger middleValue, CXInteger rightValue, CXComparisonResult order){
	if(CXIntegerArrayLess(leftValue, middleValue, order)){
		if(CXIntegerArrayLess(middleValue, rightValue, order)){
			return middleValue;
		}
		if(CXIntegerArrayLess(leftValue, rightValue, order)){
			return rightValue;
		}
		return leftValue;
	}
	if(CXIntegerArrayLess(leftValue, rightValue, order)){
		return leftValue;
	}
	if(CXIntegerArrayLess(middleValue, rightValue, order)){
		return rightValue;
	}
	return middleValue;
}

CX_STATIC_INLINE void CXIntegerArrayPartition3RangeWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXUInteger lo, CXUInteger hi, CXInteger pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXInteger* indices = indicesArray->data;
	while(i <= gt){
		if(CXIntegerArrayLess(indices[i], pivot, order)){
			CXIntegerArraySwapWithFloat(indicesArray, floatArray, (CXUInteger)lt, (CXUInteger)i);
			lt++;
			i++;
		}else if(CXIntegerArrayLess(pivot, indices[i], order)){
			CXIntegerArraySwapWithFloat(indicesArray, floatArray, (CXUInteger)i, (CXUInteger)gt);
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXIntegerArrayInsertionSortRangeWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXInteger* indices = indicesArray->data;
	CXFloat* values = floatArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXInteger indexValue = indices[i];
		CXFloat floatValue = values[i];
		CXUInteger j = i;
		while(j > lo && CXIntegerArrayLess(indexValue, indices[j - 1], order)){
			indices[j] = indices[j - 1];
			values[j] = values[j - 1];
			j--;
		}
		indices[j] = indexValue;
		values[j] = floatValue;
	}
}

CX_STATIC_INLINE void CXIntegerArraySiftDownWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXInteger* indices = indicesArray->data + base;
	CXFloat* values = floatArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXIntegerArrayLess(indices[swapIndex], indices[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXIntegerArrayLess(indices[swapIndex], indices[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXInteger tmpIndex = indices[root];
		CXFloat tmpValue = values[root];
		indices[root] = indices[swapIndex];
		values[root] = values[swapIndex];
		indices[swapIndex] = tmpIndex;
		values[swapIndex] = tmpValue;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXIntegerArrayHeapSortRangeWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXIntegerArraySiftDownWithFloat(indicesArray, floatArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXIntegerArraySwapWithFloat(indicesArray, floatArray, lo, lo + end);
		end--;
		CXIntegerArraySiftDownWithFloat(indicesArray, floatArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXIntegerArrayIntroSortRangeWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXIntegerArrayInsertionSortRangeWithFloat(indicesArray, floatArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXIntegerArrayHeapSortRangeWithFloat(indicesArray, floatArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXInteger pivot = CXIntegerArrayMedianOfThree(indicesArray->data[lo], indicesArray->data[mid], indicesArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXIntegerArrayPartition3RangeWithFloat(indicesArray, floatArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXIntegerArrayIntroSortRangeWithFloat(indicesArray, floatArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXIntegerArrayIntroSortRangeWithFloat(indicesArray, floatArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_STATIC_INLINE CXBool CXIntegerArraySortWithFloat(CXIntegerArray* indicesArray, CXFloatArray* floatArray, CXComparisonResult order){
	if(indicesArray->count != floatArray->count){
		return CXFalse;
	}
	if(indicesArray->count < 2){
		return CXTrue;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(indicesArray->count);
	CXIntegerArrayIntroSortRangeWithFloat(indicesArray, floatArray, 0, indicesArray->count - 1, order, depthLimit);
	return CXTrue;
}

/** Sorts an index array while permuting a parallel float array accordingly. */
CX_INLINE CXBool CXQuickSortIndicesArrayWithFloat(CXIntegerArray indicesArray, CXFloatArray floatArray){
	return CXIntegerArraySortWithFloat(&indicesArray, &floatArray, CXOrderedAscending);
}

CX_STATIC_INLINE void CXIntegerArraySwapWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXUInteger left, CXUInteger right){
	CXInteger tmpIndex = indicesArray->data[left];
	CXDouble tmpValue = doubleArray->data[left];
	indicesArray->data[left] = indicesArray->data[right];
	doubleArray->data[left] = doubleArray->data[right];
	indicesArray->data[right] = tmpIndex;
	doubleArray->data[right] = tmpValue;
}

CX_STATIC_INLINE void CXIntegerArrayPartition3RangeWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXUInteger lo, CXUInteger hi, CXInteger pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXInteger* indices = indicesArray->data;
	while(i <= gt){
		if(CXIntegerArrayLess(indices[i], pivot, order)){
			CXIntegerArraySwapWithDouble(indicesArray, doubleArray, (CXUInteger)lt, (CXUInteger)i);
			lt++;
			i++;
		}else if(CXIntegerArrayLess(pivot, indices[i], order)){
			CXIntegerArraySwapWithDouble(indicesArray, doubleArray, (CXUInteger)i, (CXUInteger)gt);
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXIntegerArrayInsertionSortRangeWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXInteger* indices = indicesArray->data;
	CXDouble* values = doubleArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXInteger indexValue = indices[i];
		CXDouble doubleValue = values[i];
		CXUInteger j = i;
		while(j > lo && CXIntegerArrayLess(indexValue, indices[j - 1], order)){
			indices[j] = indices[j - 1];
			values[j] = values[j - 1];
			j--;
		}
		indices[j] = indexValue;
		values[j] = doubleValue;
	}
}

CX_STATIC_INLINE void CXIntegerArraySiftDownWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXInteger* indices = indicesArray->data + base;
	CXDouble* values = doubleArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXIntegerArrayLess(indices[swapIndex], indices[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXIntegerArrayLess(indices[swapIndex], indices[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXInteger tmpIndex = indices[root];
		CXDouble tmpValue = values[root];
		indices[root] = indices[swapIndex];
		values[root] = values[swapIndex];
		indices[swapIndex] = tmpIndex;
		values[swapIndex] = tmpValue;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXIntegerArrayHeapSortRangeWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXIntegerArraySiftDownWithDouble(indicesArray, doubleArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXIntegerArraySwapWithDouble(indicesArray, doubleArray, lo, lo + end);
		end--;
		CXIntegerArraySiftDownWithDouble(indicesArray, doubleArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXIntegerArrayIntroSortRangeWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXIntegerArrayInsertionSortRangeWithDouble(indicesArray, doubleArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXIntegerArrayHeapSortRangeWithDouble(indicesArray, doubleArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXInteger pivot = CXIntegerArrayMedianOfThree(indicesArray->data[lo], indicesArray->data[mid], indicesArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXIntegerArrayPartition3RangeWithDouble(indicesArray, doubleArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXIntegerArrayIntroSortRangeWithDouble(indicesArray, doubleArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXIntegerArrayIntroSortRangeWithDouble(indicesArray, doubleArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_STATIC_INLINE CXBool CXIntegerArraySortWithDouble(CXIntegerArray* indicesArray, CXDoubleArray* doubleArray, CXComparisonResult order){
	if(indicesArray->count != doubleArray->count){
		return CXFalse;
	}
	if(indicesArray->count < 2){
		return CXTrue;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(indicesArray->count);
	CXIntegerArrayIntroSortRangeWithDouble(indicesArray, doubleArray, 0, indicesArray->count - 1, order, depthLimit);
	return CXTrue;
}

/** Sorts an index array while permuting a parallel double array accordingly. */
CX_INLINE CXBool CXQuickSortIndicesArrayWithDouble(CXIntegerArray indicesArray, CXDoubleArray doubleArray){
	return CXIntegerArraySortWithDouble(&indicesArray, &doubleArray, CXOrderedAscending);
}

/** Sorts an integer array in ascending order using introsort. */
CX_INLINE CXBool CXQuickSortIndicesArray(CXIntegerArray indicesArray){
	CXIntegerArraySort(&indicesArray, CXOrderedAscending);
	return CXTrue;
}

/** Sorts an unsigned integer array in ascending order using introsort. */
CX_INLINE CXBool CXQuickSortUIntegerArray(CXUIntegerArray indicesArray){
	CXUIntegerArraySort(&indicesArray, CXOrderedAscending);
	return CXTrue;
}

CX_STATIC_INLINE CXUInteger CXUIntegerArrayMedianOfThree(CXUInteger leftValue, CXUInteger middleValue, CXUInteger rightValue, CXComparisonResult order){
	if(CXUIntegerArrayLess(leftValue, middleValue, order)){
		if(CXUIntegerArrayLess(middleValue, rightValue, order)){
			return middleValue;
		}
		if(CXUIntegerArrayLess(leftValue, rightValue, order)){
			return rightValue;
		}
		return leftValue;
	}
	if(CXUIntegerArrayLess(leftValue, rightValue, order)){
		return leftValue;
	}
	if(CXUIntegerArrayLess(middleValue, rightValue, order)){
		return rightValue;
	}
	return middleValue;
}

CX_STATIC_INLINE void CXIntegerArrayPartition3Range(CXIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXInteger pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXInteger* arrayData = theArray->data;
	while(i <= gt){
		if(CXIntegerArrayLess(arrayData[i], pivot, order)){
			CXInteger tmp = arrayData[lt];
			arrayData[lt] = arrayData[i];
			arrayData[i] = tmp;
			lt++;
			i++;
		}else if(CXIntegerArrayLess(pivot, arrayData[i], order)){
			CXInteger tmp = arrayData[gt];
			arrayData[gt] = arrayData[i];
			arrayData[i] = tmp;
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXIntegerArrayInsertionSortRange(CXIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXInteger* arrayData = theArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXInteger value = arrayData[i];
		CXUInteger j = i;
		while(j > lo && CXIntegerArrayLess(value, arrayData[j - 1], order)){
			arrayData[j] = arrayData[j - 1];
			j--;
		}
		arrayData[j] = value;
	}
}

CX_STATIC_INLINE void CXIntegerArraySiftDown(CXIntegerArray* theArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXInteger* arrayData = theArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXIntegerArrayLess(arrayData[swapIndex], arrayData[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXIntegerArrayLess(arrayData[swapIndex], arrayData[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXInteger tmp = arrayData[root];
		arrayData[root] = arrayData[swapIndex];
		arrayData[swapIndex] = tmp;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXIntegerArrayHeapSortRange(CXIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXIntegerArraySiftDown(theArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXInteger tmp = theArray->data[lo];
		theArray->data[lo] = theArray->data[lo + end];
		theArray->data[lo + end] = tmp;
		end--;
		CXIntegerArraySiftDown(theArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXIntegerArrayIntroSortRange(CXIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXIntegerArrayInsertionSortRange(theArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXIntegerArrayHeapSortRange(theArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXInteger pivot = CXIntegerArrayMedianOfThree(theArray->data[lo], theArray->data[mid], theArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXIntegerArrayPartition3Range(theArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXIntegerArrayIntroSortRange(theArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXIntegerArrayIntroSortRange(theArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_INLINE void CXIntegerArraySort(CXIntegerArray* theArray, CXComparisonResult order){
	if(theArray->count < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(theArray->count);
	CXIntegerArrayIntroSortRange(theArray, 0, theArray->count - 1, order, depthLimit);
}

CX_INLINE void CXIntegerArraySortAscending(CXIntegerArray* theArray){
	CXIntegerArraySort(theArray, CXOrderedAscending);
}

CX_INLINE void CXIntegerArraySortDescending(CXIntegerArray* theArray){
	CXIntegerArraySort(theArray, CXOrderedDescending);
}

CX_INLINE void CXIntegerArrayQuickSortImplementation(CXIntegerArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	if(l < f){
		return;
	}
	CXUInteger size = l - f + 1;
	if(size < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(size);
	CXIntegerArrayIntroSortRange(theArray, f, l, comparisonResult, depthLimit);
}

CX_INLINE void CXIntegerArrayInsertSortImplementation(CXIntegerArray* theArray, CXComparisonResult comparisonResult){
	if(theArray->count < 2){
		return;
	}
	CXIntegerArrayInsertionSortRange(theArray, 0, theArray->count - 1, comparisonResult);
}

CX_INLINE void CXIntegerArrayInsertSortImplementation2(CXIntegerArray* theArray){
	CXInteger temp, current, walker;
	CXUInteger count = theArray->count;
	CXInteger* arrayData = theArray->data;

	for(current = 1; current < (CXInteger)count; current++){
		temp = arrayData[current];
		walker = current - 1;
		while(walker >= 0 && temp > arrayData[walker]){
			arrayData[walker + 1] = arrayData[walker];
			walker--;
		}
		arrayData[walker + 1] = temp;
	}

	return;
}

CX_STATIC_INLINE void CXIntegerArrayQuickSort3Implementation(CXIntegerArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	CXIntegerArrayQuickSortImplementation(theArray, f, l, comparisonResult);
}

CX_INLINE void CXIntegerArrayQuickSort3(CXIntegerArray* theArray){
	CXIntegerArraySort(theArray, CXOrderedAscending);
}

CX_STATIC_INLINE void CXUIntegerArrayPartition3Range(CXUIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXUInteger pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXUInteger* arrayData = theArray->data;
	while(i <= gt){
		if(CXUIntegerArrayLess(arrayData[i], pivot, order)){
			CXUInteger tmp = arrayData[lt];
			arrayData[lt] = arrayData[i];
			arrayData[i] = tmp;
			lt++;
			i++;
		}else if(CXUIntegerArrayLess(pivot, arrayData[i], order)){
			CXUInteger tmp = arrayData[gt];
			arrayData[gt] = arrayData[i];
			arrayData[i] = tmp;
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXUIntegerArrayInsertionSortRange(CXUIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger* arrayData = theArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXUInteger value = arrayData[i];
		CXUInteger j = i;
		while(j > lo && CXUIntegerArrayLess(value, arrayData[j - 1], order)){
			arrayData[j] = arrayData[j - 1];
			j--;
		}
		arrayData[j] = value;
	}
}

CX_STATIC_INLINE void CXUIntegerArraySiftDown(CXUIntegerArray* theArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXUInteger* arrayData = theArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXUIntegerArrayLess(arrayData[swapIndex], arrayData[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXUIntegerArrayLess(arrayData[swapIndex], arrayData[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXUInteger tmp = arrayData[root];
		arrayData[root] = arrayData[swapIndex];
		arrayData[swapIndex] = tmp;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXUIntegerArrayHeapSortRange(CXUIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXUIntegerArraySiftDown(theArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXUInteger tmp = theArray->data[lo];
		theArray->data[lo] = theArray->data[lo + end];
		theArray->data[lo + end] = tmp;
		end--;
		CXUIntegerArraySiftDown(theArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXUIntegerArrayIntroSortRange(CXUIntegerArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXUIntegerArrayInsertionSortRange(theArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXUIntegerArrayHeapSortRange(theArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXUInteger pivot = CXUIntegerArrayMedianOfThree(theArray->data[lo], theArray->data[mid], theArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXUIntegerArrayPartition3Range(theArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXUIntegerArrayIntroSortRange(theArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXUIntegerArrayIntroSortRange(theArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_INLINE void CXUIntegerArraySort(CXUIntegerArray* theArray, CXComparisonResult order){
	if(theArray->count < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(theArray->count);
	CXUIntegerArrayIntroSortRange(theArray, 0, theArray->count - 1, order, depthLimit);
}

CX_INLINE void CXUIntegerArraySortAscending(CXUIntegerArray* theArray){
	CXUIntegerArraySort(theArray, CXOrderedAscending);
}

CX_INLINE void CXUIntegerArraySortDescending(CXUIntegerArray* theArray){
	CXUIntegerArraySort(theArray, CXOrderedDescending);
}

CX_STATIC_INLINE void CXFloatArrayPartition3Range(CXFloatArray* theArray, CXUInteger lo, CXUInteger hi, CXFloat pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXFloat* arrayData = theArray->data;
	while(i <= gt){
		if(CXFloatArrayLess(arrayData[i], pivot, order)){
			CXFloat tmp = arrayData[lt];
			arrayData[lt] = arrayData[i];
			arrayData[i] = tmp;
			lt++;
			i++;
		}else if(CXFloatArrayLess(pivot, arrayData[i], order)){
			CXFloat tmp = arrayData[gt];
			arrayData[gt] = arrayData[i];
			arrayData[i] = tmp;
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXFloatArrayInsertionSortRange(CXFloatArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXFloat* arrayData = theArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXFloat value = arrayData[i];
		CXUInteger j = i;
		while(j > lo && CXFloatArrayLess(value, arrayData[j - 1], order)){
			arrayData[j] = arrayData[j - 1];
			j--;
		}
		arrayData[j] = value;
	}
}

CX_STATIC_INLINE void CXFloatArraySiftDown(CXFloatArray* theArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXFloat* arrayData = theArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXFloatArrayLess(arrayData[swapIndex], arrayData[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXFloatArrayLess(arrayData[swapIndex], arrayData[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXFloat tmp = arrayData[root];
		arrayData[root] = arrayData[swapIndex];
		arrayData[swapIndex] = tmp;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXFloatArrayHeapSortRange(CXFloatArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXFloatArraySiftDown(theArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXFloat tmp = theArray->data[lo];
		theArray->data[lo] = theArray->data[lo + end];
		theArray->data[lo + end] = tmp;
		end--;
		CXFloatArraySiftDown(theArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXFloatArrayIntroSortRange(CXFloatArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXFloatArrayInsertionSortRange(theArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXFloatArrayHeapSortRange(theArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXFloat pivot = CXFloatArrayMedianOfThree(theArray->data[lo], theArray->data[mid], theArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXFloatArrayPartition3Range(theArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXFloatArrayIntroSortRange(theArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXFloatArrayIntroSortRange(theArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_INLINE void CXFloatArraySort(CXFloatArray* theArray, CXComparisonResult order){
	if(theArray->count < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(theArray->count);
	CXFloatArrayIntroSortRange(theArray, 0, theArray->count - 1, order, depthLimit);
}

CX_INLINE void CXFloatArraySortAscending(CXFloatArray* theArray){
	CXFloatArraySort(theArray, CXOrderedAscending);
}

CX_INLINE void CXFloatArraySortDescending(CXFloatArray* theArray){
	CXFloatArraySort(theArray, CXOrderedDescending);
}

CX_INLINE void CXFloatArrayQuickSortImplementation(CXFloatArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	if(l < f){
		return;
	}
	CXUInteger size = l - f + 1;
	if(size < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(size);
	CXFloatArrayIntroSortRange(theArray, f, l, comparisonResult, depthLimit);
}

CX_INLINE void CXFloatArrayInsertSortImplementation(CXFloatArray* theArray, CXComparisonResult comparisonResult){
	if(theArray->count < 2){
		return;
	}
	CXFloatArrayInsertionSortRange(theArray, 0, theArray->count - 1, comparisonResult);
}

CX_INLINE void CXFloatArrayInsertSortImplementation2(CXFloatArray* theArray){
	CXFloat temp;
	CXInteger current, walker;
	CXUInteger count = theArray->count;
	CXFloat* arrayData = theArray->data;

	for(current = 1; current < (CXInteger)count; current++){
		temp = arrayData[current];
		walker = current - 1;
		while(walker >= 0 && temp > arrayData[walker]){
			arrayData[walker + 1] = arrayData[walker];
			walker--;
		}
		arrayData[walker + 1] = temp;
	}

	return;
}

CX_STATIC_INLINE void CXFloatArrayQuickSort3Implementation(CXFloatArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	CXFloatArrayQuickSortImplementation(theArray, f, l, comparisonResult);
}

CX_INLINE void CXFloatArrayQuickSort3(CXFloatArray* theArray, CXComparisonResult order){
	CXFloatArraySort(theArray, order);
}

CX_STATIC_INLINE void CXDoubleArrayPartition3Range(CXDoubleArray* theArray, CXUInteger lo, CXUInteger hi, CXDouble pivot, CXComparisonResult order, CXUInteger* outLt, CXUInteger* outGt){
	CXInteger lt = (CXInteger)lo;
	CXInteger i = (CXInteger)lo;
	CXInteger gt = (CXInteger)hi;
	CXDouble* arrayData = theArray->data;
	while(i <= gt){
		if(CXDoubleArrayLess(arrayData[i], pivot, order)){
			CXDouble tmp = arrayData[lt];
			arrayData[lt] = arrayData[i];
			arrayData[i] = tmp;
			lt++;
			i++;
		}else if(CXDoubleArrayLess(pivot, arrayData[i], order)){
			CXDouble tmp = arrayData[gt];
			arrayData[gt] = arrayData[i];
			arrayData[i] = tmp;
			gt--;
		}else{
			i++;
		}
	}
	*outLt = (CXUInteger)lt;
	*outGt = (CXUInteger)gt;
}

CX_STATIC_INLINE void CXDoubleArrayInsertionSortRange(CXDoubleArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXDouble* arrayData = theArray->data;
	for(CXUInteger i = lo + 1; i <= hi; i++){
		CXDouble value = arrayData[i];
		CXUInteger j = i;
		while(j > lo && CXDoubleArrayLess(value, arrayData[j - 1], order)){
			arrayData[j] = arrayData[j - 1];
			j--;
		}
		arrayData[j] = value;
	}
}

CX_STATIC_INLINE void CXDoubleArraySiftDown(CXDoubleArray* theArray, CXUInteger start, CXUInteger end, CXUInteger base, CXComparisonResult order){
	CXDouble* arrayData = theArray->data + base;
	CXUInteger root = start;
	while(root * 2 + 1 <= end){
		CXUInteger child = root * 2 + 1;
		CXUInteger swapIndex = root;
		if(CXDoubleArrayLess(arrayData[swapIndex], arrayData[child], order)){
			swapIndex = child;
		}
		if(child + 1 <= end && CXDoubleArrayLess(arrayData[swapIndex], arrayData[child + 1], order)){
			swapIndex = child + 1;
		}
		if(swapIndex == root){
			return;
		}
		CXDouble tmp = arrayData[root];
		arrayData[root] = arrayData[swapIndex];
		arrayData[swapIndex] = tmp;
		root = swapIndex;
	}
}

CX_STATIC_INLINE void CXDoubleArrayHeapSortRange(CXDoubleArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order){
	CXUInteger size = hi - lo + 1;
	if(size < 2){
		return;
	}
	CXUInteger start = (size - 2) / 2;
	while(CXTrue){
		CXDoubleArraySiftDown(theArray, start, size - 1, lo, order);
		if(start == 0){
			break;
		}
		start--;
	}
	CXUInteger end = size - 1;
	while(end > 0){
		CXDouble tmp = theArray->data[lo];
		theArray->data[lo] = theArray->data[lo + end];
		theArray->data[lo + end] = tmp;
		end--;
		CXDoubleArraySiftDown(theArray, 0, end, lo, order);
	}
}

CX_STATIC_INLINE void CXDoubleArrayIntroSortRange(CXDoubleArray* theArray, CXUInteger lo, CXUInteger hi, CXComparisonResult order, CXUInteger depthLimit){
	while(lo < hi){
		CXUInteger size = hi - lo + 1;
		if(size <= CX_INTROSORT_INSERTION_THRESHOLD){
			CXDoubleArrayInsertionSortRange(theArray, lo, hi, order);
			return;
		}
		if(depthLimit == 0){
			CXDoubleArrayHeapSortRange(theArray, lo, hi, order);
			return;
		}
		depthLimit--;
		CXUInteger mid = lo + (hi - lo) / 2;
		CXDouble pivot = CXDoubleArrayMedianOfThree(theArray->data[lo], theArray->data[mid], theArray->data[hi], order);
		CXUInteger lt = lo;
		CXUInteger gt = hi;
		CXDoubleArrayPartition3Range(theArray, lo, hi, pivot, order, &lt, &gt);
		CXUInteger leftCount = lt > lo ? (lt - lo) : 0;
		CXUInteger rightCount = hi > gt ? (hi - gt) : 0;
		if(leftCount < rightCount){
			if(leftCount > 0){
				CXDoubleArrayIntroSortRange(theArray, lo, lt - 1, order, depthLimit);
			}
			lo = gt + 1;
		}else{
			if(rightCount > 0){
				CXDoubleArrayIntroSortRange(theArray, gt + 1, hi, order, depthLimit);
			}
			if(leftCount == 0){
				return;
			}
			hi = lt - 1;
		}
	}
}

CX_INLINE void CXDoubleArraySort(CXDoubleArray* theArray, CXComparisonResult order){
	if(theArray->count < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(theArray->count);
	CXDoubleArrayIntroSortRange(theArray, 0, theArray->count - 1, order, depthLimit);
}

CX_INLINE void CXDoubleArraySortAscending(CXDoubleArray* theArray){
	CXDoubleArraySort(theArray, CXOrderedAscending);
}

CX_INLINE void CXDoubleArraySortDescending(CXDoubleArray* theArray){
	CXDoubleArraySort(theArray, CXOrderedDescending);
}

CX_INLINE void CXDoubleArrayQuickSortImplementation(CXDoubleArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	if(l < f){
		return;
	}
	CXUInteger size = l - f + 1;
	if(size < 2){
		return;
	}
	CXUInteger depthLimit = 2 * CXFloorLog2(size);
	CXDoubleArrayIntroSortRange(theArray, f, l, comparisonResult, depthLimit);
}

CX_INLINE void CXDoubleArrayInsertSortImplementation(CXDoubleArray* theArray, CXComparisonResult comparisonResult){
	if(theArray->count < 2){
		return;
	}
	CXDoubleArrayInsertionSortRange(theArray, 0, theArray->count - 1, comparisonResult);
}

CX_INLINE void CXDoubleArrayInsertSortImplementation2(CXDoubleArray* theArray){
	CXDouble temp;
	CXInteger current, walker;
	CXUInteger count = theArray->count;
	CXDouble* arrayData = theArray->data;

	for(current = 1; current < (CXInteger)count; current++){
		temp = arrayData[current];
		walker = current - 1;
		while(walker >= 0 && temp > arrayData[walker]){
			arrayData[walker + 1] = arrayData[walker];
			walker--;
		}
		arrayData[walker + 1] = temp;
	}

	return;
}

CX_STATIC_INLINE void CXDoubleArrayQuickSort3Implementation(CXDoubleArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	CXDoubleArrayQuickSortImplementation(theArray, f, l, comparisonResult);
}

CX_INLINE void CXDoubleArrayQuickSort3(CXDoubleArray* theArray, CXComparisonResult order){
	CXDoubleArraySort(theArray, order);
}

#endif
