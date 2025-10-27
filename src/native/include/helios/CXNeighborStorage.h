//
//  CXNeighborStorage.h
//  Helios Network Core
//
//  Abstractions for per-node neighbour storage supporting both packed list
//  representation and hash-map backed representation for heavy hitters.
//

#ifndef CXNetwork_CXNeighborStorage_h
#define CXNetwork_CXNeighborStorage_h

#include "CXCommons.h"
#include "CXDictionary.h"
#include "CXSet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CXNeighborListType = 0,
	CXNeighborMapType  = 1
} CXNeighborStorageType;

typedef struct {
	CXIndex *nodes;
	CXIndex *edges;
	CXSize count;
	CXSize capacity;
} CXNeighborList;

typedef struct {
	CXUIntegerDictionaryRef edgeToNode;         // edge index -> CXIndex* (neighbour)
	CXUIntegerDictionaryRef nodeToMultiplicity; // neighbour node -> CXUInteger* (edge multiplicity)
} CXNeighborMap;

typedef struct {
	CXNeighborStorageType storageType;
	union {
		CXNeighborList list;
		CXNeighborMap map;
	} storage;
} CXNeighborContainer;

typedef struct {
	CXSize index;
} CXNeighborListIterator;

typedef struct {
	CXUIntegerDictionaryEntry *entry;
} CXNeighborMapIterator;

typedef struct {
	CXNeighborContainer *container;
	CXIndex node;
	CXIndex edge;
	union {
		CXNeighborListIterator list;
		CXNeighborMapIterator map;
	} state;
} CXNeighborIterator;

// List primitives ------------------------------------------------------------
CXBool CXNeighborListInit(CXNeighborList *list, CXSize initialCapacity);
void CXNeighborListFree(CXNeighborList *list);
CXBool CXNeighborListEnsureCapacity(CXNeighborList *list, CXSize requiredCapacity);
CXBool CXNeighborListAdd(CXNeighborList *list, CXIndex node, CXIndex edge);
CXBool CXNeighborListRemoveEdgesFromSet(CXNeighborList *list, CXUIntegerSetRef edgeSet);
CXBool CXNeighborListRemoveEdgesFromArray(CXNeighborList *list, const CXIndex *edgeArray, CXSize edgeCount);
CXSize CXNeighborListGetNodes(const CXNeighborList *list, CXIndex *outNodes, CXSize maxCount);
CXSize CXNeighborListGetEdges(const CXNeighborList *list, CXIndex *outEdges, CXSize maxCount);

// Map primitives -------------------------------------------------------------
CXBool CXNeighborMapInit(CXNeighborMap *map);
void CXNeighborMapFree(CXNeighborMap *map);
CXBool CXNeighborMapAdd(CXNeighborMap *map, CXIndex node, CXIndex edge);
CXBool CXNeighborMapRemoveEdgesFromSet(CXNeighborMap *map, CXUIntegerSetRef edgeSet);
CXBool CXNeighborMapRemoveEdgesFromArray(CXNeighborMap *map, const CXIndex *edgeArray, CXSize edgeCount);
CXSize CXNeighborMapCount(const CXNeighborMap *map);
CXSize CXNeighborMapGetNodes(const CXNeighborMap *map, CXIndex *outNodes, CXSize maxCount);
CXSize CXNeighborMapGetEdges(const CXNeighborMap *map, CXIndex *outEdges, CXSize maxCount);

// Container helpers ----------------------------------------------------------
CXBool CXNeighborContainerInit(CXNeighborContainer *container, CXNeighborStorageType storageType, CXSize initialCapacity);
void CXNeighborContainerFree(CXNeighborContainer *container);
CXBool CXNeighborContainerAdd(CXNeighborContainer *container, CXIndex node, CXIndex edge);
CXBool CXNeighborContainerRemoveEdgesFromSet(CXNeighborContainer *container, CXUIntegerSetRef edgeSet);
CXBool CXNeighborContainerRemoveEdgesFromArray(CXNeighborContainer *container, const CXIndex *edgeArray, CXSize edgeCount);
CXSize CXNeighborContainerCount(const CXNeighborContainer *container);
CXSize CXNeighborContainerGetNodes(const CXNeighborContainer *container, CXIndex *outNodes, CXSize maxCount);
CXSize CXNeighborContainerGetEdges(const CXNeighborContainer *container, CXIndex *outEdges, CXSize maxCount);

// Iteration ------------------------------------------------------------------
void CXNeighborIteratorInit(CXNeighborIterator *iterator, CXNeighborContainer *container);
CXBool CXNeighborIteratorNext(CXNeighborIterator *iterator);

// Convenience macro for iteration
#define CXNeighborFOR(nodeVar, edgeVar, containerPtr) \
	for (CXNeighborIterator __it; \
		 CXNeighborIteratorInit(&__it, (containerPtr)), \
		 CXNeighborIteratorNext(&__it); ) \
		for (CXBool __once = CXTrue; __once; __once = CXFalse) \
			for (CXIndex nodeVar = __it.node, edgeVar = __it.edge; __once; __once = CXFalse)

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNeighborStorage_h */
