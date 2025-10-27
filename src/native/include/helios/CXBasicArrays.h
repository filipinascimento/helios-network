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

typedef struct{
	CXFloat* data;
	CXSize count;
	CXSize _capacity;
} CXFloatArray;

typedef CXFloatArray CXFloatStack;

typedef struct{
	CXDouble* data;
	CXSize count;
	CXSize _capacity;
} CXDoubleArray;

typedef CXDoubleArray CXDoubleStack;

typedef struct{
	CXInteger* data;
	CXSize count;
	CXSize _capacity;
} CXIntegerArray;

typedef CXIntegerArray CXIntegerStack;

typedef struct{
	CXInteger* data;
	CXSize count;
	CXSize _capacity;
} CXGLShortArray;

typedef struct{
	CXUInteger* data;
	CXSize count;
	CXSize _capacity;
} CXUIntegerArray;



typedef struct{
	void** data;
	CXSize count;
	CXSize _capacity;
} CXGenericArray;


typedef CXUIntegerArray CXUIntegerStack;
typedef CXGenericArray CXGenericStack;


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

CX_INLINE void CXFloatArrayDestroy(CXFloatArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

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

CX_INLINE void CXFloatArrayAdd(CXFloat value, CXFloatArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXFloatArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

CX_INLINE void CXFloatArraySetCount(CXUInteger count, CXFloatArray* theArray){
	if(theArray->_capacity < count){
		CXFloatArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXFloatArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

CX_INLINE CXFloatStack CXFloatStackMake(){
	CXFloatStack floatStack;
	CXFloatArrayInitWithCapacity(1, &floatStack);
	return floatStack;
}

CX_INLINE CXFloat CXFloatStackPop(CXFloatStack* floatStack){
	if (floatStack->count>0){
		floatStack->count--;
		return floatStack->data[floatStack->count];
	}else{
		return 0.0f;
	}
}

CX_INLINE void CXFloatStackPush(CXFloat value, CXFloatStack* floatStack){
	CXFloatArrayAdd(value, floatStack);
}

CX_INLINE CXFloat CXFloatStackTop(CXFloatStack* floatStack){
	if (floatStack->count>0){
		return floatStack->data[floatStack->count-1];
	}else{
		return 0.0f;
	}
}

CX_INLINE CXBool CXFloatStackIsEmpty(CXFloatStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

CX_INLINE CXFloatArray CXFloatArrayCopy(CXFloatArray* theArray){
	CXFloatArray newArray;
	CXFloatArrayInitWithCapacity(theArray->count, &newArray);
	CXFloatArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXFloat)*theArray->count);
	return newArray;
}


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

CX_INLINE void CXDoubleArrayDestroy(CXDoubleArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

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

CX_INLINE void CXDoubleArrayAdd(CXDouble value, CXDoubleArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXDoubleArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

CX_INLINE void CXDoubleArraySetCount(CXUInteger count, CXDoubleArray* theArray){
	if(theArray->_capacity < count){
		CXDoubleArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXDoubleArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

CX_INLINE CXDoubleStack CXDoubleStackMake(){
	CXDoubleStack floatStack;
	CXDoubleArrayInitWithCapacity(1, &floatStack);
	return floatStack;
}

CX_INLINE CXDouble CXDoubleStackPop(CXDoubleStack* floatStack){
	if (floatStack->count>0){
		floatStack->count--;
		return floatStack->data[floatStack->count];
	}else{
		return 0.0;
	}
}

CX_INLINE void CXDoubleStackPush(CXDouble value, CXDoubleStack* floatStack){
	CXDoubleArrayAdd(value, floatStack);
}

CX_INLINE CXDouble CXDoubleStackTop(CXDoubleStack* floatStack){
	if (floatStack->count>0){
		return floatStack->data[floatStack->count-1];
	}else{
		return 0.0;
	}
}

CX_INLINE CXBool CXDoubleStackIsEmpty(CXDoubleStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

CX_INLINE CXDoubleArray CXDoubleArrayCopy(CXDoubleArray* theArray){
	CXDoubleArray newArray;
	CXDoubleArrayInitWithCapacity(theArray->count, &newArray);
	CXDoubleArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXDouble)*theArray->count);
	return newArray;
}



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

CX_INLINE void CXIntegerArrayDestroy(CXIntegerArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

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

CX_INLINE void CXIntegerArraySetCount(CXUInteger count, CXIntegerArray* theArray){
	if(theArray->_capacity < count){
		CXIntegerArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXIntegerArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}


CX_INLINE void CXIntegerArrayAdd(CXInteger value, CXIntegerArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXIntegerArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

CX_INLINE CXIntegerStack CXIntegerStackMake(){
	CXIntegerStack stack;
	CXIntegerArrayInitWithCapacity(1, &stack);
	return stack;
}

CX_INLINE CXInteger CXIntegerStackPop(CXIntegerStack* stack){
	if(stack->count>0){
		stack->count--;
		return stack->data[stack->count];
	}else{
		return 0;
	}
}

CX_INLINE void CXIntegerStackPush(CXInteger value, CXIntegerStack* stack){
	CXIntegerArrayAdd(value, stack);
}

CX_INLINE CXInteger CXIntegerStackTop(CXIntegerStack* stack){
	if(stack->count>0){
		return stack->data[stack->count-1];
	}else{
		return 0;
	}
}

CX_INLINE CXBool CXIntegerStackIsEmpty(CXIntegerStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

CX_INLINE CXIntegerArray CXIntegerArrayCopy(CXIntegerArray* theArray){
	CXIntegerArray newArray;
	CXIntegerArrayInitWithCapacity(theArray->count, &newArray);
	CXIntegerArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXInteger)*theArray->count);
	return newArray;
}



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

CX_INLINE void CXUIntegerArrayDestroy(CXUIntegerArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}

CX_INLINE void CXUIntegerArrayReallocToCapacity(CXUInteger newCapacity, CXUIntegerArray* theArray){
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(CXUInteger));
	}else{
		theArray->data = calloc(newCapacity, sizeof(CXUInteger));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count) theArray->count = theArray->_capacity;
}

CX_INLINE void CXUIntegerArrayAdd(CXUInteger value, CXUIntegerArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXUIntegerArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}


CX_INLINE void CXUIntegerArraySetCount(CXSize count, CXUIntegerArray* theArray){
	if(theArray->_capacity < count){
		CXUIntegerArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXUIntegerArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

CX_INLINE CXUIntegerStack CXUIntegerStackMake(){
	CXUIntegerStack stack;
	CXUIntegerArrayInitWithCapacity(1, &stack);
	return stack;
}

CX_INLINE CXUInteger CXUIntegerStackPop(CXUIntegerStack* stack){
	if(stack->count>0){
		stack->count--;
		return stack->data[stack->count];
	}else{
		return 0;
	}
}

CX_INLINE void CXUIntegerStackPush(CXUInteger value, CXUIntegerStack* stack){
	CXUIntegerArrayAdd(value, stack);
}

CX_INLINE CXUInteger CXUIntegerStackTop(CXUIntegerStack* stack){
	if(stack->count>0){
		return stack->data[stack->count-1];
	}else{
		return 0;
	}
}

CX_INLINE CXBool CXUIntegerStackIsEmpty(CXUIntegerStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}

CX_INLINE CXUIntegerArray CXUIntegerArrayCopy(CXUIntegerArray* theArray){
	CXUIntegerArray newArray;
	CXUIntegerArrayInitWithCapacity(theArray->count, &newArray);
	CXUIntegerArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(CXUInteger)*theArray->count);
	return newArray;
}


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

CX_INLINE void CXGenericArrayDestroy(CXGenericArray* theArray){
	if(theArray->data!=NULL){
		free(theArray->data);
	}
}



CX_INLINE void CXGenericArrayReallocToCapacity(CXUInteger newCapacity, CXGenericArray* theArray){
	if(theArray->data!=NULL){
		theArray->data = realloc(theArray->data,newCapacity*sizeof(void*));
	}else{
		theArray->data = calloc(newCapacity, sizeof(void*));
	}
	theArray->_capacity=newCapacity;
	if(theArray->_capacity<theArray->count) theArray->count = theArray->_capacity;
}

CX_INLINE void CXGenericArrayAdd(void* value, CXGenericArray* theArray){
	if(theArray->_capacity < theArray->count+1){
		CXGenericArrayReallocToCapacity(CXCapacityGrow(theArray->count+1), theArray);
	}
	theArray->data[theArray->count] = value;
	theArray->count++;
}

CX_INLINE void CXGenericArraySetCount(CXUInteger count, CXGenericArray* theArray){
	if(theArray->_capacity < count){
		CXGenericArrayReallocToCapacity(CXCapacityGrow(count), theArray);
	}else if(theArray->_capacity > count*3){
		CXGenericArrayReallocToCapacity(count, theArray);
	}
	theArray->count = count;
}

CX_INLINE CXGenericStack CXGenericStackMake(){
	CXGenericStack stack;
	CXGenericArrayInitWithCapacity(1, &stack);
	return stack;
}

CX_INLINE void* CXGenericStackPop(CXGenericStack* stack){
	if(stack->count>0){
		stack->count--;
		return stack->data[stack->count];
	}else{
		return 0;
	}
}

CX_INLINE void CXGenericStackPush(void* value, CXGenericStack* stack){
	CXGenericArrayAdd(value, stack);
}

CX_INLINE void* CXGenericStackTop(CXGenericStack* stack){
	if(stack->count>0){
		return stack->data[stack->count-1];
	}else{
		return 0;
	}
}

CX_INLINE CXBool CXGenericStackIsEmpty(CXGenericStack* stack){
	if(stack->count>0){
		return CXFalse;
	}else{
		return CXTrue;
	}
}


CX_INLINE CXGenericArray CXGenericArrayCopy(CXGenericArray* theArray){
	CXGenericArray newArray;
	CXGenericArrayInitWithCapacity(theArray->count, &newArray);
	CXGenericArraySetCount(theArray->count, &newArray);
	memcpy(newArray.data, theArray->data, sizeof(void*)*theArray->count);
	return newArray;
}





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

CX_INLINE void CXIntegerArrayQuickSortImplementation(CXIntegerArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	while(f < l){
		CXUInteger m = CXIntegerArrayPartition(theArray, f, l, theArray->data[f],comparisonResult);
		CXIntegerArrayQuickSortImplementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


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


CX_INLINE void CXIntegerArrayQuickSort3(CXIntegerArray* theArray){
	if(theArray->count==0)
		return;
	CXIntegerArrayQuickSort3Implementation(theArray, 0, theArray->count-1,CXOrderedAscending);
	CXIntegerArrayInsertSortImplementation(theArray,CXOrderedAscending);
}











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

CX_INLINE void CXFloatArrayQuickSortImplementation(CXFloatArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	while(f < l){
		CXUInteger m = CXFloatArrayPartition(theArray, f, l, theArray->data[f],comparisonResult);
		CXFloatArrayQuickSortImplementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


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


CX_INLINE void CXFloatArrayQuickSort3(CXFloatArray* theArray, CXComparisonResult order){
	if(theArray->count==0)
		return;
	CXFloatArrayQuickSort3Implementation(theArray, 0, theArray->count-1,order);
	CXFloatArrayInsertSortImplementation(theArray,order);
}











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

CX_INLINE void CXDoubleArrayQuickSortImplementation(CXDoubleArray* theArray, CXUInteger f, CXUInteger l, CXComparisonResult comparisonResult){
	while(f < l){
		CXUInteger m = CXDoubleArrayPartition(theArray, f, l, theArray->data[f],comparisonResult);
		CXDoubleArrayQuickSortImplementation(theArray, f, m,comparisonResult);
		f = m+1;
	}
}


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


CX_INLINE void CXDoubleArrayQuickSort3(CXDoubleArray* theArray, CXComparisonResult order){
	if(theArray->count==0)
		return;
	CXDoubleArrayQuickSort3Implementation(theArray, 0, theArray->count-1,order);
	CXDoubleArrayInsertSortImplementation(theArray,order);
}
















#endif
