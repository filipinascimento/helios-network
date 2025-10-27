#include "CXNeighborStorage.h"

/** Removes bookkeeping for a single edge from the map-backed container. */
static CXBool CXNeighborMapRemoveEdgeInternal(CXNeighborMap *map, CXIndex edge);

// -----------------------------------------------------------------------------
// Neighbor list
// -----------------------------------------------------------------------------

/** Implementation of the list-based neighbour initialiser. */
CXBool CXNeighborListInit(CXNeighborList *list, CXSize initialCapacity) {
	if (!list) {
		return CXFalse;
	}
	list->nodes = NULL;
	list->edges = NULL;
	list->count = 0;
	list->capacity = 0;

	if (initialCapacity > 0) {
		list->nodes = calloc(initialCapacity, sizeof(CXIndex));
		list->edges = calloc(initialCapacity, sizeof(CXIndex));
		if (!list->nodes || !list->edges) {
			CXNeighborListFree(list);
			return CXFalse;
		}
		list->capacity = initialCapacity;
	}
	return CXTrue;
}

/** Releases memory held by the list-backed storage. */
void CXNeighborListFree(CXNeighborList *list) {
	if (!list) {
		return;
	}
	if (list->nodes) {
		free(list->nodes);
		list->nodes = NULL;
	}
	if (list->edges) {
		free(list->edges);
		list->edges = NULL;
	}
	list->count = 0;
	list->capacity = 0;
}

/** Grows the list-backed storage to at least `requiredCapacity`. */
CXBool CXNeighborListEnsureCapacity(CXNeighborList *list, CXSize requiredCapacity) {
	if (!list) {
		return CXFalse;
	}
	if (requiredCapacity <= list->capacity) {
		return CXTrue;
	}

	CXSize newCapacity = list->capacity > 0 ? list->capacity : 4;
	while (newCapacity < requiredCapacity) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < requiredCapacity) {
			newCapacity = requiredCapacity;
		}
	}

	CXIndex *newNodes = malloc(sizeof(CXIndex) * newCapacity);
	CXIndex *newEdges = malloc(sizeof(CXIndex) * newCapacity);
	if (!newNodes || !newEdges) {
		free(newNodes);
		free(newEdges);
		return CXFalse;
	}

	if (list->count > 0) {
		memcpy(newNodes, list->nodes, sizeof(CXIndex) * list->count);
		memcpy(newEdges, list->edges, sizeof(CXIndex) * list->count);
	}

	free(list->nodes);
	free(list->edges);
	list->nodes = newNodes;
	list->edges = newEdges;
	list->capacity = newCapacity;
	return CXTrue;
}

/** Adds a neighbour pair to the list-backed storage. */
CXBool CXNeighborListAdd(CXNeighborList *list, CXIndex node, CXIndex edge) {
	if (!CXNeighborListEnsureCapacity(list, list->count + 1)) {
		return CXFalse;
	}
	list->nodes[list->count] = node;
	list->edges[list->count] = edge;
	list->count++;
	return CXTrue;
}

/** Removes every edge present in the supplied set. */
CXBool CXNeighborListRemoveEdgesFromSet(CXNeighborList *list, CXUIntegerSetRef edgeSet) {
	if (!list || !edgeSet) {
		return CXFalse;
	}
	CXSize writeIndex = 0;
	for (CXSize readIndex = 0; readIndex < list->count; readIndex++) {
		CXIndex edge = list->edges[readIndex];
		if (!CXUIntegerSetHas(edgeSet, (CXUInteger)edge)) {
			list->nodes[writeIndex] = list->nodes[readIndex];
			list->edges[writeIndex] = edge;
			writeIndex++;
		}
	}
	list->count = writeIndex;
	return CXTrue;
}

/** Removes any edges that appear in the provided array. */
CXBool CXNeighborListRemoveEdgesFromArray(CXNeighborList *list, const CXIndex *edgeArray, CXSize edgeCount) {
	if (!list || !edgeArray) {
		return CXFalse;
	}
	CXSize writeIndex = 0;
	for (CXSize readIndex = 0; readIndex < list->count; readIndex++) {
		CXIndex edge = list->edges[readIndex];
		CXBool keep = CXTrue;
		for (CXSize i = 0; i < edgeCount; i++) {
			if (edge == edgeArray[i]) {
				keep = CXFalse;
				break;
			}
		}
		if (keep) {
			list->nodes[writeIndex] = list->nodes[readIndex];
			list->edges[writeIndex] = edge;
			writeIndex++;
		}
	}
	list->count = writeIndex;
	return CXTrue;
}

/** Copies neighbour node ids into the caller-provided buffer. */
CXSize CXNeighborListGetNodes(const CXNeighborList *list, CXIndex *outNodes, CXSize maxCount) {
	if (!list) {
		return 0;
	}
	if (!outNodes || maxCount == 0) {
		return list->count;
	}
	CXSize copyCount = CXMIN(list->count, maxCount);
	memcpy(outNodes, list->nodes, sizeof(CXIndex) * copyCount);
	return copyCount;
}

/** Copies neighbour edge ids into the caller-provided buffer. */
CXSize CXNeighborListGetEdges(const CXNeighborList *list, CXIndex *outEdges, CXSize maxCount) {
	if (!list) {
		return 0;
	}
	if (!outEdges || maxCount == 0) {
		return list->count;
	}
	CXSize copyCount = CXMIN(list->count, maxCount);
	memcpy(outEdges, list->edges, sizeof(CXIndex) * copyCount);
	return copyCount;
}

// -----------------------------------------------------------------------------
// Neighbor map
// -----------------------------------------------------------------------------

/** Initializes hash tables used by the map-backed representation. */
CXBool CXNeighborMapInit(CXNeighborMap *map) {
	if (!map) {
		return CXFalse;
	}
	map->edgeToNode = CXNewUIntegerDictionary();
	map->nodeToMultiplicity = CXNewUIntegerDictionary();
	if (!map->edgeToNode || !map->nodeToMultiplicity) {
		if (map->edgeToNode) {
			CXUIntegerDictionaryDestroy(map->edgeToNode);
			map->edgeToNode = NULL;
		}
		if (map->nodeToMultiplicity) {
			CXUIntegerDictionaryDestroy(map->nodeToMultiplicity);
			map->nodeToMultiplicity = NULL;
		}
		return CXFalse;
	}
	return CXTrue;
}

/** Releases hash table resources associated with the map-backed storage. */
void CXNeighborMapFree(CXNeighborMap *map) {
	if (!map) {
		return;
	}
	if (map->edgeToNode) {
		CXUIntegerDictionaryClearAndFree(map->edgeToNode);
		CXUIntegerDictionaryDestroy(map->edgeToNode);
		map->edgeToNode = NULL;
	}
	if (map->nodeToMultiplicity) {
		CXUIntegerDictionaryClearAndFree(map->nodeToMultiplicity);
		CXUIntegerDictionaryDestroy(map->nodeToMultiplicity);
		map->nodeToMultiplicity = NULL;
	}
}

/** Tracks how many edges reference the supplied neighbour node. */
static CXBool CXNeighborMapIncrementMultiplicity(CXNeighborMap *map, CXIndex node) {
	CXUIntegerDictionaryRef dict = map->nodeToMultiplicity;
	CXUInteger *countPtr = (CXUInteger *)CXUIntegerDictionaryEntryForKey(dict, (CXUInteger)node);
	if (!countPtr) {
		countPtr = calloc(1, sizeof(CXUInteger));
		if (!countPtr) {
			return CXFalse;
		}
		*countPtr = 1;
		CXUIntegerDictionarySetEntry(dict, (CXUInteger)node, countPtr);
	} else {
		(*countPtr)++;
	}
	return CXTrue;
}

/** Decrements the multiplicity counter, removing the entry when it hits zero. */
static void CXNeighborMapDecrementMultiplicity(CXNeighborMap *map, CXIndex node) {
	CXUIntegerDictionaryRef dict = map->nodeToMultiplicity;
	CXUInteger *countPtr = (CXUInteger *)CXUIntegerDictionaryEntryForKey(dict, (CXUInteger)node);
	if (!countPtr) {
		return;
	}
	if (*countPtr <= 1) {
		CXUIntegerDictionaryDeleteAndFreeEntry(dict, (CXUInteger)node);
	} else {
		(*countPtr)--;
	}
}

/** Helper that strips the provided edge from the map and updates counts. */
static CXBool CXNeighborMapRemoveEdgeInternal(CXNeighborMap *map, CXIndex edge) {
	if (!map || !map->edgeToNode) {
		return CXFalse;
	}
	CXIndex *nodePtr = (CXIndex *)CXUIntegerDictionaryEntryForKey(map->edgeToNode, (CXUInteger)edge);
	if (!nodePtr) {
		return CXFalse;
	}
	CXIndex node = *nodePtr;
	CXUIntegerDictionaryDeleteAndFreeEntry(map->edgeToNode, (CXUInteger)edge);
	CXNeighborMapDecrementMultiplicity(map, node);
	return CXTrue;
}

/** Inserts or updates the mapping for `edge`, pointing it at `node`. */
CXBool CXNeighborMapAdd(CXNeighborMap *map, CXIndex node, CXIndex edge) {
	if (!map) {
		return CXFalse;
	}

	// Remove existing mapping if the edge is being reused
	CXNeighborMapRemoveEdgeInternal(map, edge);

	CXIndex *nodePtr = calloc(1, sizeof(CXIndex));
	if (!nodePtr) {
		return CXFalse;
	}
	*nodePtr = node;
	CXUIntegerDictionarySetEntry(map->edgeToNode, (CXUInteger)edge, nodePtr);
	if (!CXNeighborMapIncrementMultiplicity(map, node)) {
		CXUIntegerDictionaryDeleteAndFreeEntry(map->edgeToNode, (CXUInteger)edge);
		return CXFalse;
	}
	return CXTrue;
}

/** Removes any edges referenced by the supplied set. */
CXBool CXNeighborMapRemoveEdgesFromSet(CXNeighborMap *map, CXUIntegerSetRef edgeSet) {
	if (!map || !edgeSet) {
		return CXFalse;
	}
	CXUIntegerSetFOR(entry, edgeSet) {
		CXNeighborMapRemoveEdgeInternal(map, (CXIndex)entry->element);
	}
	return CXTrue;
}

/** Removes any edges referenced by the supplied array. */
CXBool CXNeighborMapRemoveEdgesFromArray(CXNeighborMap *map, const CXIndex *edgeArray, CXSize edgeCount) {
	if (!map || !edgeArray) {
		return CXFalse;
	}
	for (CXSize i = 0; i < edgeCount; i++) {
		CXNeighborMapRemoveEdgeInternal(map, edgeArray[i]);
	}
	return CXTrue;
}

/** Returns how many edges are currently tracked in the map. */
CXSize CXNeighborMapCount(const CXNeighborMap *map) {
	if (!map || !map->edgeToNode) {
		return 0;
	}
	return CXUIntegerDictionaryCount(map->edgeToNode);
}

/** Copies neighbour nodes into `outNodes`, returning how many were written. */
CXSize CXNeighborMapGetNodes(const CXNeighborMap *map, CXIndex *outNodes, CXSize maxCount) {
	if (!map || !map->edgeToNode) {
		return 0;
	}
	CXSize total = CXNeighborMapCount(map);
	if (!outNodes || maxCount == 0) {
		return total;
	}
	CXSize copied = 0;
	CXUIntegerDictionaryFOR(entry, map->edgeToNode) {
		if (copied >= maxCount) {
			break;
		}
		CXIndex *nodePtr = (CXIndex *)entry->data;
		outNodes[copied++] = nodePtr ? *nodePtr : CXIndexMAX;
	}
	return copied;
}

/** Copies edge ids into `outEdges`, returning how many were written. */
CXSize CXNeighborMapGetEdges(const CXNeighborMap *map, CXIndex *outEdges, CXSize maxCount) {
	if (!map || !map->edgeToNode) {
		return 0;
	}
	CXSize total = CXNeighborMapCount(map);
	if (!outEdges || maxCount == 0) {
		return total;
	}
	CXSize copied = 0;
	CXUIntegerDictionaryFOR(entry, map->edgeToNode) {
		if (copied >= maxCount) {
			break;
		}
		outEdges[copied++] = (CXIndex)entry->key;
	}
	return copied;
}

// -----------------------------------------------------------------------------
// Container helpers
// -----------------------------------------------------------------------------

/** Initializes the container with either list or map storage. */
CXBool CXNeighborContainerInit(CXNeighborContainer *container, CXNeighborStorageType storageType, CXSize initialCapacity) {
	if (!container) {
		return CXFalse;
	}
	container->storageType = storageType;
	if (storageType == CXNeighborListType) {
		return CXNeighborListInit(&container->storage.list, initialCapacity);
	}
	return CXNeighborMapInit(&container->storage.map);
}

/** Releases internal storage for the container. */
void CXNeighborContainerFree(CXNeighborContainer *container) {
	if (!container) {
		return;
	}
	if (container->storageType == CXNeighborListType) {
		CXNeighborListFree(&container->storage.list);
	} else {
		CXNeighborMapFree(&container->storage.map);
	}
}

/** Adds an edge reference to the container. */
CXBool CXNeighborContainerAdd(CXNeighborContainer *container, CXIndex node, CXIndex edge) {
	if (!container) {
		return CXFalse;
	}
	if (container->storageType == CXNeighborListType) {
		return CXNeighborListAdd(&container->storage.list, node, edge);
	}
	return CXNeighborMapAdd(&container->storage.map, node, edge);
}

/** Removes edges referenced by the supplied set. */
CXBool CXNeighborContainerRemoveEdgesFromSet(CXNeighborContainer *container, CXUIntegerSetRef edgeSet) {
	if (!container) {
		return CXFalse;
	}
	if (container->storageType == CXNeighborListType) {
		return CXNeighborListRemoveEdgesFromSet(&container->storage.list, edgeSet);
	}
	return CXNeighborMapRemoveEdgesFromSet(&container->storage.map, edgeSet);
}

/** Removes edges referenced by the supplied array. */
CXBool CXNeighborContainerRemoveEdgesFromArray(CXNeighborContainer *container, const CXIndex *edgeArray, CXSize edgeCount) {
	if (!container) {
		return CXFalse;
	}
	if (container->storageType == CXNeighborListType) {
		return CXNeighborListRemoveEdgesFromArray(&container->storage.list, edgeArray, edgeCount);
	}
	return CXNeighborMapRemoveEdgesFromArray(&container->storage.map, edgeArray, edgeCount);
}

/** Returns how many neighbours are stored in the container. */
CXSize CXNeighborContainerCount(const CXNeighborContainer *container) {
	if (!container) {
		return 0;
	}
	if (container->storageType == CXNeighborListType) {
		return container->storage.list.count;
	}
	return CXNeighborMapCount(&container->storage.map);
}

/** Copies neighbour node ids, delegating to the active storage backend. */
CXSize CXNeighborContainerGetNodes(const CXNeighborContainer *container, CXIndex *outNodes, CXSize maxCount) {
	if (!container) {
		return 0;
	}
	if (container->storageType == CXNeighborListType) {
		return CXNeighborListGetNodes(&container->storage.list, outNodes, maxCount);
	}
	return CXNeighborMapGetNodes(&container->storage.map, outNodes, maxCount);
}

/** Copies edge ids, delegating to the active storage backend. */
CXSize CXNeighborContainerGetEdges(const CXNeighborContainer *container, CXIndex *outEdges, CXSize maxCount) {
	if (!container) {
		return 0;
	}
	if (container->storageType == CXNeighborListType) {
		return CXNeighborListGetEdges(&container->storage.list, outEdges, maxCount);
	}
	return CXNeighborMapGetEdges(&container->storage.map, outEdges, maxCount);
}

// -----------------------------------------------------------------------------
// Iteration
// -----------------------------------------------------------------------------

/** Prepares the iterator for traversal of the supplied container. */
void CXNeighborIteratorInit(CXNeighborIterator *iterator, CXNeighborContainer *container) {
	if (!iterator) {
		return;
	}
	iterator->container = container;
	iterator->node = 0;
	iterator->edge = 0;
	if (!container) {
		return;
	}
	if (container->storageType == CXNeighborListType) {
		iterator->state.list.index = 0;
	} else {
		iterator->state.map.entry = NULL;
	}
}

/** Advances the iterator, exposing the next neighbour when available. */
CXBool CXNeighborIteratorNext(CXNeighborIterator *iterator) {
	if (!iterator || !iterator->container) {
		return CXFalse;
	}
	if (iterator->container->storageType == CXNeighborListType) {
		CXNeighborList *list = &iterator->container->storage.list;
		if (iterator->state.list.index >= list->count) {
			return CXFalse;
		}
		CXSize idx = iterator->state.list.index++;
		iterator->node = list->nodes[idx];
		iterator->edge = list->edges[idx];
		return CXTrue;
	}

	CXNeighborMap *map = &iterator->container->storage.map;
	CXUIntegerDictionaryEntry *entry;
	if (!iterator->state.map.entry) {
		entry = map->edgeToNode ? *(map->edgeToNode) : NULL;
	} else {
		entry = iterator->state.map.entry->hh.next;
	}
	if (!entry) {
		return CXFalse;
	}
	iterator->state.map.entry = entry;
	iterator->edge = (CXIndex)entry->key;
	CXIndex *nodePtr = (CXIndex *)entry->data;
	iterator->node = nodePtr ? *nodePtr : CXIndexMAX;
	return CXTrue;
}
