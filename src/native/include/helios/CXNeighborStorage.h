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

/**
 * Selects which storage backend should be used for a neighbour container.
 * - `CXNeighborListType`: contiguous arrays, efficient for low-degree nodes.
 * - `CXNeighborMapType`: hash maps, efficient for high-degree nodes or when
 *    multiplicity information is required.
 */
typedef enum {
	CXNeighborListType = 0,
	CXNeighborMapType  = 1
} CXNeighborStorageType;

/** Packed adjacency list backed by parallel arrays of nodes and edge ids. */
typedef struct {
	CXIndex *nodes;
	CXIndex *edges;
	CXSize count;
	CXSize capacity;
} CXNeighborList;

/** Hash-map backed adjacency store supporting multiplicity bookkeeping. */
typedef struct {
	CXUIntegerDictionaryRef edgeToNode;         // edge index -> CXIndex* (neighbour)
	CXUIntegerDictionaryRef nodeToMultiplicity; // neighbour node -> CXUInteger* (edge multiplicity)
} CXNeighborMap;

/** Tagged union describing the storage strategy used by a node. */
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
/** Initializes a neighbour list with the requested capacity. */
CXBool CXNeighborListInit(CXNeighborList *list, CXSize initialCapacity);
/** Releases heap memory owned by the list. */
void CXNeighborListFree(CXNeighborList *list);
/** Ensures the list can store at least `requiredCapacity` entries. */
CXBool CXNeighborListEnsureCapacity(CXNeighborList *list, CXSize requiredCapacity);
/** Pushes a neighbour entry (node,edge) into the list. */
CXBool CXNeighborListAdd(CXNeighborList *list, CXIndex node, CXIndex edge);
/** Removes any edges referenced in `edgeSet` from the list. */
CXBool CXNeighborListRemoveEdgesFromSet(CXNeighborList *list, CXUIntegerSetRef edgeSet);
/** Removes a batch of edges supplied as an array of indices. */
CXBool CXNeighborListRemoveEdgesFromArray(CXNeighborList *list, const CXIndex *edgeArray, CXSize edgeCount);
/** Copies neighbour node ids into `outNodes`, returning how many were written. */
CXSize CXNeighborListGetNodes(const CXNeighborList *list, CXIndex *outNodes, CXSize maxCount);
/** Copies edge ids into `outEdges`, returning how many were written. */
CXSize CXNeighborListGetEdges(const CXNeighborList *list, CXIndex *outEdges, CXSize maxCount);

// Map primitives -------------------------------------------------------------
/** Initializes the hash-map backed adjacency store. */
CXBool CXNeighborMapInit(CXNeighborMap *map);
/** Releases all resources held by the map. */
void CXNeighborMapFree(CXNeighborMap *map);
/** Adds a neighbour (node,edge) pair to the map. */
CXBool CXNeighborMapAdd(CXNeighborMap *map, CXIndex node, CXIndex edge);
/** Removes any edges referenced in `edgeSet` from the map. */
CXBool CXNeighborMapRemoveEdgesFromSet(CXNeighborMap *map, CXUIntegerSetRef edgeSet);
/** Removes a batch of edges supplied as an array of indices. */
CXBool CXNeighborMapRemoveEdgesFromArray(CXNeighborMap *map, const CXIndex *edgeArray, CXSize edgeCount);
/** Returns the total number of stored neighbours. */
CXSize CXNeighborMapCount(const CXNeighborMap *map);
/** Enumerates neighbour node ids into `outNodes`. */
CXSize CXNeighborMapGetNodes(const CXNeighborMap *map, CXIndex *outNodes, CXSize maxCount);
/** Enumerates edge ids into `outEdges`. */
CXSize CXNeighborMapGetEdges(const CXNeighborMap *map, CXIndex *outEdges, CXSize maxCount);

// Container helpers ----------------------------------------------------------
/** Initializes a container with the requested storage type. */
CXBool CXNeighborContainerInit(CXNeighborContainer *container, CXNeighborStorageType storageType, CXSize initialCapacity);
/** Releases any memory owned by the container. */
void CXNeighborContainerFree(CXNeighborContainer *container);
/** Adds a neighbour to the container, dispatching to the proper backend. */
CXBool CXNeighborContainerAdd(CXNeighborContainer *container, CXIndex node, CXIndex edge);
/** Removes edges present in `edgeSet` from the container. */
CXBool CXNeighborContainerRemoveEdgesFromSet(CXNeighborContainer *container, CXUIntegerSetRef edgeSet);
/** Removes a batch of edges supplied as an array. */
CXBool CXNeighborContainerRemoveEdgesFromArray(CXNeighborContainer *container, const CXIndex *edgeArray, CXSize edgeCount);
/** Returns the number of neighbours stored in the container. */
CXSize CXNeighborContainerCount(const CXNeighborContainer *container);
/** Copies neighbour node ids into `outNodes`. */
CXSize CXNeighborContainerGetNodes(const CXNeighborContainer *container, CXIndex *outNodes, CXSize maxCount);
/** Copies neighbour edge ids into `outEdges`. */
CXSize CXNeighborContainerGetEdges(const CXNeighborContainer *container, CXIndex *outEdges, CXSize maxCount);

// Iteration ------------------------------------------------------------------
/**
 * Prepares an iterator for traversal. The first call to `CXNeighborIteratorNext`
 * will advance to the first neighbour, so the iterator can be stack allocated.
 */
void CXNeighborIteratorInit(CXNeighborIterator *iterator, CXNeighborContainer *container);
/** Advances the iterator, returning CXFalse when traversal is finished. */
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
