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

/** Sorts a float array in ascending order while reordering the parallel index array. */
CX_INLINE CXBool CXQuickSortFloatArrayWithIndices(CXFloatArray floatArray, CXUIntegerArray indicesArray){
	CXUInteger MAX_LEVELS=floatArray.count;
	if(floatArray.count!=indicesArray.count){
		return CXFalse;
	}
	CXFloat  piv;
	CXInteger piv2, i=0, L, R ;
	CXInteger* beg=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	CXInteger* end=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	
	beg[0]=0; end[0]=floatArray.count;
	while (i>=0) {
		L=beg[i];
		R=end[i]-1;
		if (L<R) {
			piv=floatArray.data[L];
			piv2=indicesArray.data[L];
			if (i==MAX_LEVELS-1){
				free(beg);
				return CXFalse;
			}
			
			while (L<R) {
				while (floatArray.data[R]>=piv && L<R)
					R--;
				
				if (L<R){
					indicesArray.data[L]=indicesArray.data[R];
					floatArray.data[L++]=floatArray.data[R];
				}
				while (floatArray.data[L]<=piv && L<R)
					L++;
				
				if (L<R){
					indicesArray.data[R]=indicesArray.data[L];
					floatArray.data[R--]=floatArray.data[L];
				}
			}
			floatArray.data[L]=piv;
			indicesArray.data[L]=piv2;
			beg[i+1]=L+1;
			end[i+1]=end[i];
			end[i++]=L;
		}
		else {
			i--;
		}
	}
	free(beg);
	free(end);
	return CXTrue;
}

/** Sorts an index array while permuting a parallel float array accordingly. */
CX_INLINE CXBool CXQuickSortIndicesArrayWithFloat(CXIntegerArray indicesArray,CXFloatArray floatArray){
	CXInteger MAX_LEVELS=indicesArray.count;
	if(indicesArray.count!=floatArray.count){
		return CXFalse;
	}
	CXInteger  piv, i=0, L, R ;
	CXFloat piv2;
	CXInteger* beg=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	CXInteger* end=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	
	beg[0]=0; end[0]=indicesArray.count;
	while (i>=0) {
		L=beg[i];
		R=end[i]-1;
		if (L<R) {
			piv2=floatArray.data[L];
			piv=indicesArray.data[L];
			if (i==MAX_LEVELS-1){
				free(beg);
				return CXFalse;
			}
			
			while (L<R) {
				while (indicesArray.data[R]>=piv && L<R)
					R--;
				
				if (L<R){
					indicesArray.data[L]=indicesArray.data[R];
					floatArray.data[L++]=floatArray.data[R];
				}
				while (indicesArray.data[L]<=piv && L<R)
					L++;
				
				if (L<R){
					indicesArray.data[R]=indicesArray.data[L];
					floatArray.data[R--]=floatArray.data[L];
				}
			}
			floatArray.data[L]=piv2;
			indicesArray.data[L]=piv;
			beg[i+1]=L+1;
			end[i+1]=end[i];
			end[i++]=L;
		}
		else {
			i--;
		}
	}
	free(beg);
	free(end);
	return CXTrue;
}
/** Sorts a double array in ascending order while reordering the parallel index array. */
CX_INLINE CXBool CXQuickSortDoubleArrayWithIndices(CXDoubleArray doubleArray, CXUIntegerArray indicesArray){
	CXUInteger MAX_LEVELS=doubleArray.count;
	if(doubleArray.count!=indicesArray.count){
		return CXFalse;
	}
	CXDouble  piv;
	CXInteger piv2, i=0, L, R ;
	CXInteger* beg=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	CXInteger* end=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));

	beg[0]=0; end[0]=doubleArray.count;
	while (i>=0) {
		L=beg[i];
		R=end[i]-1;
		if (L<R) {
			piv=doubleArray.data[L];
			piv2=indicesArray.data[L];
			if (i==MAX_LEVELS-1){
				free(beg);
				return CXFalse;
			}

			while (L<R) {
				while (doubleArray.data[R]>=piv && L<R)
					R--;

				if (L<R){
					indicesArray.data[L]=indicesArray.data[R];
					doubleArray.data[L++]=doubleArray.data[R];
				}
				while (doubleArray.data[L]<=piv && L<R)
					L++;

				if (L<R){
					indicesArray.data[R]=indicesArray.data[L];
					doubleArray.data[R--]=doubleArray.data[L];
				}
			}
			doubleArray.data[L]=piv;
			indicesArray.data[L]=piv2;
			beg[i+1]=L+1;
			end[i+1]=end[i];
			end[i++]=L;
		}
		else {
			i--;
		}
	}
	free(beg);
	free(end);
	return CXTrue;
}

/** Sorts an index array while permuting a parallel double array accordingly. */
CX_INLINE CXBool CXQuickSortIndicesArrayWithDouble(CXIntegerArray indicesArray,CXDoubleArray doubleArray){
	CXInteger MAX_LEVELS=indicesArray.count;
	if(indicesArray.count!=doubleArray.count){
		return CXFalse;
	}
	CXInteger  piv, i=0, L, R ;
	CXDouble piv2;
	CXInteger* beg=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	CXInteger* end=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));

	beg[0]=0; end[0]=indicesArray.count;
	while (i>=0) {
		L=beg[i];
		R=end[i]-1;
		if (L<R) {
			piv2=doubleArray.data[L];
			piv=indicesArray.data[L];
			if (i==MAX_LEVELS-1){
				free(beg);
				return CXFalse;
			}

			while (L<R) {
				while (indicesArray.data[R]>=piv && L<R)
					R--;

				if (L<R){
					indicesArray.data[L]=indicesArray.data[R];
					doubleArray.data[L++]=doubleArray.data[R];
				}
				while (indicesArray.data[L]<=piv && L<R)
					L++;

				if (L<R){
					indicesArray.data[R]=indicesArray.data[L];
					doubleArray.data[R--]=doubleArray.data[L];
				}
			}
			doubleArray.data[L]=piv2;
			indicesArray.data[L]=piv;
			beg[i+1]=L+1;
			end[i+1]=end[i];
			end[i++]=L;
		}
		else {
			i--;
		}
	}
	free(beg);
	free(end);
	return CXTrue;
}
/** Sorts an integer array in ascending order using an iterative quicksort. */
CX_INLINE CXBool CXQuickSortIndicesArray(CXIntegerArray indicesArray){
	CXUInteger MAX_LEVELS=indicesArray.count;
	CXInteger  piv, i=0, L, R ;
	CXInteger* beg=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	CXInteger* end=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	
	beg[0]=0; end[0]=indicesArray.count;
	while (i>=0) {
		L=beg[i];
		R=end[i]-1;
		if (L<R) {
			piv=indicesArray.data[L];
			if (i==MAX_LEVELS-1){
				free(beg);
				return CXFalse;
			}
			
			while (L<R) {
				while (indicesArray.data[R]>=piv && L<R)
					R--;
				
				if (L<R){
					indicesArray.data[L++]=indicesArray.data[R];
				}
				while (indicesArray.data[L]<=piv && L<R)
					L++;
				
				if (L<R){
					indicesArray.data[R--]=indicesArray.data[L];
				}
			}
			indicesArray.data[L]=piv;
			beg[i+1]=L+1;
			end[i+1]=end[i];
			end[i++]=L;
		}
		else {
			i--;
		}
	}
	free(beg);
	free(end);
	return CXTrue;
}
/** Sorts an unsigned integer array in ascending order using an iterative quicksort. */
CX_INLINE CXBool CXQuickSortUIntegerArray(CXUIntegerArray indicesArray){
	CXUInteger MAX_LEVELS=indicesArray.count;
	CXUInteger  piv;
	CXInteger i=0, L, R ;
	CXInteger* beg=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	CXInteger* end=(CXInteger*) calloc(MAX_LEVELS, sizeof(CXInteger));
	
	beg[0]=0; end[0]=indicesArray.count;
	while (i>=0) {
		L=beg[i];
		R=end[i]-1;
		if (L<R) {
			piv=indicesArray.data[L];
			if (i==MAX_LEVELS-1){
				free(beg);
				return CXFalse;
			}
			
			while (L<R) {
				while (indicesArray.data[R]>=piv && L<R)
					R--;
				
				if (L<R){
					indicesArray.data[L++]=indicesArray.data[R];
				}
				while (indicesArray.data[L]<=piv && L<R)
					L++;
				
				if (L<R){
					indicesArray.data[R--]=indicesArray.data[L];
				}
			}
			indicesArray.data[L]=piv;
			beg[i+1]=L+1;
			end[i+1]=end[i];
			end[i++]=L;
		}
		else {
			i--;
		}
	}
	free(beg);
	free(end);
	return CXTrue;
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

/** Internal helper that partitions the array segment around `pivot`. */
CX_INLINE CXUInteger CXIntegerArrayPartition(CXIntegerArray* theArray, CXUInteger f, CXUInteger l, CXInteger pivot, CXComparisonResult comparisonResult){
	CXUInteger i = f-1, j = l+1;
	CXInteger* arrayData = theArray->data;
	while(CXTrue){
		do{
			j--;
		}while(CX_ARRAY_COMPARE_FUNCTION(pivot,arrayData[j])==comparisonResult);
		
		do{
			i++;
		}while(CX_ARRAY_COMPARE_FUNCTION(arrayData[i],pivot)==comparisonResult);
		
		if(i<j){
			CXInteger tmp = arrayData[i];
			arrayData[i] = arrayData[j];
			arrayData[j] = tmp;
		}else{
			return j;
		}
	}
}

/** Recursive quicksort implementation used when index sorting is insufficient. */
CX_INLINE void CXIntegerArrayQuickSortImplementation(CXIntegerArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	while(f < l){
		CXUInteger m = CXIntegerArrayPartition(theArray, f, l, theArray->data[f],comparisonResult);
		CXIntegerArrayQuickSortImplementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


/** Simple insertion sort used for small partitions to improve cache locality. */
/** In-place insertion sort that honours the provided comparison direction. */
CX_INLINE void CXIntegerArrayInsertSortImplementation(CXIntegerArray* theArray, CXComparisonResult comparisonResult){
	if(theArray->count==0)
		return;
	CXUInteger i,count = theArray->count;
	CXInteger* arrayData = theArray->data;
	for(i = 1; i < count; i++){
		CXInteger value = arrayData[i];
		CXUInteger j = i;
		while(j > 0 && CX_ARRAY_COMPARE_FUNCTION(value,arrayData[j-1])==comparisonResult){
			arrayData[j] = arrayData[j-1];
			j--;
		}
		theArray->data[j] = value;
	}
}

/** Variant of insertion sort that orders values in descending order. */
CX_INLINE void CXIntegerArrayInsertSortImplementation2(CXIntegerArray* theArray){
    //  Local Declaration
    CXInteger temp, current, walker;
	CXUInteger count = theArray->count;
	CXInteger* arrayData = theArray->data;
	
    //  Statement
    for(current = 1; current < count; current++)
    {
        temp = arrayData[current];
        walker = current - 1;
        while(walker >= 0 && temp > arrayData[walker])
        {
            arrayData[walker + 1] = arrayData[walker];
            walker--;
        }
        arrayData[walker + 1] = temp;
    }
	
    return;
}

/** Quicksort implementation that leverages median-of-three pivot selection. */
CX_STATIC_INLINE void CXIntegerArrayQuickSort3Implementation(CXIntegerArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	if(theArray->count==0)
		return;
	CXInteger* arrayData = theArray->data;
	while(f + 16 < l){
		CXInteger v1 = arrayData[f], v2 = arrayData[l], v3 = arrayData[(f+l)/2];
        CXInteger median;
		if(CX_ARRAY_COMPARE_FUNCTION(v1,v2)==comparisonResult){
			if(CX_ARRAY_COMPARE_FUNCTION(v3,v1)==comparisonResult){
				median = v1;
			}else{
				if(CX_ARRAY_COMPARE_FUNCTION(v2,v3)==comparisonResult){
					median=v2;
				}else{
					median=v3;
				}
			}
		}else{
			if(CX_ARRAY_COMPARE_FUNCTION(v3,v2)==comparisonResult){
				median = v2;
			}else{
				if(CX_ARRAY_COMPARE_FUNCTION(v1,v3)==comparisonResult){
					median=v1;
				}else{
					median=v3;
				}
			}
		}
		CXInteger m = CXIntegerArrayPartition(theArray, f, l, median,comparisonResult);
		CXIntegerArrayQuickSort3Implementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


/** Public entry point that sorts the integer array in ascending order. */
CX_INLINE void CXIntegerArrayQuickSort3(CXIntegerArray* theArray){
	if(theArray->count==0)
		return;
	CXIntegerArrayQuickSort3Implementation(theArray, 0, theArray->count-1,CXOrderedAscending);
	CXIntegerArrayInsertSortImplementation(theArray,CXOrderedAscending);
}











/** Internal helper that partitions a float array segment around `pivot`. */
CX_INLINE CXUInteger CXFloatArrayPartition(CXFloatArray* theArray, CXUInteger f, CXUInteger l, CXFloat pivot, CXComparisonResult comparisonResult){
	CXUInteger i = f-1, j = l+1;
	CXFloat* arrayData = theArray->data;
	while(CXTrue){
		do{
			j--;
		}while(CX_ARRAY_COMPARE_FUNCTION(pivot,arrayData[j])==comparisonResult);
		
		do{
			i++;
		}while(CX_ARRAY_COMPARE_FUNCTION(arrayData[i],pivot)==comparisonResult);
		
		if(i<j){
			CXFloat tmp = arrayData[i];
			arrayData[i] = arrayData[j];
			arrayData[j] = tmp;
		}else{
			return j;
		}
	}
}

/** Recursive quicksort implementation used for float arrays. */
CX_INLINE void CXFloatArrayQuickSortImplementation(CXFloatArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	while(f < l){
		CXUInteger m = CXFloatArrayPartition(theArray, f, l, theArray->data[f],comparisonResult);
		CXFloatArrayQuickSortImplementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


/** In-place insertion sort for float arrays that honours the comparison flag. */
CX_INLINE void CXFloatArrayInsertSortImplementation(CXFloatArray* theArray, CXComparisonResult comparisonResult){
	if(theArray->count==0)
		return;
	CXUInteger i,count = theArray->count;
	CXFloat* arrayData = theArray->data;
	for(i = 1; i < count; i++){
		CXFloat value = arrayData[i];
		CXUInteger j = i;
		while(j > 0 && CX_ARRAY_COMPARE_FUNCTION(value,arrayData[j-1])==comparisonResult){
			arrayData[j] = arrayData[j-1];
			j--;
		}
		theArray->data[j] = value;
	}
}

/** Variant of insertion sort that orders float values in descending order. */
CX_INLINE void CXFloatArrayInsertSortImplementation2(CXFloatArray* theArray){
	//  Local Declaration
	CXFloat temp;
	CXInteger current, walker;
	CXUInteger count = theArray->count;
	CXFloat* arrayData = theArray->data;
	
	//  Statement
	for(current = 1; current < count; current++)
	{
		temp = arrayData[current];
		walker = current - 1;
		while(walker >= 0 && temp > arrayData[walker])
		{
			arrayData[walker + 1] = arrayData[walker];
			walker--;
		}
		arrayData[walker + 1] = temp;
	}
	
	return;
}

/** Quicksort implementation for float arrays leveraging median-of-three pivoting. */
CX_STATIC_INLINE void CXFloatArrayQuickSort3Implementation(CXFloatArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	if(theArray->count==0)
		return;
	CXFloat* arrayData = theArray->data;
	while(f + 16 < l){
		CXFloat v1 = arrayData[f], v2 = arrayData[l], v3 = arrayData[(f+l)/2];
		CXFloat median;
		if(CX_ARRAY_COMPARE_FUNCTION(v1,v2)==comparisonResult){
			if(CX_ARRAY_COMPARE_FUNCTION(v3,v1)==comparisonResult){
				median = v1;
			}else{
				if(CX_ARRAY_COMPARE_FUNCTION(v2,v3)==comparisonResult){
					median=v2;
				}else{
					median=v3;
				}
			}
		}else{
			if(CX_ARRAY_COMPARE_FUNCTION(v3,v2)==comparisonResult){
				median = v2;
			}else{
				if(CX_ARRAY_COMPARE_FUNCTION(v1,v3)==comparisonResult){
					median=v1;
				}else{
					median=v3;
				}
			}
		}
		CXFloat m = CXFloatArrayPartition(theArray, f, l, median,comparisonResult);
		CXFloatArrayQuickSort3Implementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


/** Public entry point that sorts the float array in the requested order. */
CX_INLINE void CXFloatArrayQuickSort3(CXFloatArray* theArray, CXComparisonResult order){
	if(theArray->count==0)
		return;
	CXFloatArrayQuickSort3Implementation(theArray, 0, theArray->count-1,order);
	CXFloatArrayInsertSortImplementation(theArray,order);
}











/** Internal helper that partitions a double array segment around `pivot`. */
CX_INLINE CXUInteger CXDoubleArrayPartition(CXDoubleArray* theArray, CXUInteger f, CXUInteger l, CXDouble pivot, CXComparisonResult comparisonResult){
	CXUInteger i = f-1, j = l+1;
	CXDouble* arrayData = theArray->data;
	while(CXTrue){
		do{
			j--;
		}while(CX_ARRAY_COMPARE_FUNCTION(pivot,arrayData[j])==comparisonResult);

		do{
			i++;
		}while(CX_ARRAY_COMPARE_FUNCTION(arrayData[i],pivot)==comparisonResult);

		if(i<j){
			CXDouble tmp = arrayData[i];
			arrayData[i] = arrayData[j];
			arrayData[j] = tmp;
		}else{
			return j;
		}
	}
}

/** Recursive quicksort implementation used for double arrays. */
CX_INLINE void CXDoubleArrayQuickSortImplementation(CXDoubleArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	while(f < l){
		CXUInteger m = CXDoubleArrayPartition(theArray, f, l, theArray->data[f],comparisonResult);
		CXDoubleArrayQuickSortImplementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


/** In-place insertion sort for double arrays that honours the comparison flag. */
CX_INLINE void CXDoubleArrayInsertSortImplementation(CXDoubleArray* theArray, CXComparisonResult comparisonResult){
	if(theArray->count==0)
		return;
	CXUInteger i,count = theArray->count;
	CXDouble* arrayData = theArray->data;
	for(i = 1; i < count; i++){
		CXDouble value = arrayData[i];
		CXUInteger j = i;
		while(j > 0 && CX_ARRAY_COMPARE_FUNCTION(value,arrayData[j-1])==comparisonResult){
			arrayData[j] = arrayData[j-1];
			j--;
		}
		theArray->data[j] = value;
	}
}

/** Variant of insertion sort that orders double values in descending order. */
CX_INLINE void CXDoubleArrayInsertSortImplementation2(CXDoubleArray* theArray){
	//  Local Declaration
	CXDouble temp;
	CXInteger current, walker;
	CXUInteger count = theArray->count;
	CXDouble* arrayData = theArray->data;

	//  Statement
	for(current = 1; current < count; current++)
	{
		temp = arrayData[current];
		walker = current - 1;
		while(walker >= 0 && temp > arrayData[walker])
		{
			arrayData[walker + 1] = arrayData[walker];
			walker--;
		}
		arrayData[walker + 1] = temp;
	}

	return;
}

/** Quicksort implementation for double arrays leveraging median-of-three pivoting. */
CX_STATIC_INLINE void CXDoubleArrayQuickSort3Implementation(CXDoubleArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	if(theArray->count==0)
		return;
	CXDouble* arrayData = theArray->data;
	while(f + 16 < l){
		CXDouble v1 = arrayData[f], v2 = arrayData[l], v3 = arrayData[(f+l)/2];
		CXDouble median;
		if(CX_ARRAY_COMPARE_FUNCTION(v1,v2)==comparisonResult){
			if(CX_ARRAY_COMPARE_FUNCTION(v3,v1)==comparisonResult){
				median = v1;
			}else{
				if(CX_ARRAY_COMPARE_FUNCTION(v2,v3)==comparisonResult){
					median=v2;
				}else{
					median=v3;
				}
			}
		}else{
			if(CX_ARRAY_COMPARE_FUNCTION(v3,v2)==comparisonResult){
				median = v2;
			}else{
				if(CX_ARRAY_COMPARE_FUNCTION(v1,v3)==comparisonResult){
					median=v1;
				}else{
					median=v3;
				}
			}
		}
		CXDouble m = CXDoubleArrayPartition(theArray, f, l, median,comparisonResult);
		CXDoubleArrayQuickSort3Implementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


/** Public entry point that sorts the double array in the requested order. */
CX_INLINE void CXDoubleArrayQuickSort3(CXDoubleArray* theArray, CXComparisonResult order){
	if(theArray->count==0)
		return;
	CXDoubleArrayQuickSort3Implementation(theArray, 0, theArray->count-1,order);
	CXDoubleArrayInsertSortImplementation(theArray,order);
}
















#endif
