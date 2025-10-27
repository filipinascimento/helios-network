//
//  CXSet.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/5/16.
//  Copyright © 2016 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXSet_h
#define CXSet_h


#include "uthash.h"
#include "CXCommons.h"


typedef struct {
	char* element;
	UT_hash_handle hh;
} CXStringSetEntry;

typedef CXStringSetEntry* CXStringSet;
typedef CXStringSet* CXStringSetRef;

#define CXStringSetFOR(setEntry,set) for(CXStringSetEntry* setEntry=*set; setEntry != NULL; setEntry=setEntry->hh.next)

CXStringSetRef CXNewStringSet();
CXBool CXStringSetHas(const CXStringSetRef set, const CXString key);
void CXStringSetAdd(CXStringSetRef set, const CXString element);
void CXStringSetRemove(CXStringSetRef set, const CXString element);
void CXStringSetClear(CXStringSetRef set);
void CXStringSetDestroy(CXStringSetRef set);
CXStringSetRef CXNewStringSetFromUnion(const CXStringSetRef firtSet, const CXStringSetRef secondSet);
void CXStringSetUnion(CXStringSetRef destinationSet, const CXStringSetRef unionSet);
CXStringSetRef CXNewStringSetFromIntersection(const CXStringSetRef firtSet, const CXStringSetRef secondSet);
CXStringSetRef CXNewStringSetFromDifference(const CXStringSetRef firtSet, const CXStringSetRef secondSet);
CXStringSetRef CXNewStringSetFromSymmetricDifference(const CXStringSetRef firtSet, const CXStringSetRef secondSet);
CXStringSetRef CXNewStringSetFromSet(const CXStringSetRef aSet);
CXBool CXStringSetIsSubsetOf(const CXStringSetRef subSet, const CXStringSetRef superSet);
CXBool CXStringSetIsSupersetOf(const CXStringSetRef superSet, const CXStringSetRef subSet);

CX_INLINE CXSize CXStringSetCount(CXStringSetRef aSet){
	return (CXSize)HASH_COUNT((*aSet));
}





typedef struct {
	CXInteger element;
	UT_hash_handle hh;
} CXIntegerSetEntry;

typedef CXIntegerSetEntry* CXIntegerSet;
typedef CXIntegerSet* CXIntegerSetRef;

#define CXIntegerSetFOR(setEntry,set) for(CXIntegerSetEntry* setEntry=*set; setEntry != NULL; setEntry=setEntry->hh.next)


CXIntegerSetRef CXNewIntegerSet();
CXBool CXIntegerSetHas(const CXIntegerSetRef set, const CXInteger key);
void CXIntegerSetAdd(CXIntegerSetRef set, const CXInteger element);
void CXIntegerSetRemove(CXIntegerSetRef set, const CXInteger element);
void CXIntegerSetClear(CXIntegerSetRef set);
void CXIntegerSetDestroy(CXIntegerSetRef set);
CXIntegerSetRef CXNewIntegerSetFromUnion(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet);
void CXIntegerSetUnion(CXIntegerSetRef destinationSet, const CXIntegerSetRef unionSet);
CXIntegerSetRef CXNewIntegerSetFromIntersection(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet);
CXIntegerSetRef CXNewIntegerSetFromDifference(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet);
CXIntegerSetRef CXNewIntegerSetFromSymmetricDifference(const CXIntegerSetRef firtSet, const CXIntegerSetRef secondSet);
CXIntegerSetRef CXNewIntegerSetFromSet(const CXIntegerSetRef aSet);
CXBool CXIntegerSetIsSubsetOf(const CXIntegerSetRef subSet, const CXIntegerSetRef superSet);
CXBool CXIntegerSetIsSupersetOf(const CXIntegerSetRef superSet, const CXIntegerSetRef subSet);

CX_INLINE CXSize CXIntegerSetCount(CXIntegerSetRef aSet){
	return (CXSize)HASH_COUNT((*aSet));
}



typedef struct {
	CXUInteger element;
	UT_hash_handle hh;
} CXUIntegerSetEntry;

typedef CXUIntegerSetEntry* CXUIntegerSet;
typedef CXUIntegerSet* CXUIntegerSetRef;

#define CXUIntegerSetFOR(setEntry,set) for(CXUIntegerSetEntry* setEntry=*set; setEntry != NULL; setEntry=setEntry->hh.next)


CXUIntegerSetRef CXNewUIntegerSet();
CXBool CXUIntegerSetHas(const CXUIntegerSetRef set, const CXUInteger key);
void CXUIntegerSetAdd(CXUIntegerSetRef set, const CXUInteger element);
void CXUIntegerSetRemove(CXUIntegerSetRef set, const CXUInteger element);
void CXUIntegerSetClear(CXUIntegerSetRef set);
void CXUIntegerSetDestroy(CXUIntegerSetRef set);
CXUIntegerSetRef CXNewUIntegerSetFromUnion(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet);
void CXUIntegerSetUnion(CXUIntegerSetRef destinationSet, const CXUIntegerSetRef unionSet);
CXUIntegerSetRef CXNewUIntegerSetFromIntersection(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet);
CXUIntegerSetRef CXNewUIntegerSetFromDifference(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet);
CXUIntegerSetRef CXNewUIntegerSetFromSymmetricDifference(const CXUIntegerSetRef firtSet, const CXUIntegerSetRef secondSet);
CXUIntegerSetRef CXNewUIntegerSetFromSet(const CXUIntegerSetRef aSet);
CXBool CXUIntegerSetIsSubsetOf(const CXUIntegerSetRef subSet, const CXUIntegerSetRef superSet);
CXBool CXUIntegerSetIsSupersetOf(const CXUIntegerSetRef superSet, const CXUIntegerSetRef subSet);

CX_INLINE CXSize CXUIntegerSetCount(CXUIntegerSetRef aSet){
	return (CXSize)HASH_COUNT((*aSet));
}





typedef struct {
	CXEdge element;
	UT_hash_handle hh;
} CXEdgeSetEntry;

typedef CXEdgeSetEntry* CXEdgeSet;
typedef CXEdgeSet* CXEdgeSetRef;

#define CXEdgeSetFOR(setEntry,set) for(CXEdgeSetEntry* setEntry=*set; setEntry != NULL; setEntry=setEntry->hh.next)

CXEdgeSetRef CXNewEdgeSet();
CXBool CXEdgeSetHas(const CXEdgeSetRef set, const CXEdge key);
void CXEdgeSetAdd(CXEdgeSetRef set, const CXEdge element);
void CXEdgeSetRemove(CXEdgeSetRef set, const CXEdge element);
void CXEdgeSetClear(CXEdgeSetRef set);
void CXEdgeSetDestroy(CXEdgeSetRef set);
CXEdgeSetRef CXNewEdgeSetFromUnion(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet);
void CXEdgeSetUnion(CXEdgeSetRef destinationSet, const CXEdgeSetRef unionSet);
CXEdgeSetRef CXNewEdgeSetFromIntersection(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet);
CXEdgeSetRef CXNewEdgeSetFromDifference(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet);
CXEdgeSetRef CXNewEdgeSetFromSymmetricDifference(const CXEdgeSetRef firtSet, const CXEdgeSetRef secondSet);
CXEdgeSetRef CXNewEdgeSetFromSet(const CXEdgeSetRef aSet);
CXBool CXEdgeSetIsSubsetOf(const CXEdgeSetRef subSet, const CXEdgeSetRef superSet);
CXBool CXEdgeSetIsSupersetOf(const CXEdgeSetRef superSet, const CXEdgeSetRef subSet);

CX_INLINE CXSize CXEdgeSetCount(CXEdgeSetRef aSet){
	return (CXSize)HASH_COUNT((*aSet));
}



typedef struct {
	void* element;
	CXSize elementSize;
	UT_hash_handle hh;
} CXGenericSetEntry;

typedef CXGenericSetEntry* CXGenericSet;
typedef CXGenericSet* CXGenericSetRef;

#define CXGenericSetFOR(setEntry,set) for(CXGenericSetEntry* setEntry=*set; setEntry != NULL; setEntry=setEntry->hh.next)



#endif /* CXSet_h */
