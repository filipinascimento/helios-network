#include "CXNetwork.h"

// Internal helpers -----------------------------------------------------------

static const CXIndex CXInvalidIndexValue = CXIndexMAX;

static CXAttributeRef CXAttributeCreate(CXAttributeType type, CXSize dimension, CXSize capacity);
static void CXAttributeDestroy(CXAttributeRef attribute);
static CXBool CXAttributeEnsureCapacity(CXAttributeRef attribute, CXSize requiredCapacity);
static void CXAttributeClearSlot(CXAttributeRef attribute, CXIndex index);

static void CXDestroyAttributeDictionary(CXStringDictionaryRef dictionary);

static CXBool CXNetworkEnsureNodeCapacity(CXNetworkRef network, CXSize required);
static CXBool CXNetworkEnsureEdgeCapacity(CXNetworkRef network, CXSize required);
static void CXNetworkResetNodeRecord(CXNetworkRef network, CXIndex node);
static void CXNetworkResetEdgeRecord(CXNetworkRef network, CXIndex edge);
static void CXNetworkDetachEdge(CXNetworkRef network, CXIndex edge, CXBool recycleIndex);

static CXSelector* CXSelectorCreateInternal(CXSize initialCapacity);
static void CXSelectorDestroyInternal(CXSelector *selector);
static CXBool CXSelectorEnsureCapacity(CXSelector *selector, CXSize capacity);

// -----------------------------------------------------------------------------
// Attribute helpers
// -----------------------------------------------------------------------------

static CXBool CXAttributeComputeLayout(
	CXAttributeType type,
	CXSize dimension,
	CXSize *elementSize,
	CXSize *stride,
	CXBool *usesJavascriptShadow
) {
	if (dimension == 0) {
		dimension = 1;
	}
	CXSize baseSize = 0;
	CXBool requiresShadow = CXFalse;
	switch (type) {
		case CXStringAttributeType:
			baseSize = sizeof(CXString);
			break;
		case CXBooleanAttributeType:
			baseSize = sizeof(uint8_t);
			break;
		case CXFloatAttributeType:
			baseSize = sizeof(float);
			break;
		case CXIntegerAttributeType:
			baseSize = sizeof(int64_t);
			break;
		case CXUnsignedIntegerAttributeType:
			baseSize = sizeof(uint64_t);
			break;
		case CXDoubleAttributeType:
			baseSize = sizeof(double);
			break;
		case CXDataAttributeCategoryType:
			baseSize = sizeof(uint32_t);
			break;
		case CXDataAttributeType:
			baseSize = sizeof(uintptr_t);
			break;
		case CXJavascriptAttributeType:
			baseSize = sizeof(uint32_t);
			requiresShadow = CXTrue;
			break;
		default:
			return CXFalse;
	}
	if (elementSize) {
		*elementSize = baseSize;
	}
	if (stride) {
		*stride = baseSize * dimension;
	}
	if (usesJavascriptShadow) {
		*usesJavascriptShadow = requiresShadow;
	}
	return CXTrue;
}

static CXAttributeRef CXAttributeCreate(CXAttributeType type, CXSize dimension, CXSize capacity) {
	CXSize elementSize = 0;
	CXSize stride = 0;
	CXBool usesJavascriptShadow = CXFalse;
	if (!CXAttributeComputeLayout(type, dimension, &elementSize, &stride, &usesJavascriptShadow)) {
		return NULL;
	}

	CXAttributeRef attribute = calloc(1, sizeof(CXAttribute));
	if (!attribute) {
		return NULL;
	}
	attribute->type = type;
	attribute->dimension = dimension == 0 ? 1 : dimension;
	attribute->elementSize = elementSize;
	attribute->stride = stride;
	attribute->usesJavascriptShadow = usesJavascriptShadow;

	if (type == CXDataAttributeCategoryType) {
		attribute->categoricalDictionary = CXNewStringDictionary();
	}

	if (capacity > 0) {
		attribute->data = calloc(capacity, stride);
		if (!attribute->data) {
			CXAttributeDestroy(attribute);
			return NULL;
		}
		attribute->capacity = capacity;
	}
	return attribute;
}

static void CXAttributeDestroy(CXAttributeRef attribute) {
	if (!attribute) {
		return;
	}
	if (attribute->data) {
		free(attribute->data);
		attribute->data = NULL;
	}
	if (attribute->categoricalDictionary) {
		CXStringDictionaryDestroy(attribute->categoricalDictionary);
	}
	free(attribute);
}

/** Ensures an attribute buffer has room for the requested number of entries. */
static CXBool CXAttributeEnsureCapacity(CXAttributeRef attribute, CXSize requiredCapacity) {
	if (!attribute) {
		return CXFalse;
	}
	if (requiredCapacity <= attribute->capacity) {
		return CXTrue;
	}
	CXSize newCapacity = attribute->capacity > 0 ? attribute->capacity : 4;
	while (newCapacity < requiredCapacity) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < requiredCapacity) {
			newCapacity = requiredCapacity;
			break;
		}
	}
	uint8_t *newData = realloc(attribute->data, newCapacity * attribute->stride);
	if (!newData) {
		return CXFalse;
	}
	size_t oldBytes = (size_t)attribute->capacity * attribute->stride;
	size_t newBytes = (size_t)newCapacity * attribute->stride;
	if (newBytes > oldBytes) {
		memset(newData + oldBytes, 0, newBytes - oldBytes);
	}
	attribute->data = newData;
	attribute->capacity = newCapacity;
	return CXTrue;
}

/** Zeroes the attribute payload for a single logical index. */
static void CXAttributeClearSlot(CXAttributeRef attribute, CXIndex index) {
	if (!attribute || !attribute->data || index >= attribute->capacity) {
		return;
	}
	memset(attribute->data + ((size_t)index * attribute->stride), 0, attribute->stride);
}

// -----------------------------------------------------------------------------
// Dictionary helper
// -----------------------------------------------------------------------------

/** Helper that destroys every attribute stored in the provided dictionary. */
static void CXDestroyAttributeDictionary(CXStringDictionaryRef dictionary) {
	if (!dictionary) {
		return;
	}
	CXStringDictionaryEntry *entry, *tmp;
	HASH_ITER(hh, (*dictionary), entry, tmp) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		CXAttributeDestroy(attribute);
	}
	CXStringDictionaryClear(dictionary);
	CXStringDictionaryDestroy(dictionary);
}

// -----------------------------------------------------------------------------
// Network allocation and capacity
// -----------------------------------------------------------------------------

/** Initializes a node record with empty neighbour containers. */
static void CXNodeRecordInit(CXNodeRecord *record) {
	if (!record) {
		return;
	}
	memset(record, 0, sizeof(CXNodeRecord));
	CXNeighborContainerInit(&record->outNeighbors, CXDefaultNeighborStorage, 0);
	CXNeighborContainerInit(&record->inNeighbors, CXDefaultNeighborStorage, 0);
}

/** Grows node-centric buffers until the requested capacity is satisfied. */
static CXBool CXNetworkEnsureNodeCapacity(CXNetworkRef network, CXSize required) {
	if (!network) {
		return CXFalse;
	}
	if (required <= network->nodeCapacity) {
		return CXTrue;
	}

	CXSize newCapacity = network->nodeCapacity > 0 ? network->nodeCapacity : CXNetwork_INITIAL_NODE_CAPACITY;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}

	CXNodeRecord *newNodes = calloc(newCapacity, sizeof(CXNodeRecord));
	if (!newNodes) {
		return CXFalse;
	}
	if (network->nodes) {
		memcpy(newNodes, network->nodes, sizeof(CXNodeRecord) * network->nodeCapacity);
	}
	for (CXSize idx = network->nodeCapacity; idx < newCapacity; idx++) {
		CXNodeRecordInit(&newNodes[idx]);
	}
	CXBool *newActive = calloc(newCapacity, sizeof(CXBool));
	if (!newActive) {
		free(newNodes);
		return CXFalse;
	}
	if (network->nodeActive) {
		memcpy(newActive, network->nodeActive, sizeof(CXBool) * network->nodeCapacity);
	}

	if (network->nodeIndexManager) {
		if (!CXResizeIndexManager(network->nodeIndexManager, newCapacity)) {
			free(newNodes);
			free(newActive);
			return CXFalse;
		}
	}

	CXStringDictionaryFOR(entry, network->nodeAttributes) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		if (!CXAttributeEnsureCapacity(attribute, newCapacity)) {
			free(newNodes);
			free(newActive);
			return CXFalse;
		}
	}

	free(network->nodes);
	free(network->nodeActive);
	network->nodes = newNodes;
	network->nodeActive = newActive;
	network->nodeCapacity = newCapacity;
	return CXTrue;
}

/** Grows edge-centric buffers until the requested capacity is satisfied. */
static CXBool CXNetworkEnsureEdgeCapacity(CXNetworkRef network, CXSize required) {
	if (!network) {
		return CXFalse;
	}
	if (required <= network->edgeCapacity) {
		return CXTrue;
	}

	CXSize newCapacity = network->edgeCapacity > 0 ? network->edgeCapacity : CXNetwork_INITIAL_EDGE_CAPACITY;
	while (newCapacity < required) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < required) {
			newCapacity = required;
			break;
		}
	}

	CXEdge *newEdges = calloc(newCapacity, sizeof(CXEdge));
	if (!newEdges) {
		return CXFalse;
	}
	if (network->edges) {
		memcpy(newEdges, network->edges, sizeof(CXEdge) * network->edgeCapacity);
	}
	CXBool *newActive = calloc(newCapacity, sizeof(CXBool));
	if (!newActive) {
		free(newEdges);
		return CXFalse;
	}
	if (network->edgeActive) {
		memcpy(newActive, network->edgeActive, sizeof(CXBool) * network->edgeCapacity);
	}

	if (network->edgeIndexManager) {
		if (!CXResizeIndexManager(network->edgeIndexManager, newCapacity)) {
			free(newEdges);
			free(newActive);
			return CXFalse;
		}
	}

	CXStringDictionaryFOR(entry, network->edgeAttributes) {
		CXAttributeRef attribute = (CXAttributeRef)entry->data;
		if (!CXAttributeEnsureCapacity(attribute, newCapacity)) {
			free(newEdges);
			free(newActive);
			return CXFalse;
		}
	}

	free(network->edges);
	free(network->edgeActive);
	network->edges = newEdges;
	network->edgeActive = newActive;
	network->edgeCapacity = newCapacity;
	return CXTrue;
}

// -----------------------------------------------------------------------------
// Network lifecycle
// -----------------------------------------------------------------------------

/** Allocates and initializes a network with explicit node/edge capacities. */
CXNetworkRef CXNewNetworkWithCapacity(CXBool isDirected, CXSize initialNodeCapacity, CXSize initialEdgeCapacity) {
	CXNetworkRef network = calloc(1, sizeof(CXNetwork));
	if (!network) {
		return NULL;
	}
	network->isDirected = isDirected;
	network->nodeAttributes = CXNewStringDictionary();
	network->edgeAttributes = CXNewStringDictionary();
	network->networkAttributes = CXNewStringDictionary();
	network->nodeIndexManager = CXNewIndexManager(initialNodeCapacity, initialNodeCapacity);
	network->edgeIndexManager = CXNewIndexManager(initialEdgeCapacity, initialEdgeCapacity);

	if (!CXNetworkEnsureNodeCapacity(network, initialNodeCapacity)) {
		CXFreeNetwork(network);
		return NULL;
	}
	if (!CXNetworkEnsureEdgeCapacity(network, initialEdgeCapacity)) {
		CXFreeNetwork(network);
		return NULL;
	}
	return network;
}

/** Convenience wrapper that builds a network using default capacities. */
CXNetworkRef CXNewNetwork(CXBool isDirected) {
	return CXNewNetworkWithCapacity(isDirected, CXNetwork_INITIAL_NODE_CAPACITY, CXNetwork_INITIAL_EDGE_CAPACITY);
}

/** Releases all resources owned by the network instance. */
void CXFreeNetwork(CXNetworkRef network) {
	if (!network) {
		return;
	}

	if (network->nodes) {
		for (CXSize i = 0; i < network->nodeCapacity; i++) {
			CXNeighborContainerFree(&network->nodes[i].outNeighbors);
			CXNeighborContainerFree(&network->nodes[i].inNeighbors);
		}
		free(network->nodes);
		network->nodes = NULL;
	}

	if (network->nodeActive) {
		free(network->nodeActive);
		network->nodeActive = NULL;
	}

	if (network->edges) {
		free(network->edges);
		network->edges = NULL;
	}

	if (network->edgeActive) {
		free(network->edgeActive);
		network->edgeActive = NULL;
	}

	if (network->nodeIndexManager) {
		CXFreeIndexManager(network->nodeIndexManager);
		free(network->nodeIndexManager);
		network->nodeIndexManager = NULL;
	}
	if (network->edgeIndexManager) {
		CXFreeIndexManager(network->edgeIndexManager);
		free(network->edgeIndexManager);
		network->edgeIndexManager = NULL;
	}

	CXDestroyAttributeDictionary(network->nodeAttributes);
	CXDestroyAttributeDictionary(network->edgeAttributes);
	CXDestroyAttributeDictionary(network->networkAttributes);

	free(network);
}

// -----------------------------------------------------------------------------
// Capacity queries
// -----------------------------------------------------------------------------

/** Returns the number of active nodes in the network. */
CXSize CXNetworkNodeCount(CXNetworkRef network) {
	return network ? network->nodeCount : 0;
}

/** Returns the number of active edges in the network. */
CXSize CXNetworkEdgeCount(CXNetworkRef network) {
	return network ? network->edgeCount : 0;
}

/** Returns the allocated node capacity. */
CXSize CXNetworkNodeCapacity(CXNetworkRef network) {
	return network ? network->nodeCapacity : 0;
}

/** Returns the allocated edge capacity. */
CXSize CXNetworkEdgeCapacity(CXNetworkRef network) {
	return network ? network->edgeCapacity : 0;
}

// -----------------------------------------------------------------------------
// Node management
// -----------------------------------------------------------------------------

/** Clears neighbour state for the given node so it can be reused. */
static void CXNetworkResetNodeRecord(CXNetworkRef network, CXIndex node) {
	CXNodeRecord *record = &network->nodes[node];
	CXNeighborContainerFree(&record->outNeighbors);
	CXNeighborContainerFree(&record->inNeighbors);
	CXNeighborContainerInit(&record->outNeighbors, CXDefaultNeighborStorage, 0);
	CXNeighborContainerInit(&record->inNeighbors, CXDefaultNeighborStorage, 0);
}

/** Adds `count` nodes to the network, optionally returning their indices. */
CXBool CXNetworkAddNodes(CXNetworkRef network, CXSize count, CXIndex *outIndices) {
	if (!network || count == 0) {
		return CXFalse;
	}
	if (!CXNetworkEnsureNodeCapacity(network, network->nodeCount + count)) {
		return CXFalse;
	}

	for (CXSize i = 0; i < count; i++) {
		CXIndex index = CXIndexManagerGetIndex(network->nodeIndexManager);
		if (index == CXInvalidIndexValue) {
			if (!CXNetworkEnsureNodeCapacity(network, network->nodeCapacity + 1)) {
				return CXFalse;
			}
			index = CXIndexManagerGetIndex(network->nodeIndexManager);
			if (index == CXInvalidIndexValue) {
				return CXFalse;
			}
		}
		if (outIndices) {
			outIndices[i] = index;
		}
		network->nodeActive[index] = CXTrue;
		CXNetworkResetNodeRecord(network, index);

		CXStringDictionaryFOR(entry, network->nodeAttributes) {
			CXAttributeRef attribute = (CXAttributeRef)entry->data;
			CXAttributeEnsureCapacity(attribute, network->nodeCapacity);
			CXAttributeClearSlot(attribute, index);
		}

		network->nodeCount++;
	}
	return CXTrue;
}

/** Collects edges from a neighbour container into a heap-allocated buffer. */
static void CXCollectEdgesFromContainer(CXNeighborContainer *container, CXIndex **buffer, CXSize *count) {
	CXSize edgeCount = CXNeighborContainerCount(container);
	if (edgeCount == 0) {
		*buffer = NULL;
		*count = 0;
		return;
	}
	CXIndex *edges = malloc(sizeof(CXIndex) * edgeCount);
	if (!edges) {
		*buffer = NULL;
		*count = 0;
		return;
	}
	CXNeighborContainerGetEdges(container, edges, edgeCount);
	*buffer = edges;
	*count = edgeCount;
}

/** Removes the referenced nodes alongside their incident edges. */
CXBool CXNetworkRemoveNodes(CXNetworkRef network, const CXIndex *indices, CXSize count) {
	if (!network || !indices || count == 0) {
		return CXFalse;
	}

	for (CXSize i = 0; i < count; i++) {
		CXIndex node = indices[i];
		if (node >= network->nodeCapacity || !network->nodeActive[node]) {
			continue;
		}

		// Remove outgoing edges
		CXIndex *edgesBuffer = NULL;
		CXSize edgesCount = 0;
		CXCollectEdgesFromContainer(&network->nodes[node].outNeighbors, &edgesBuffer, &edgesCount);
		for (CXSize e = 0; e < edgesCount; e++) {
			CXNetworkDetachEdge(network, edgesBuffer[e], CXTrue);
		}
		free(edgesBuffer);

		// Remove incoming edges
		edgesBuffer = NULL;
		edgesCount = 0;
		CXCollectEdgesFromContainer(&network->nodes[node].inNeighbors, &edgesBuffer, &edgesCount);
		for (CXSize e = 0; e < edgesCount; e++) {
			CXNetworkDetachEdge(network, edgesBuffer[e], CXTrue);
		}
		free(edgesBuffer);

		CXNetworkResetNodeRecord(network, node);
		network->nodeActive[node] = CXFalse;
		CXIndexManagerAddIndex(network->nodeIndexManager, node);
		CXStringDictionaryFOR(entry, network->nodeAttributes) {
			CXAttributeClearSlot((CXAttributeRef)entry->data, node);
		}
		if (network->nodeCount > 0) {
			network->nodeCount--;
		}
	}
	return CXTrue;
}

/** Returns whether the provided node index currently maps to an active node. */
CXBool CXNetworkIsNodeActive(CXNetworkRef network, CXIndex node) {
	if (!network || node >= network->nodeCapacity) {
		return CXFalse;
	}
	return network->nodeActive[node];
}

/** Exposes the raw node-activity bitmap for zero-copy access. */
const CXBool* CXNetworkNodeActivityBuffer(CXNetworkRef network) {
	return network ? network->nodeActive : NULL;
}

// -----------------------------------------------------------------------------
// Edge management
// -----------------------------------------------------------------------------

/** Clears edge endpoints so the slot can be reused. */
static void CXNetworkResetEdgeRecord(CXNetworkRef network, CXIndex edge) {
	if (!network || edge >= network->edgeCapacity) {
		return;
	}
	memset(&network->edges[edge], 0, sizeof(CXEdge));
}

/** Removes the specific edge from the provided neighbour container. */
static void CXNeighborContainerRemoveSingleEdge(CXNeighborContainer *container, CXIndex edge) {
	if (!container) {
		return;
	}
	if (container->storageType == CXNeighborListType) {
		CXNeighborListRemoveEdgesFromArray(&container->storage.list, &edge, 1);
	} else {
		CXNeighborMapRemoveEdgesFromArray(&container->storage.map, &edge, 1);
	}
}

/** Disconnects an edge from its endpoints and optionally recycles its index. */
static void CXNetworkDetachEdge(CXNetworkRef network, CXIndex edge, CXBool recycleIndex) {
	if (!network || edge >= network->edgeCapacity || !network->edgeActive[edge]) {
		return;
	}
	CXEdge edgeData = network->edges[edge];
	CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.from].outNeighbors, edge);
	CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.to].inNeighbors, edge);
	if (!network->isDirected) {
		CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.from].inNeighbors, edge);
		CXNeighborContainerRemoveSingleEdge(&network->nodes[edgeData.to].outNeighbors, edge);
	}
	network->edgeActive[edge] = CXFalse;
	CXNetworkResetEdgeRecord(network, edge);
	CXStringDictionaryFOR(entry, network->edgeAttributes) {
		CXAttributeClearSlot((CXAttributeRef)entry->data, edge);
	}
	if (network->edgeCount > 0) {
		network->edgeCount--;
	}
	if (recycleIndex) {
		CXIndexManagerAddIndex(network->edgeIndexManager, edge);
	}
}

/** Adds new edges to the network, validating endpoints and returning indices. */
CXBool CXNetworkAddEdges(CXNetworkRef network, const CXEdge *edges, CXSize count, CXIndex *outIndices) {
	if (!network || !edges || count == 0) {
		return CXFalse;
	}
	if (!CXNetworkEnsureEdgeCapacity(network, network->edgeCount + count)) {
		return CXFalse;
	}

	for (CXSize i = 0; i < count; i++) {
		CXEdge edge = edges[i];
		if (edge.from >= network->nodeCapacity || edge.to >= network->nodeCapacity) {
			return CXFalse;
		}
		if (!network->nodeActive[edge.from] || !network->nodeActive[edge.to]) {
			return CXFalse;
		}

		CXIndex edgeIndex = CXIndexManagerGetIndex(network->edgeIndexManager);
		if (edgeIndex == CXInvalidIndexValue) {
			if (!CXNetworkEnsureEdgeCapacity(network, network->edgeCapacity + 1)) {
				return CXFalse;
			}
			edgeIndex = CXIndexManagerGetIndex(network->edgeIndexManager);
			if (edgeIndex == CXInvalidIndexValue) {
				return CXFalse;
			}
		}
		if (outIndices) {
			outIndices[i] = edgeIndex;
		}
		network->edges[edgeIndex] = edge;
		network->edgeActive[edgeIndex] = CXTrue;

		CXNeighborContainerAdd(&network->nodes[edge.from].outNeighbors, edge.to, edgeIndex);
		CXNeighborContainerAdd(&network->nodes[edge.to].inNeighbors, edge.from, edgeIndex);
		if (!network->isDirected) {
			CXNeighborContainerAdd(&network->nodes[edge.from].inNeighbors, edge.to, edgeIndex);
			CXNeighborContainerAdd(&network->nodes[edge.to].outNeighbors, edge.from, edgeIndex);
		}

		CXStringDictionaryFOR(entry, network->edgeAttributes) {
			CXAttributeEnsureCapacity((CXAttributeRef)entry->data, network->edgeCapacity);
			CXAttributeClearSlot((CXAttributeRef)entry->data, edgeIndex);
		}

		network->edgeCount++;
	}
	return CXTrue;
}

/** Removes the referenced edges from the network. */
CXBool CXNetworkRemoveEdges(CXNetworkRef network, const CXIndex *indices, CXSize count) {
	if (!network || !indices || count == 0) {
		return CXFalse;
	}
	for (CXSize i = 0; i < count; i++) {
		CXNetworkDetachEdge(network, indices[i], CXTrue);
	}
	return CXTrue;
}

/** Returns whether the supplied edge index corresponds to an active edge. */
CXBool CXNetworkIsEdgeActive(CXNetworkRef network, CXIndex edge) {
	if (!network || edge >= network->edgeCapacity) {
		return CXFalse;
	}
	return network->edgeActive[edge];
}

/** Exposes the raw edge-activity bitmap for zero-copy access. */
const CXBool* CXNetworkEdgeActivityBuffer(CXNetworkRef network) {
	return network ? network->edgeActive : NULL;
}

/** Returns a pointer to the contiguous edge buffer `[from,to,...]`. */
CXEdge* CXNetworkEdgesBuffer(CXNetworkRef network) {
	return network ? network->edges : NULL;
}

// -----------------------------------------------------------------------------
// Adjacency access
// -----------------------------------------------------------------------------

/** Returns the outbound neighbour container for the given node. */
CXNeighborContainer* CXNetworkOutNeighbors(CXNetworkRef network, CXIndex node) {
	if (!network || node >= network->nodeCapacity) {
		return NULL;
	}
	return &network->nodes[node].outNeighbors;
}

/** Returns the inbound neighbour container for the given node. */
CXNeighborContainer* CXNetworkInNeighbors(CXNetworkRef network, CXIndex node) {
	if (!network || node >= network->nodeCapacity) {
		return NULL;
	}
	return &network->nodes[node].inNeighbors;
}

// -----------------------------------------------------------------------------
// Attribute API
// -----------------------------------------------------------------------------

/** Looks up an attribute within the provided dictionary helper. */
static CXAttributeRef CXNetworkGetAttribute(CXStringDictionaryRef dictionary, const CXString name) {
	if (!dictionary || !name) {
		return NULL;
	}
	return (CXAttributeRef)CXStringDictionaryEntryForKey(dictionary, name);
}

/** Registers a node attribute with the provided configuration. */
CXBool CXNetworkDefineNodeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkGetAttribute(network->nodeAttributes, name)) {
		return CXFalse;
	}
	CXAttributeRef attribute = CXAttributeCreate(type, dimension, network->nodeCapacity);
	if (!attribute) {
		return CXFalse;
	}
	CXStringDictionarySetEntry(network->nodeAttributes, name, attribute);
	return CXTrue;
}

/** Registers an edge attribute with the provided configuration. */
CXBool CXNetworkDefineEdgeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkGetAttribute(network->edgeAttributes, name)) {
		return CXFalse;
	}
	CXAttributeRef attribute = CXAttributeCreate(type, dimension, network->edgeCapacity);
	if (!attribute) {
		return CXFalse;
	}
	CXStringDictionarySetEntry(network->edgeAttributes, name, attribute);
	return CXTrue;
}

/** Registers a network-level attribute with the provided configuration. */
CXBool CXNetworkDefineNetworkAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension) {
	if (!network || !name) {
		return CXFalse;
	}
	if (CXNetworkGetAttribute(network->networkAttributes, name)) {
		return CXFalse;
	}
	CXAttributeRef attribute = CXAttributeCreate(type, dimension, 1);
	if (!attribute) {
		return CXFalse;
	}
	CXStringDictionarySetEntry(network->networkAttributes, name, attribute);
	return CXTrue;
}

/** Retrieves the node attribute descriptor for the supplied name. */
CXAttributeRef CXNetworkGetNodeAttribute(CXNetworkRef network, const CXString name) {
	return network ? CXNetworkGetAttribute(network->nodeAttributes, name) : NULL;
}

/** Retrieves the edge attribute descriptor for the supplied name. */
CXAttributeRef CXNetworkGetEdgeAttribute(CXNetworkRef network, const CXString name) {
	return network ? CXNetworkGetAttribute(network->edgeAttributes, name) : NULL;
}

/** Retrieves the network-level attribute descriptor for the supplied name. */
CXAttributeRef CXNetworkGetNetworkAttribute(CXNetworkRef network, const CXString name) {
	return network ? CXNetworkGetAttribute(network->networkAttributes, name) : NULL;
}

/** Returns a pointer to the raw node attribute buffer, or NULL when missing. */
void* CXNetworkGetNodeAttributeBuffer(CXNetworkRef network, const CXString name) {
	CXAttributeRef attr = CXNetworkGetNodeAttribute(network, name);
	return attr ? attr->data : NULL;
}

/** Returns a pointer to the raw edge attribute buffer, or NULL when missing. */
void* CXNetworkGetEdgeAttributeBuffer(CXNetworkRef network, const CXString name) {
	CXAttributeRef attr = CXNetworkGetEdgeAttribute(network, name);
	return attr ? attr->data : NULL;
}

/** Returns a pointer to the raw network attribute buffer, or NULL when missing. */
void* CXNetworkGetNetworkAttributeBuffer(CXNetworkRef network, const CXString name) {
	CXAttributeRef attr = CXNetworkGetNetworkAttribute(network, name);
	return attr ? attr->data : NULL;
}

/** Returns the byte stride for entries in the attribute buffer. */
CXSize CXAttributeStride(CXAttributeRef attribute) {
	return attribute ? attribute->stride : 0;
}

// -----------------------------------------------------------------------------
// Selector utilities
// -----------------------------------------------------------------------------

/** Allocates a selector and optionally reserves storage for indices. */
static CXSelector* CXSelectorCreateInternal(CXSize initialCapacity) {
	CXSelector *selector = calloc(1, sizeof(CXSelector));
	if (!selector) {
		return NULL;
	}
	if (initialCapacity > 0) {
		selector->indices = malloc(sizeof(CXIndex) * initialCapacity);
		if (!selector->indices) {
			free(selector);
			return NULL;
		}
		selector->capacity = initialCapacity;
	}
	return selector;
}

/** Releases the selector and its backing storage. */
static void CXSelectorDestroyInternal(CXSelector *selector) {
	if (!selector) {
		return;
	}
	if (selector->indices) {
		free(selector->indices);
	}
	free(selector);
}

/** Ensures the selector can store at least `capacity` indices. */
static CXBool CXSelectorEnsureCapacity(CXSelector *selector, CXSize capacity) {
	if (!selector) {
		return CXFalse;
	}
	if (capacity <= selector->capacity) {
		return CXTrue;
	}
	CXSize newCapacity = selector->capacity > 0 ? selector->capacity : 4;
	while (newCapacity < capacity) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < capacity) {
			newCapacity = capacity;
			break;
		}
	}
	CXIndex *newIndices = realloc(selector->indices, sizeof(CXIndex) * newCapacity);
	if (!newIndices) {
		return CXFalse;
	}
	selector->indices = newIndices;
	selector->capacity = newCapacity;
	return CXTrue;
}

/** Populates the selector with every active index reported by `activity`. */
static CXBool CXSelectorFillAll(CXSelector *selector, const CXBool *activity, CXSize capacity) {
	if (!selector || !activity) {
		return CXFalse;
	}
	CXSize required = 0;
	for (CXSize idx = 0; idx < capacity; idx++) {
		if (activity[idx]) {
			required++;
		}
	}
	if (!CXSelectorEnsureCapacity(selector, required)) {
		return CXFalse;
	}
	selector->count = 0;
	for (CXSize idx = 0; idx < capacity; idx++) {
		if (activity[idx]) {
			selector->indices[selector->count++] = idx;
		}
	}
	return CXTrue;
}

/** Copies the provided indices into the selector, resizing as needed. */
static CXBool CXSelectorFillFromArrayInternal(CXSelector *selector, const CXIndex *indices, CXSize count) {
	if (!selector || !indices) {
		return CXFalse;
	}
	if (!CXSelectorEnsureCapacity(selector, count)) {
		return CXFalse;
	}
	memcpy(selector->indices, indices, sizeof(CXIndex) * count);
	selector->count = count;
	return CXTrue;
}

/** Allocates a node selector with optional preallocated capacity. */
CXNodeSelectorRef CXNodeSelectorCreate(CXSize initialCapacity) {
	return CXSelectorCreateInternal(initialCapacity);
}

/** Releases the selector and its backing storage. */
void CXNodeSelectorDestroy(CXNodeSelectorRef selector) {
	CXSelectorDestroyInternal(selector);
}

/** Populates the selector with all active node indices. */
CXBool CXNodeSelectorFillAll(CXNodeSelectorRef selector, CXNetworkRef network) {
	if (!network || !selector) {
		return CXFalse;
	}
	return CXSelectorFillAll(selector, network->nodeActive, network->nodeCapacity);
}

/** Copies the provided list of indices into the selector. */
CXBool CXNodeSelectorFillFromArray(CXNodeSelectorRef selector, const CXIndex *indices, CXSize count) {
	return CXSelectorFillFromArrayInternal(selector, indices, count);
}

/** Returns a pointer to the contiguous array of node indices. */
CXIndex* CXNodeSelectorData(CXNodeSelectorRef selector) {
	return selector ? selector->indices : NULL;
}

/** Returns how many entries are currently stored in the selector. */
CXSize CXNodeSelectorCount(CXNodeSelectorRef selector) {
	return selector ? selector->count : 0;
}

/** Allocates an edge selector with optional preallocated capacity. */
CXEdgeSelectorRef CXEdgeSelectorCreate(CXSize initialCapacity) {
	return CXSelectorCreateInternal(initialCapacity);
}

/** Releases the selector and its backing storage. */
void CXEdgeSelectorDestroy(CXEdgeSelectorRef selector) {
	CXSelectorDestroyInternal(selector);
}

/** Populates the selector with all active edge indices. */
CXBool CXEdgeSelectorFillAll(CXEdgeSelectorRef selector, CXNetworkRef network) {
	if (!network || !selector) {
		return CXFalse;
	}
	return CXSelectorFillAll(selector, network->edgeActive, network->edgeCapacity);
}

/** Copies the provided list of edge indices into the selector. */
CXBool CXEdgeSelectorFillFromArray(CXEdgeSelectorRef selector, const CXIndex *indices, CXSize count) {
	return CXSelectorFillFromArrayInternal(selector, indices, count);
}

/** Returns a pointer to the contiguous array of edge indices. */
CXIndex* CXEdgeSelectorData(CXEdgeSelectorRef selector) {
	return selector ? selector->indices : NULL;
}

/** Returns how many entries are currently stored in the selector. */
CXSize CXEdgeSelectorCount(CXEdgeSelectorRef selector) {
	return selector ? selector->count : 0;
}
