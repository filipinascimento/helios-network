//
//  CXSet.c
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/5/16.
//  Copyright © 2016 Filipi Nascimento Silva. All rights reserved.
//

#include "CXSet.h"

/*
 * Implementation follows the API described in CXSet.h. Each function mirrors
 * the expected behaviour documented in the header; comments here highlight the
 * key allocation choices for each concrete key type.
 */



CXStringSetRef CXNewStringSet(){
	return calloc(1, sizeof(CXStringSet));
}

CXBool CXStringSetHas(const CXStringSetRef set, const CXString key){
	CXStringSetEntry* entry = NULL;
	if(set){
		HASH_FIND_STR((*set), key, entry);
	}
	if(entry){
		return CXTrue;
	}else{
		return CXFalse;
	}
}

void CXStringSetAdd(CXStringSetRef set, const CXString element){
	CXStringSetEntry* entry = calloc(1,sizeof(CXStringSetEntry));
	entry->element = CXNewStringFromString(element);

	CXStringSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND_STR((*set), entry->element, entryTemp);
	}
	if(!entryTemp){
		HASH_ADD_KEYPTR( hh, (*set), entry->element, strlen(entry->element), entry);
	}else{
		free(entry);
	}
}

void CXStringSetRemove(CXStringSetRef set, const CXString element){
	CXStringSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND_STR((*set), element, entryTemp);
	}
	if(entryTemp){
		HASH_DEL((*set), entryTemp);
		free(entryTemp->element);
		free(entryTemp);
	}
}

void CXStringSetClear(CXStringSetRef set){
	CXStringSetEntry* entry = NULL;
	CXStringSetEntry* entryTemp = NULL;
	HASH_ITER(hh, (*set), entry, entryTemp) {
		HASH_DEL((*set), entry);
		free(entry->element);
		free(entry);
	}
}

void CXStringSetDestroy(CXStringSetRef set){
	CXStringSetClear(set);
	free(set);
}

CXStringSetRef CXNewStringSetFromUnion(const CXStringSetRef firtSet, const CXStringSetRef secondSet){
	CXStringSetRef newSet = CXNewStringSet();
	CXStringSetFOR(setEntry, firtSet){
		CXStringSetAdd(newSet, setEntry->element);
	}
	CXStringSetFOR(setEntry, secondSet){
		CXStringSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

void CXStringSetUnion(CXStringSetRef destinationSet, const CXStringSetRef unionSet){
	CXStringSetFOR(setEntry, unionSet){
		CXStringSetAdd(destinationSet, setEntry->element);
	}
}

CXStringSetRef CXNewStringSetFromIntersection(const CXStringSetRef firtSet, const CXStringSetRef secondSet){
	CXStringSetRef newSet = CXNewStringSet();
	CXStringSetFOR(setEntry, firtSet){
		if(CXStringSetHas(secondSet, setEntry->element)){
			CXStringSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXStringSetRef CXNewStringSetFromDifference(const CXStringSetRef firtSet, const CXStringSetRef secondSet){
	CXStringSetRef newSet = CXNewStringSet();
	CXStringSetFOR(setEntry, firtSet){
		if(!CXStringSetHas(secondSet, setEntry->element)){
			CXStringSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXStringSetRef CXNewStringSetFromSymmetricDifference(const CXStringSetRef firtSet, const CXStringSetRef secondSet){
	CXStringSetRef newSet = CXNewStringSet();
	CXStringSetFOR(setEntry, firtSet){
		if(!CXStringSetHas(secondSet, setEntry->element)){
			CXStringSetAdd(newSet, setEntry->element);
		}
	}
	CXStringSetFOR(setEntry, secondSet){
		if(!CXStringSetHas(firtSet, setEntry->element)){
			CXStringSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXStringSetRef CXNewStringSetFromSet(const CXStringSetRef aSet){
	CXStringSetRef newSet = CXNewStringSet();
	CXStringSetFOR(setEntry, aSet){
		CXStringSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

CXBool CXStringSetIsSubsetOf(const CXStringSetRef subSet, const CXStringSetRef superSet){
	CXBool isSubset = CXTrue;
	CXStringSetFOR(setEntry, subSet){
		if(!CXStringSetHas(superSet, setEntry->element)){
			isSubset = CXFalse;
			break;
		}
	}
	return isSubset;
}

CXBool CXStringSetIsSupersetOf(const CXStringSetRef superSet, const CXStringSetRef subSet){
	CXBool isSuperset = CXTrue;
	CXStringSetFOR(setEntry, subSet){
		if(!CXStringSetHas(superSet, setEntry->element)){
			isSuperset = CXFalse;
			break;
		}
	}
	return isSuperset;
}


// CXGeneric



CXGenericSetRef CXNewGenericSet(){
	return calloc(1, sizeof(CXGenericSet));
}

CXBool CXGenericSetHas(const CXGenericSetRef set, const void* element, CXSize elementSize){
	CXGenericSetEntry* entry = NULL;
	if(set){
		HASH_FIND(hh, (*set), element, elementSize, entry);
	}
	if(entry){
		return CXTrue;
	}else{
		return CXFalse;
	}
}

void CXGenericSetAdd(CXGenericSetRef set, const void* element, CXSize elementSize){
	CXGenericSetEntry* entry = calloc(1,sizeof(CXGenericSetEntry));
	entry->element=calloc(1, elementSize);
	memcpy(entry->element, element, elementSize);
	entry->elementSize = elementSize;
	CXGenericSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), entry->element, entry->elementSize, entryTemp);
	}
	if(!entryTemp){
		HASH_ADD_KEYPTR( hh, (*set), entry->element, entry->elementSize, entry);
	}else{
		free(entry);
	}
}

void CXGenericSetRemove(CXGenericSetRef set, const void* element, CXSize elementSize){
	CXGenericSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), element, elementSize, entryTemp);
	}
	if(entryTemp){
		HASH_DEL((*set), entryTemp);
		free(entryTemp->element);
		free(entryTemp);
	}
}

void CXGenericSetClear(CXGenericSetRef set){
	CXGenericSetEntry* entry = NULL;
	CXGenericSetEntry* entryTemp = NULL;
	HASH_ITER(hh, (*set), entry, entryTemp) {
		HASH_DEL((*set), entry);
		free(entry->element);
		free(entry);
	}
}

void CXGenericSetDestroy(CXGenericSetRef set){
	CXGenericSetClear(set);
	free(set);
}

CXGenericSetRef CXNewGenericSetFromUnion(const CXGenericSetRef firtSet, const CXGenericSetRef secondSet){
	CXGenericSetRef newSet = CXNewGenericSet();
	CXGenericSetFOR(setEntry, firtSet){
		CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
	}
	CXGenericSetFOR(setEntry, secondSet){
		CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
	}
	return newSet;
}

void CXGenericSetUnion(CXGenericSetRef destinationSet, const CXGenericSetRef unionSet){
	CXGenericSetFOR(setEntry, unionSet){
		CXGenericSetAdd(destinationSet, setEntry->element, setEntry->elementSize);
	}
}

CXGenericSetRef CXNewGenericSetFromIntersection(const CXGenericSetRef firtSet, const CXGenericSetRef secondSet){
	CXGenericSetRef newSet = CXNewGenericSet();
	CXGenericSetFOR(setEntry, firtSet){
		if(CXGenericSetHas(secondSet, setEntry->element, setEntry->elementSize)){
			CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
		}
	}
	return newSet;
}

CXGenericSetRef CXNewGenericSetFromDifference(const CXGenericSetRef firtSet, const CXGenericSetRef secondSet){
	CXGenericSetRef newSet = CXNewGenericSet();
	CXGenericSetFOR(setEntry, firtSet){
		if(!CXGenericSetHas(secondSet, setEntry->element, setEntry->elementSize)){
			CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
		}
	}
	return newSet;
}

CXGenericSetRef CXNewGenericSetFromSymmetricDifference(const CXGenericSetRef firtSet, const CXGenericSetRef secondSet){
	CXGenericSetRef newSet = CXNewGenericSet();
	CXGenericSetFOR(setEntry, firtSet){
		if(!CXGenericSetHas(secondSet, setEntry->element, setEntry->elementSize)){
			CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
		}
	}
	CXGenericSetFOR(setEntry, secondSet){
		if(!CXGenericSetHas(firtSet, setEntry->element, setEntry->elementSize)){
			CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
		}
	}
	return newSet;
}

CXGenericSetRef CXNewGenericSetFromSet(const CXGenericSetRef aSet){
	CXGenericSetRef newSet = CXNewGenericSet();
	CXGenericSetFOR(setEntry, aSet){
		CXGenericSetAdd(newSet, setEntry->element, setEntry->elementSize);
	}
	return newSet;
}

CXBool CXGenericSetIsSubsetOf(const CXGenericSetRef subSet, const CXGenericSetRef superSet){
	CXBool isSubset = CXTrue;
	CXGenericSetFOR(setEntry, subSet){
		if(!CXGenericSetHas(superSet, setEntry->element, setEntry->elementSize)){
			isSubset = CXFalse;
			break;
		}
	}
	return isSubset;
}

CXBool CXGenericSetIsSupersetOf(const CXGenericSetRef superSet, const CXGenericSetRef subSet){
	CXBool isSuperset = CXTrue;
	CXGenericSetFOR(setEntry, subSet){
		if(!CXGenericSetHas(superSet, setEntry->element, setEntry->elementSize)){
			isSuperset = CXFalse;
			break;
		}
	}
	return isSuperset;
}





//CXUInteger



CXUIntegerSetRef CXNewUIntegerSet(){
	return calloc(1, sizeof(CXUIntegerSet));
}

CXBool CXUIntegerSetHas(const CXUIntegerSetRef set, CXUInteger element){
	CXUIntegerSetEntry* entry = NULL;
	if(set){
		HASH_FIND(hh, (*set), &element, sizeof(CXUInteger), entry);
	}
	if(entry){
		return CXTrue;
	}else{
		return CXFalse;
	}
}

void CXUIntegerSetAdd(CXUIntegerSetRef set, CXUInteger element){
	CXUIntegerSetEntry* entry = calloc(1,sizeof(CXUIntegerSetEntry));
	entry->element = element;
	CXUIntegerSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), &(entry->element), sizeof(CXUInteger), entryTemp);
	}
	if(!entryTemp){
		HASH_ADD_KEYPTR( hh, (*set), &(entry->element), sizeof(CXUInteger), entry);
	}else{
		free(entry);
	}
}

void CXUIntegerSetRemove(CXUIntegerSetRef set, CXUInteger element){
	CXUIntegerSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), &element, sizeof(CXUInteger), entryTemp);
	}
	if(entryTemp){
		HASH_DEL((*set), entryTemp);
		free(entryTemp);
	}
}

void CXUIntegerSetClear(CXUIntegerSetRef set){
	CXUIntegerSetEntry* entry = NULL;
	CXUIntegerSetEntry* entryTemp = NULL;
	HASH_ITER(hh, (*set), entry, entryTemp) {
		HASH_DEL((*set), entry);
		free(entry);
	}
}

void CXUIntegerSetDestroy(CXUIntegerSetRef set){
	CXUIntegerSetClear(set);
	free(set);
}

CXUIntegerSetRef CXNewUIntegerSetFromUnion(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet){
	CXUIntegerSetRef newSet = CXNewUIntegerSet();
	CXUIntegerSetFOR(setEntry, firtSet){
		CXUIntegerSetAdd(newSet, setEntry->element);
	}
	CXUIntegerSetFOR(setEntry, secondSet){
		CXUIntegerSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

void CXUIntegerSetUnion(CXUIntegerSetRef destinationSet, const CXUIntegerSetRef unionSet){
	CXUIntegerSetFOR(setEntry, unionSet){
		CXUIntegerSetAdd(destinationSet, setEntry->element);
	}
}

CXUIntegerSetRef CXNewUIntegerSetFromIntersection(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet){
	CXUIntegerSetRef newSet = CXNewUIntegerSet();
	CXUIntegerSetFOR(setEntry, firtSet){
		if(CXUIntegerSetHas(secondSet, setEntry->element)){
			CXUIntegerSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXUIntegerSetRef CXNewUIntegerSetFromDifference(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet){
	CXUIntegerSetRef newSet = CXNewUIntegerSet();
	CXUIntegerSetFOR(setEntry, firtSet){
		if(!CXUIntegerSetHas(secondSet, setEntry->element)){
			CXUIntegerSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXUIntegerSetRef CXNewUIntegerSetFromSymmetricDifference(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet){
	CXUIntegerSetRef newSet = CXNewUIntegerSet();
	CXUIntegerSetFOR(setEntry, firtSet){
		if(!CXUIntegerSetHas(secondSet, setEntry->element)){
			CXUIntegerSetAdd(newSet, setEntry->element);
		}
	}
	CXUIntegerSetFOR(setEntry, secondSet){
		if(!CXUIntegerSetHas(firtSet, setEntry->element)){
			CXUIntegerSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXUIntegerSetRef CXNewUIntegerSetFromSet(const CXUIntegerSetRef aSet){
	CXUIntegerSetRef newSet = CXNewUIntegerSet();
	CXUIntegerSetFOR(setEntry, aSet){
		CXUIntegerSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

CXBool CXUIntegerSetIsSubsetOf(const CXUIntegerSetRef subSet, const CXUIntegerSetRef superSet){
	CXBool isSubset = CXTrue;
	CXUIntegerSetFOR(setEntry, subSet){
		if(!CXUIntegerSetHas(superSet, setEntry->element)){
			isSubset = CXFalse;
			break;
		}
	}
	return isSubset;
}

CXBool CXUIntegerSetIsSupersetOf(const CXUIntegerSetRef superSet, const CXUIntegerSetRef subSet){
	CXBool isSuperset = CXTrue;
	CXUIntegerSetFOR(setEntry, subSet){
		if(!CXUIntegerSetHas(superSet, setEntry->element)){
			isSuperset = CXFalse;
			break;
		}
	}
	return isSuperset;
}


//CXInteger



CXIntegerSetRef CXNewIntegerSet(){
	return calloc(1, sizeof(CXIntegerSet));
}

CXBool CXIntegerSetHas(const CXIntegerSetRef set, CXInteger element){
	CXIntegerSetEntry* entry = NULL;
	if(set){
		HASH_FIND(hh, (*set), &element, sizeof(CXInteger), entry);
	}
	if(entry){
		return CXTrue;
	}else{
		return CXFalse;
	}
}

void CXIntegerSetAdd(CXIntegerSetRef set, CXInteger element){
	CXIntegerSetEntry* entry = calloc(1,sizeof(CXIntegerSetEntry));
	entry->element = element;
	CXIntegerSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), &(entry->element), sizeof(CXInteger), entryTemp);
	}
	if(!entryTemp){
		HASH_ADD_KEYPTR( hh, (*set), &(entry->element), sizeof(CXInteger), entry);
	}else{
		free(entry);
	}
}

void CXIntegerSetRemove(CXIntegerSetRef set, CXInteger element){
	CXIntegerSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), &element, sizeof(CXInteger), entryTemp);
	}
	if(entryTemp){
		HASH_DEL((*set), entryTemp);
		free(entryTemp);
	}
}

void CXIntegerSetClear(CXIntegerSetRef set){
	CXIntegerSetEntry* entry = NULL;
	CXIntegerSetEntry* entryTemp = NULL;
	HASH_ITER(hh, (*set), entry, entryTemp) {
		HASH_DEL((*set), entry);
		free(entry);
	}
}

void CXIntegerSetDestroy(CXIntegerSetRef set){
	CXIntegerSetClear(set);
	free(set);
}

CXIntegerSetRef CXNewIntegerSetFromUnion(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet){
	CXIntegerSetRef newSet = CXNewIntegerSet();
	CXIntegerSetFOR(setEntry, firtSet){
		CXIntegerSetAdd(newSet, setEntry->element);
	}
	CXIntegerSetFOR(setEntry, secondSet){
		CXIntegerSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

void CXIntegerSetUnion(CXIntegerSetRef destinationSet, const CXIntegerSetRef unionSet){
	CXIntegerSetFOR(setEntry, unionSet){
		CXIntegerSetAdd(destinationSet, setEntry->element);
	}
}

CXIntegerSetRef CXNewIntegerSetFromIntersection(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet){
	CXIntegerSetRef newSet = CXNewIntegerSet();
	CXIntegerSetFOR(setEntry, firtSet){
		if(CXIntegerSetHas(secondSet, setEntry->element)){
			CXIntegerSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXIntegerSetRef CXNewIntegerSetFromDifference(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet){
	CXIntegerSetRef newSet = CXNewIntegerSet();
	CXIntegerSetFOR(setEntry, firtSet){
		if(!CXIntegerSetHas(secondSet, setEntry->element)){
			CXIntegerSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXIntegerSetRef CXNewIntegerSetFromSymmetricDifference(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet){
	CXIntegerSetRef newSet = CXNewIntegerSet();
	CXIntegerSetFOR(setEntry, firtSet){
		if(!CXIntegerSetHas(secondSet, setEntry->element)){
			CXIntegerSetAdd(newSet, setEntry->element);
		}
	}
	CXIntegerSetFOR(setEntry, secondSet){
		if(!CXIntegerSetHas(firtSet, setEntry->element)){
			CXIntegerSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXIntegerSetRef CXNewIntegerSetFromSet(const CXIntegerSetRef aSet){
	CXIntegerSetRef newSet = CXNewIntegerSet();
	CXIntegerSetFOR(setEntry, aSet){
		CXIntegerSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

CXBool CXIntegerSetIsSubsetOf(const CXIntegerSetRef subSet, const CXIntegerSetRef superSet){
	CXBool isSubset = CXTrue;
	CXIntegerSetFOR(setEntry, subSet){
		if(!CXIntegerSetHas(superSet, setEntry->element)){
			isSubset = CXFalse;
			break;
		}
	}
	return isSubset;
}

CXBool CXIntegerSetIsSupersetOf(const CXIntegerSetRef superSet, const CXIntegerSetRef subSet){
	CXBool isSuperset = CXTrue;
	CXIntegerSetFOR(setEntry, subSet){
		if(!CXIntegerSetHas(superSet, setEntry->element)){
			isSuperset = CXFalse;
			break;
		}
	}
	return isSuperset;
}


//CXEdge



CXEdgeSetRef CXNewEdgeSet(){
	return calloc(1, sizeof(CXEdgeSet));
}

CXBool CXEdgeSetHas(const CXEdgeSetRef set, CXEdge element){
	CXEdgeSetEntry* entry = NULL;
	if(set){
		HASH_FIND(hh, (*set), &element, sizeof(CXEdge), entry);
	}
	if(entry){
		return CXTrue;
	}else{
		return CXFalse;
	}
}

void CXEdgeSetAdd(CXEdgeSetRef set, CXEdge element){
	CXEdgeSetEntry* entry = calloc(1,sizeof(CXEdgeSetEntry));
	entry->element = element;
	CXEdgeSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), &(entry->element), sizeof(CXEdge), entryTemp);
	}
	if(!entryTemp){
		HASH_ADD_KEYPTR( hh, (*set), &(entry->element), sizeof(CXEdge), entry);
	}else{
		free(entry);
	}
}

void CXEdgeSetRemove(CXEdgeSetRef set, CXEdge element){
	CXEdgeSetEntry* entryTemp = NULL;
	if(set){
		HASH_FIND(hh, (*set), &element, sizeof(CXEdge), entryTemp);
	}
	if(entryTemp){
		HASH_DEL((*set), entryTemp);
		free(entryTemp);
	}
}

void CXEdgeSetClear(CXEdgeSetRef set){
	CXEdgeSetEntry* entry = NULL;
	CXEdgeSetEntry* entryTemp = NULL;
	HASH_ITER(hh, (*set), entry, entryTemp) {
		HASH_DEL((*set), entry);
		free(entry);
	}
}

void CXEdgeSetDestroy(CXEdgeSetRef set){
	CXEdgeSetClear(set);
	free(set);
}

CXEdgeSetRef CXNewEdgeSetFromUnion(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet){
	CXEdgeSetRef newSet = CXNewEdgeSet();
	CXEdgeSetFOR(setEntry, firtSet){
		CXEdgeSetAdd(newSet, setEntry->element);
	}
	CXEdgeSetFOR(setEntry, secondSet){
		CXEdgeSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

void CXEdgeSetUnion(CXEdgeSetRef destinationSet, const CXEdgeSetRef unionSet){
	CXEdgeSetFOR(setEntry, unionSet){
		CXEdgeSetAdd(destinationSet, setEntry->element);
	}
}

CXEdgeSetRef CXNewEdgeSetFromIntersection(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet){
	CXEdgeSetRef newSet = CXNewEdgeSet();
	CXEdgeSetFOR(setEntry, firtSet){
		if(CXEdgeSetHas(secondSet, setEntry->element)){
			CXEdgeSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXEdgeSetRef CXNewEdgeSetFromDifference(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet){
	CXEdgeSetRef newSet = CXNewEdgeSet();
	CXEdgeSetFOR(setEntry, firtSet){
		if(!CXEdgeSetHas(secondSet, setEntry->element)){
			CXEdgeSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXEdgeSetRef CXNewEdgeSetFromSymmetricDifference(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet){
	CXEdgeSetRef newSet = CXNewEdgeSet();
	CXEdgeSetFOR(setEntry, firtSet){
		if(!CXEdgeSetHas(secondSet, setEntry->element)){
			CXEdgeSetAdd(newSet, setEntry->element);
		}
	}
	CXEdgeSetFOR(setEntry, secondSet){
		if(!CXEdgeSetHas(firtSet, setEntry->element)){
			CXEdgeSetAdd(newSet, setEntry->element);
		}
	}
	return newSet;
}

CXEdgeSetRef CXNewEdgeSetFromSet(const CXEdgeSetRef aSet){
	CXEdgeSetRef newSet = CXNewEdgeSet();
	CXEdgeSetFOR(setEntry, aSet){
		CXEdgeSetAdd(newSet, setEntry->element);
	}
	return newSet;
}

CXBool CXEdgeSetIsSubsetOf(const CXEdgeSetRef subSet, const CXEdgeSetRef superSet){
	CXBool isSubset = CXTrue;
	CXEdgeSetFOR(setEntry, subSet){
		if(!CXEdgeSetHas(superSet, setEntry->element)){
			isSubset = CXFalse;
			break;
		}
	}
	return isSubset;
}

CXBool CXEdgeSetIsSupersetOf(const CXEdgeSetRef superSet, const CXEdgeSetRef subSet){
	CXBool isSuperset = CXTrue;
	CXEdgeSetFOR(setEntry, subSet){
		if(!CXEdgeSetHas(superSet, setEntry->element)){
			isSuperset = CXFalse;
			break;
		}
	}
	return isSuperset;
}

