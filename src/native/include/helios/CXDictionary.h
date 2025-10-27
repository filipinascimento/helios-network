//
//  CXDictionary.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/5/16.
//  Copyright © 2016 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXDictionary_h
#define CXDictionary_h

#include "uthash.h"
#include "CXCommons.h"


typedef struct {
	char* key;
	void* data;
	UT_hash_handle hh;
} CXStringDictionaryEntry;

typedef CXStringDictionaryEntry* CXStringDictionary;
typedef CXStringDictionary* CXStringDictionaryRef;

#define CXStringDictionaryFOR(dictionaryEntry,dictionary) for(CXStringDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

CXStringDictionaryRef CXNewStringDictionary();
void* CXStringDictionaryEntryForKey(const CXStringDictionaryRef dictionary, const CXString key);
void* CXStringDictionarySetEntry(CXStringDictionaryRef dictionary, const CXString key, void* data);
void* CXStringDictionaryDeleteEntry(CXStringDictionaryRef dictionary, const CXString key);
void* CXStringDictionaryDeleteAndFreeEntry(CXStringDictionaryRef dictionary, const CXString key);
void CXStringDictionaryClear(CXStringDictionaryRef dictionary);
void CXStringDictionaryClearAndFree(CXStringDictionaryRef dictionary);
void CXStringDictionaryDestroy(CXStringDictionaryRef dictionary);

CX_INLINE CXSize CXStringDictionaryCount(CXStringDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}



typedef struct {
	CXUInteger key;
	void* data;
	UT_hash_handle hh;
} CXUIntegerDictionaryEntry;

typedef CXUIntegerDictionaryEntry* CXUIntegerDictionary;
typedef CXUIntegerDictionary* CXUIntegerDictionaryRef;

#define CXUIntegerDictionaryFOR(dictionaryEntry,dictionary) for(CXUIntegerDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

CXUIntegerDictionaryRef CXNewUIntegerDictionary();
void* CXUIntegerDictionaryEntryForKey(const CXUIntegerDictionaryRef dictionary, const CXUInteger key);
void* CXUIntegerDictionarySetEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key, void* data);
void* CXUIntegerDictionaryDeleteEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key);
void* CXUIntegerDictionaryDeleteAndFreeEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key);
void CXUIntegerDictionaryClear(CXUIntegerDictionaryRef dictionary);
void CXUIntegerDictionaryClearAndFree(CXUIntegerDictionaryRef dictionary);
void CXUIntegerDictionaryDestroy(CXUIntegerDictionaryRef dictionary);

CX_INLINE CXSize CXUIntegerDictionaryCount(CXUIntegerDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}



typedef struct {
	CXInteger key;
	void* data;
	UT_hash_handle hh;
} CXIntegerDictionaryEntry;

typedef CXIntegerDictionaryEntry* CXIntegerDictionary;
typedef CXIntegerDictionary* CXIntegerDictionaryRef;

#define CXIntegerDictionaryFOR(dictionaryEntry,dictionary) for(CXIntegerDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

CXIntegerDictionaryRef CXNewIntegerDictionary();
void* CXIntegerDictionaryEntryForKey(const CXIntegerDictionaryRef dictionary, const CXInteger key);
void* CXIntegerDictionarySetEntry(CXIntegerDictionaryRef dictionary, const CXInteger key, void* data);
void* CXIntegerDictionaryDeleteEntry(CXIntegerDictionaryRef dictionary, const CXInteger key);
void* CXIntegerDictionaryDeleteAndFreeEntry(CXIntegerDictionaryRef dictionary, const CXInteger key);
void CXIntegerDictionaryClear(CXIntegerDictionaryRef dictionary);
void CXIntegerDictionaryClearAndFree(CXIntegerDictionaryRef dictionary);
void CXIntegerDictionaryDestroy(CXIntegerDictionaryRef dictionary);

CX_INLINE CXSize CXIntegerDictionaryCount(CXIntegerDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}



// typedef struct {
// 	CXEdge key;
// 	void* data;
// 	UT_hash_handle hh;
// } CXEdgeDictionaryEntry;

// typedef CXEdgeDictionaryEntry* CXEdgeDictionary;
// typedef CXEdgeDictionary* CXEdgeDictionaryRef;

// #define CXEdgeDictionaryFOR(dictionaryEntry,dictionary) for(CXEdgeDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)


// CXEdgeDictionaryRef CXNewEdgeDictionary();
// void* CXEdgeDictionaryEntryForKey(const CXEdgeDictionaryRef dictionary, const CXEdge key);
// void* CXEdgeDictionarySetEntry(CXEdgeDictionaryRef dictionary, const CXEdge key, void* data);
// void* CXEdgeDictionaryDeleteEntry(CXEdgeDictionaryRef dictionary, const CXEdge key);
// void* CXEdgeDictionaryDeleteAndFreeEntry(CXEdgeDictionaryRef dictionary, const CXEdge key);
// void CXEdgeDictionaryClear(CXEdgeDictionaryRef dictionary);
// void CXEdgeDictionaryClearAndFree(CXEdgeDictionaryRef dictionary);
// void CXEdgeDictionaryDestroy(CXEdgeDictionaryRef dictionary);

// CX_INLINE CXSize CXEdgeDictionaryCount(CXEdgeDictionaryRef dictionary){
// 	return (CXSize)HASH_COUNT((*dictionary));
// }



typedef struct {
	void* key;
	void* data;
	UT_hash_handle hh;
} CXGenericDictionaryEntry;

typedef CXGenericDictionaryEntry* CXGenericDictionary;
typedef CXGenericDictionary* CXGenericDictionaryRef;

#define CXGenericDictionaryFOR(dictionaryEntry,dictionary) for(CXGenericDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

CXGenericDictionaryRef CXNewGenericDictionary();
void* CXGenericDictionarySetEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize, void* data);
void* CXGenericDictionaryDeleteEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize);
void* CXGenericDictionaryDeleteAndFreeEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize);
void CXGenericDictionaryClear(CXGenericDictionaryRef dictionary);
void CXGenericDictionaryClearAndFree(CXGenericDictionaryRef dictionary);
void CXGenericDictionaryDestroy(CXGenericDictionaryRef dictionary);

CX_INLINE CXSize CXGenericDictionaryCount(CXGenericDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}

#endif /* CXDictionary_h */
