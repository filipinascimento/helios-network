//
//  CXDictionary.c
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/5/16.
//  Copyright © 2016 Filipi Nascimento Silva. All rights reserved.
//

#include "CXDictionary.h"


CXStringDictionaryRef CXNewStringDictionary(){
	return calloc(1, sizeof(CXStringDictionary));
}

void* CXStringDictionaryEntryForKey(const CXStringDictionaryRef dictionary, const CXString key){
	CXStringDictionaryEntry* entry = NULL;
	if(dictionary){
		HASH_FIND_STR((*dictionary), key, entry);
	}
	if(entry){
		return entry->data;
	}else{
		return NULL;
	}
}

void* CXStringDictionarySetEntry(CXStringDictionaryRef dictionary, const CXString key, void* data){
	CXStringDictionaryEntry* entry = calloc(1,sizeof(CXStringDictionaryEntry));
	entry->key = CXNewStringFromString(key);
	entry->data = data;
	CXStringDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND_STR((*dictionary), entry->key, entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->key);
		free(entryTemp);
	}
	HASH_ADD_KEYPTR( hh, (*dictionary), entry->key, strlen(entry->key), entry);
	return replacedData;
}

void* CXStringDictionaryDeleteEntry(CXStringDictionaryRef dictionary, const CXString key){
	CXStringDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND_STR((*dictionary), key, entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->key);
		free(entryTemp);
	}
	return replacedData;
}
void* CXStringDictionaryDeleteAndFreeEntry(CXStringDictionaryRef dictionary, const CXString key){
	CXStringDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND_STR((*dictionary), key, entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->key);
		free(entryTemp->data);
		free(entryTemp);
	}
	return replacedData;
}

void CXStringDictionaryClear(CXStringDictionaryRef dictionary){
	CXStringDictionaryEntry* entry = NULL;
	CXStringDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry->key);
		free(entry);
	}
}

void CXStringDictionaryClearAndFree(CXStringDictionaryRef dictionary){
	CXStringDictionaryEntry* entry = NULL;
	CXStringDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry->data);
		free(entry->key);
		free(entry);
	}
}

void CXStringDictionaryDestroy(CXStringDictionaryRef dictionary){
	CXStringDictionaryClear(dictionary);
	free(dictionary);
}




/////// CXUInteger

CXUIntegerDictionaryRef CXNewUIntegerDictionary(){
	return calloc(1, sizeof(CXUIntegerDictionary));
}

void* CXUIntegerDictionaryEntryForKey(const CXUIntegerDictionaryRef dictionary, const CXUInteger key){
	CXUIntegerDictionaryEntry* entry = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), &key, sizeof(CXUInteger), entry);
	}
	if(entry){
		return entry->data;
	}else{
		return NULL;
	}
}

void* CXUIntegerDictionarySetEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key, void* data){
	CXUIntegerDictionaryEntry* entry = calloc(1,sizeof(CXUIntegerDictionaryEntry));
	entry->key = key;
	entry->data = data;
	CXUIntegerDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), &(entry->key), sizeof(CXUInteger), entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp);
	}
	HASH_ADD_KEYPTR( hh, (*dictionary), (&entry->key), sizeof(CXUInteger), entry);
	return replacedData;
}

void* CXUIntegerDictionaryDeleteEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key){
	CXUIntegerDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), (&key), sizeof(CXUInteger), entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp);
	}
	return replacedData;
}

void* CXUIntegerDictionaryDeleteAndFreeEntry(CXUIntegerDictionaryRef dictionary, const CXUInteger key){
	CXUIntegerDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), (&key), sizeof(CXUInteger), entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->data);
		free(entryTemp);
	}
	return replacedData;
}

void CXUIntegerDictionaryClear(CXUIntegerDictionaryRef dictionary){
	CXUIntegerDictionaryEntry* entry = NULL;
	CXUIntegerDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry);
	}
}

void CXUIntegerDictionaryClearAndFree(CXUIntegerDictionaryRef dictionary){
	CXUIntegerDictionaryEntry* entry = NULL;
	CXUIntegerDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry->data);
		free(entry);
	}
}

void CXUIntegerDictionaryDestroy(CXUIntegerDictionaryRef dictionary){
	CXUIntegerDictionaryClear(dictionary);
	free(dictionary);
}



/////// CXInteger

CXIntegerDictionaryRef CXNewIntegerDictionary(){
	return calloc(1, sizeof(CXIntegerDictionary));
}

void* CXIntegerDictionaryEntryForKey(const CXIntegerDictionaryRef dictionary, const CXInteger key){
	CXIntegerDictionaryEntry* entry = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), (&key), sizeof(CXInteger), entry);
	}
	if(entry){
		return entry->data;
	}else{
		return NULL;
	}
}

void* CXIntegerDictionarySetEntry(CXIntegerDictionaryRef dictionary, const CXInteger key, void* data){
	CXIntegerDictionaryEntry* entry = calloc(1,sizeof(CXIntegerDictionaryEntry));
	entry->key = key;
	entry->data = data;
	CXIntegerDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), &(entry->key), sizeof(CXInteger), entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp);
	}
	HASH_ADD_KEYPTR( hh, (*dictionary), &(entry->key), sizeof(CXInteger), entry);
	return replacedData;
}

void* CXIntegerDictionaryDeleteEntry(CXIntegerDictionaryRef dictionary, const CXInteger key){
	CXIntegerDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), &(key), sizeof(CXInteger), entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp);
	}
	return replacedData;
}

void* CXIntegerDictionaryDeleteAndFreeEntry(CXIntegerDictionaryRef dictionary, const CXInteger key){
	CXIntegerDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), &(key), sizeof(CXInteger), entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->data);
		free(entryTemp);
	}
	return replacedData;
}

void CXIntegerDictionaryClear(CXIntegerDictionaryRef dictionary){
	CXIntegerDictionaryEntry* entry = NULL;
	CXIntegerDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry);
	}
}

void CXIntegerDictionaryClearAndFree(CXIntegerDictionaryRef dictionary){
	CXIntegerDictionaryEntry* entry = NULL;
	CXIntegerDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry->data);
		free(entry);
	}
}

void CXIntegerDictionaryDestroy(CXIntegerDictionaryRef dictionary){
	CXIntegerDictionaryClear(dictionary);
	free(dictionary);
}




/////// CXEdge

// CXEdgeDictionaryRef CXNewEdgeDictionary(){
// 	return calloc(1, sizeof(CXEdgeDictionary));
// }

// void* CXEdgeDictionaryEntryForKey(const CXEdgeDictionaryRef dictionary, const CXEdge key){
// 	CXEdgeDictionaryEntry* entry = NULL;
// 	if(dictionary){
// 		HASH_FIND(hh, (*dictionary), (&key), sizeof(CXEdge), entry);
// 	}
// 	if(entry){
// 		return entry->data;
// 	}else{
// 		return NULL;
// 	}
// }

// void* CXEdgeDictionarySetEntry(CXEdgeDictionaryRef dictionary, const CXEdge key, void* data){
// 	CXEdgeDictionaryEntry* entry = calloc(1,sizeof(CXEdgeDictionaryEntry));
// 	entry->key = key;
// 	entry->data = data;
// 	CXEdgeDictionaryEntry* entryTemp = NULL;
// 	void* replacedData = NULL;
// 	if(dictionary){
// 		HASH_FIND(hh, (*dictionary), &(entry->key), sizeof(CXEdge), entryTemp);
// 	}
// 	if(entryTemp){
// 		replacedData = entryTemp->data;
// 		HASH_DEL((*dictionary), entryTemp);
// 		free(entryTemp);
// 	}
// 	HASH_ADD_KEYPTR( hh, (*dictionary), &(entry->key), sizeof(CXEdge), entry);
// 	return replacedData;
// }

// void* CXEdgeDictionaryDeleteEntry(CXEdgeDictionaryRef dictionary, const CXEdge key){
// 	CXEdgeDictionaryEntry* entryTemp = NULL;
// 	void* replacedData = NULL;
// 	if(dictionary){
// 		HASH_FIND(hh, (*dictionary), (&key), sizeof(CXEdge), entryTemp);
// 	}
// 	if(entryTemp){
// 		replacedData = entryTemp->data;
// 		HASH_DEL((*dictionary), entryTemp);
// 		free(entryTemp);
// 	}
// 	return replacedData;
// }

// void* CXEdgeDictionaryDeleteAndFreeEntry(CXEdgeDictionaryRef dictionary, const CXEdge key){
// 	CXEdgeDictionaryEntry* entryTemp = NULL;
// 	void* replacedData = NULL;
// 	if(dictionary){
// 		HASH_FIND(hh, (*dictionary), (&key), sizeof(CXEdge), entryTemp);
// 	}
// 	if(entryTemp){
// 		replacedData = entryTemp->data;
// 		HASH_DEL((*dictionary), entryTemp);
// 		free(entryTemp->data);
// 		free(entryTemp);
// 	}
// 	return replacedData;
// }

// void CXEdgeDictionaryClear(CXEdgeDictionaryRef dictionary){
// 	CXEdgeDictionaryEntry* entry = NULL;
// 	CXEdgeDictionaryEntry* entryTemp = NULL;
// 	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
// 		HASH_DEL((*dictionary), entry);
// 		free(entry);
// 	}
// }

// void CXEdgeDictionaryClearAndFree(CXEdgeDictionaryRef dictionary){
// 	CXEdgeDictionaryEntry* entry = NULL;
// 	CXEdgeDictionaryEntry* entryTemp = NULL;
// 	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
// 		HASH_DEL((*dictionary), entry);
// 		free(entry->data);
// 		free(entry);
// 	}
// }

// void CXEdgeDictionaryDestroy(CXEdgeDictionaryRef dictionary){
// 	CXEdgeDictionaryClear(dictionary);
// 	free(dictionary);
// }






/////// CXGeneric

CXGenericDictionaryRef CXNewGenericDictionary(){
	return calloc(1, sizeof(CXGenericDictionary));
}

void* CXGenericDictionaryEntryForKey(const CXGenericDictionaryRef dictionary, const void* key, CXSize keysize){
	CXGenericDictionaryEntry* entry = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), key, keysize, entry);
	}
	if(entry){
		return entry->data;
	}else{
		return NULL;
	}
}

void* CXGenericDictionarySetEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize, void* data){
	CXGenericDictionaryEntry* entry = calloc(1,sizeof(CXGenericDictionaryEntry));
	entry->key = calloc(1, keysize);
	entry->data = data;
	memcpy(entry->key, key, keysize);
	CXGenericDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), (entry->key), keysize, entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->key);
		free(entryTemp);
	}
	HASH_ADD_KEYPTR( hh, (*dictionary), (entry->key), keysize, entry);
	return replacedData;
}

void* CXGenericDictionaryDeleteEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize){
	CXGenericDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), key, keysize, entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->key);
		free(entryTemp);
	}
	return replacedData;
}

void* CXGenericDictionaryDeleteAndFreeEntry(CXGenericDictionaryRef dictionary, const void* key, CXSize keysize){
	CXGenericDictionaryEntry* entryTemp = NULL;
	void* replacedData = NULL;
	if(dictionary){
		HASH_FIND(hh, (*dictionary), key, keysize, entryTemp);
	}
	if(entryTemp){
		replacedData = entryTemp->data;
		HASH_DEL((*dictionary), entryTemp);
		free(entryTemp->data);
		free(entryTemp->key);
		free(entryTemp);
	}
	return replacedData;
}

void CXGenericDictionaryClear(CXGenericDictionaryRef dictionary){
	CXGenericDictionaryEntry* entry = NULL;
	CXGenericDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry->key);
		free(entry);
	}
}

void CXGenericDictionaryClearAndFree(CXGenericDictionaryRef dictionary){
	CXGenericDictionaryEntry* entry = NULL;
	CXGenericDictionaryEntry* entryTemp = NULL;
	HASH_ITER(hh, (*dictionary), entry, entryTemp) {
		HASH_DEL((*dictionary), entry);
		free(entry->data);
		free(entry->key);
		free(entry);
	}
}

void CXGenericDictionaryDestroy(CXGenericDictionaryRef dictionary){
	CXGenericDictionaryClear(dictionary);
	free(dictionary);
}


