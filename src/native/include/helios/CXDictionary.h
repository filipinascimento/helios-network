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


/**
 * Entry used by a string keyed dictionary (powered by uthash).
 */
typedef struct {
	char* key;
	void* data;
	UT_hash_handle hh;
} CXStringDictionaryEntry;

typedef CXStringDictionaryEntry* CXStringDictionary;
typedef CXStringDictionary* CXStringDictionaryRef;

#define CXStringDictionaryFOR(dictionaryEntry,dictionary) for(CXStringDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

/** Allocates an empty string keyed dictionary. */
CXStringDictionaryRef CXNewStringDictionary();
/** Looks up the payload associated with `key`, returning NULL when missing. */
void* CXStringDictionaryEntryForKey(const CXStringDictionaryRef dictionary, const CXString key);
/** Inserts or overwrites the payload for `key`. Returns the previous entry. */
void* CXStringDictionarySetEntry(CXStringDictionaryRef dictionary, const CXString key, void* data);
/** Removes the entry for `key`, returning the stored payload (without freeing). */
void* CXStringDictionaryDeleteEntry(CXStringDictionaryRef dictionary, const CXString key);
/** Removes the entry for `key`, releasing the stored payload via `free()`. */
void* CXStringDictionaryDeleteAndFreeEntry(CXStringDictionaryRef dictionary, const CXString key);
/** Removes every entry but keeps the dictionary alive. */
void CXStringDictionaryClear(CXStringDictionaryRef dictionary);
/** Removes every entry and frees their payloads using `free()`. */
void CXStringDictionaryClearAndFree(CXStringDictionaryRef dictionary);
/** Destroys the dictionary and releases its storage. */
void CXStringDictionaryDestroy(CXStringDictionaryRef dictionary);

CX_INLINE CXSize CXStringDictionaryCount(CXStringDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}



/** Entry used by an unsigned integer keyed dictionary. */
typedef struct {
	CXUInteger key;
	void* data;
	UT_hash_handle hh;
} CXUIntegerDictionaryEntry;

typedef CXUIntegerDictionaryEntry* CXUIntegerDictionary;
typedef CXUIntegerDictionary* CXUIntegerDictionaryRef;

#define CXUIntegerDictionaryFOR(dictionaryEntry,dictionary) for(CXUIntegerDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

/** Allocates an empty unsigned integer keyed dictionary. */
CXUIntegerDictionaryRef CXNewUIntegerDictionary();
/** Fetches the payload stored under `key`. */
void* CXUIntegerDictionaryEntryForKey(const CXUIntegerDictionaryRef dictionary, const CXUInteger key);
/** Upserts the payload for `key`, returning the previous value. */
void* CXUIntegerDictionarySetEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key, void* data);
/** Removes the entry for `key` without freeing the payload. */
void* CXUIntegerDictionaryDeleteEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key);
/** Removes the entry for `key` and frees the payload with `free()`. */
void* CXUIntegerDictionaryDeleteAndFreeEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key);
/** Deletes all entries but keeps the dictionary allocated. */
void CXUIntegerDictionaryClear(CXUIntegerDictionaryRef dictionary);
/** Deletes all entries and frees each payload. */
void CXUIntegerDictionaryClearAndFree(CXUIntegerDictionaryRef dictionary);
/** Releases the dictionary and all internal storage. */
void CXUIntegerDictionaryDestroy(CXUIntegerDictionaryRef dictionary);

CX_INLINE CXSize CXUIntegerDictionaryCount(CXUIntegerDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}



/** Entry used by a signed integer keyed dictionary. */
typedef struct {
	CXInteger key;
	void* data;
	UT_hash_handle hh;
} CXIntegerDictionaryEntry;

typedef CXIntegerDictionaryEntry* CXIntegerDictionary;
typedef CXIntegerDictionary* CXIntegerDictionaryRef;

#define CXIntegerDictionaryFOR(dictionaryEntry,dictionary) for(CXIntegerDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

/** Allocates an empty signed integer keyed dictionary. */
CXIntegerDictionaryRef CXNewIntegerDictionary();
/** Fetches the payload stored under `key`. */
void* CXIntegerDictionaryEntryForKey(const CXIntegerDictionaryRef dictionary, const CXInteger key);
/** Upserts the payload for `key`, returning the previous value. */
void* CXIntegerDictionarySetEntry(CXIntegerDictionaryRef dictionary, const CXInteger key, void* data);
/** Removes the entry for `key` without freeing the payload. */
void* CXIntegerDictionaryDeleteEntry(CXIntegerDictionaryRef dictionary, const CXInteger key);
/** Removes the entry for `key` and frees the payload with `free()`. */
void* CXIntegerDictionaryDeleteAndFreeEntry(CXIntegerDictionaryRef dictionary, const CXInteger key);
/** Deletes all entries but keeps the dictionary allocated. */
void CXIntegerDictionaryClear(CXIntegerDictionaryRef dictionary);
/** Deletes all entries and frees each payload. */
void CXIntegerDictionaryClearAndFree(CXIntegerDictionaryRef dictionary);
/** Releases the dictionary and all internal storage. */
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



/** Entry used by a dictionary with binary keys. */
typedef struct {
	void* key;
	void* data;
	UT_hash_handle hh;
} CXGenericDictionaryEntry;

typedef CXGenericDictionaryEntry* CXGenericDictionary;
typedef CXGenericDictionary* CXGenericDictionaryRef;

#define CXGenericDictionaryFOR(dictionaryEntry,dictionary) for(CXGenericDictionaryEntry* dictionaryEntry=*dictionary; dictionaryEntry != NULL; dictionaryEntry=dictionaryEntry->hh.next)

/** Allocates an empty dictionary that accepts arbitrary binary keys. */
CXGenericDictionaryRef CXNewGenericDictionary();
/** Upserts the payload for a binary key, returning the previous value. */
void* CXGenericDictionarySetEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize, void* data);
/** Removes the entry for a binary key without freeing the payload. */
void* CXGenericDictionaryDeleteEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize);
/** Removes the entry for a binary key and frees the payload with `free()`. */
void* CXGenericDictionaryDeleteAndFreeEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize);
/** Deletes all entries but preserves the backing allocation. */
void CXGenericDictionaryClear(CXGenericDictionaryRef dictionary);
/** Deletes all entries and frees their payloads. */
void CXGenericDictionaryClearAndFree(CXGenericDictionaryRef dictionary);
/** Releases the dictionary and its storage. */
void CXGenericDictionaryDestroy(CXGenericDictionaryRef dictionary);

CX_INLINE CXSize CXGenericDictionaryCount(CXGenericDictionaryRef dictionary){
	return (CXSize)HASH_COUNT((*dictionary));
}

#endif /* CXDictionary_h */
