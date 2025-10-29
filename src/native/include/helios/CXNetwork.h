//
//  CXNetwork.h
//  Helios Network Core
//
//  High-performance graph container designed for WebAssembly interop. Nodes,
//  edges, and attributes are stored in linear memory to make sharing with
//  JavaScript easy and efficient.
//

#ifndef CXNetwork_CXNetwork_h
#define CXNetwork_CXNetwork_h

#include "CXCommons.h"
#include "CXDictionary.h"
#include "CXIndexManager.h"
#include "CXNeighborStorage.h"
#include "CXSet.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CXNetwork_INITIAL_NODE_CAPACITY
#define CXNetwork_INITIAL_NODE_CAPACITY 128
#endif

#ifndef CXNetwork_INITIAL_EDGE_CAPACITY
#define CXNetwork_INITIAL_EDGE_CAPACITY 256
#endif

#ifndef CXDefaultNeighborStorage
#define CXDefaultNeighborStorage CXNeighborListType
#endif

#ifndef CXNETWORK_VERSION_MAJOR
#define CXNETWORK_VERSION_MAJOR 0
#endif

#ifndef CXNETWORK_VERSION_MINOR
#define CXNETWORK_VERSION_MINOR 2
#endif

#ifndef CXNETWORK_VERSION_PATCH
#define CXNETWORK_VERSION_PATCH 6
#endif

#ifndef CXNETWORK_VERSION_STRING
#define CXNETWORK_VERSION_STRING "0.2.6"
#endif

/**
 * Enumerates the supported attribute payload types. Values should match the
 * constants surfaced to the JavaScript bindings so the two layers stay in sync.
 */
typedef enum {
	CXStringAttributeType = 0,
	CXBooleanAttributeType = 1,
	CXFloatAttributeType = 2,
	CXIntegerAttributeType = 3,
	CXUnsignedIntegerAttributeType = 4,
	CXDoubleAttributeType = 5,
	CXDataAttributeCategoryType = 6,
	CXDataAttributeType = 7,
	CXJavascriptAttributeType = 8,
	CXUnknownAttributeType = 255
} CXAttributeType;

/**
 * Describes a single attribute buffer stored in linear memory. Attributes can
 * be associated with nodes, edges, or the network as a whole.
 */
typedef struct {
	CXAttributeType type;
	CXSize dimension;
	CXSize elementSize;
	CXSize stride;
	CXSize capacity;
	uint8_t *data;
	CXStringDictionaryRef categoricalDictionary;
	CXBool usesJavascriptShadow;
} CXAttribute;

typedef CXAttribute* CXAttributeRef;

/**
 * Holds adjacency information for a node, tracking both incoming and outgoing
 * relationships via neighbor containers.
 */
typedef struct {
	CXNeighborContainer inNeighbors;
	CXNeighborContainer outNeighbors;
} CXNodeRecord;

typedef struct {
	CXBool isDirected;
	CXSize nodeCount;
	CXSize edgeCount;
	CXSize nodeCapacity;
	CXSize edgeCapacity;

	CXNodeRecord *nodes;
	CXBool *nodeActive;
	CXEdge *edges;
	CXBool *edgeActive;

	CXIndexManagerRef nodeIndexManager;
	CXIndexManagerRef edgeIndexManager;

	CXStringDictionaryRef nodeAttributes;
	CXStringDictionaryRef edgeAttributes;
	CXStringDictionaryRef networkAttributes;
} CXNetwork;

typedef CXNetwork* CXNetworkRef;

/**
 * Selector utilities expose compact buffers of node or edge indices that can be
 * shared with external callers without copying the entire network.
 */
typedef struct {
	CXIndex *indices;
	CXSize count;
	CXSize capacity;
} CXSelector;

typedef CXSelector* CXNodeSelectorRef;
typedef CXSelector* CXEdgeSelectorRef;

// Metadata
/** Returns the semantic version string for the compiled library (e.g. "1.2.3"). */
CX_EXTERN const char* CXNetworkVersionString(void);

// Network lifecycle
/** Allocates a new network with default capacities. */
CX_EXTERN CXNetworkRef CXNewNetwork(CXBool isDirected);
/** Allocates a new network with explicit node/edge capacities. */
CX_EXTERN CXNetworkRef CXNewNetworkWithCapacity(CXBool isDirected, CXSize initialNodeCapacity, CXSize initialEdgeCapacity);
/** Releases all resources owned by a network. */
CX_EXTERN void CXFreeNetwork(CXNetworkRef network);

// Capacity queries
/** Returns the number of active nodes currently stored. */
CX_EXTERN CXSize CXNetworkNodeCount(CXNetworkRef network);
/** Returns the number of active edges currently stored. */
CX_EXTERN CXSize CXNetworkEdgeCount(CXNetworkRef network);
/** Returns the allocated node capacity (useful for buffer sizing). */
CX_EXTERN CXSize CXNetworkNodeCapacity(CXNetworkRef network);
/** Returns the allocated edge capacity. */
CX_EXTERN CXSize CXNetworkEdgeCapacity(CXNetworkRef network);

// Node management
/**
 * Appends `count` new nodes to the network. When `outIndices` is non-null it
 * receives the indices assigned to the created nodes.
 */
CX_EXTERN CXBool CXNetworkAddNodes(CXNetworkRef network, CXSize count, CXIndex *outIndices);
/** Removes the supplied nodes, reclaiming their indices for future use. */
CX_EXTERN CXBool CXNetworkRemoveNodes(CXNetworkRef network, const CXIndex *indices, CXSize count);
/** Returns CXTrue if the given node index is currently active. */
CX_EXTERN CXBool CXNetworkIsNodeActive(CXNetworkRef network, CXIndex node);
/** Provides a pointer to the node activity bitmap in linear memory. */
CX_EXTERN const CXBool* CXNetworkNodeActivityBuffer(CXNetworkRef network);

// Edge management
/**
 * Inserts the provided edges, writing the new indices to `outIndices` when
 * supplied. Edges are expressed as contiguous (from,to) pairs.
 */
CX_EXTERN CXBool CXNetworkAddEdges(CXNetworkRef network, const CXEdge *edges, CXSize count, CXIndex *outIndices);
/** Removes the supplied edges from the network. */
CX_EXTERN CXBool CXNetworkRemoveEdges(CXNetworkRef network, const CXIndex *indices, CXSize count);
/** Returns CXTrue if the edge index is active. */
CX_EXTERN CXBool CXNetworkIsEdgeActive(CXNetworkRef network, CXIndex edge);
/** Provides a pointer to the edge activity bitmap. */
CX_EXTERN const CXBool* CXNetworkEdgeActivityBuffer(CXNetworkRef network);
/** Returns a pointer to the flattened edge buffer `[from, to, ...]`. */
CX_EXTERN CXEdge* CXNetworkEdgesBuffer(CXNetworkRef network);

// Adjacency access
/** Returns the outbound neighbor container for the given node. */
CX_EXTERN CXNeighborContainer* CXNetworkOutNeighbors(CXNetworkRef network, CXIndex node);
/** Returns the inbound neighbor container for the given node. */
CX_EXTERN CXNeighborContainer* CXNetworkInNeighbors(CXNetworkRef network, CXIndex node);

// Attribute management
/** Declares a node attribute backing buffer. Dimension defaults to 1. */
CX_EXTERN CXBool CXNetworkDefineNodeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension);
/** Declares an edge attribute backing buffer. */
CX_EXTERN CXBool CXNetworkDefineEdgeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension);
/** Declares a network-level attribute backing buffer. */
CX_EXTERN CXBool CXNetworkDefineNetworkAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension);

/** Fetches a node attribute descriptor by name. */
CX_EXTERN CXAttributeRef CXNetworkGetNodeAttribute(CXNetworkRef network, const CXString name);
/** Fetches an edge attribute descriptor by name. */
CX_EXTERN CXAttributeRef CXNetworkGetEdgeAttribute(CXNetworkRef network, const CXString name);
/** Fetches a network attribute descriptor by name. */
CX_EXTERN CXAttributeRef CXNetworkGetNetworkAttribute(CXNetworkRef network, const CXString name);

/** Returns a pointer to the raw node attribute buffer for the named attribute. */
CX_EXTERN void* CXNetworkGetNodeAttributeBuffer(CXNetworkRef network, const CXString name);
/** Returns a pointer to the raw edge attribute buffer for the named attribute. */
CX_EXTERN void* CXNetworkGetEdgeAttributeBuffer(CXNetworkRef network, const CXString name);
/** Returns a pointer to the raw network attribute buffer for the named attribute. */
CX_EXTERN void* CXNetworkGetNetworkAttributeBuffer(CXNetworkRef network, const CXString name);

/** Returns the byte stride between consecutive entries of an attribute. */
CX_EXTERN CXSize CXAttributeStride(CXAttributeRef attribute);

// Selector utilities
/** Creates a selector object for nodes with an optional initial capacity. */
CX_EXTERN CXNodeSelectorRef CXNodeSelectorCreate(CXSize initialCapacity);
/** Releases all heap memory associated with a node selector. */
CX_EXTERN void CXNodeSelectorDestroy(CXNodeSelectorRef selector);
/** Populates the selector with every active node. */
CX_EXTERN CXBool CXNodeSelectorFillAll(CXNodeSelectorRef selector, CXNetworkRef network);
/** Fills the selector with the provided node indices. */
CX_EXTERN CXBool CXNodeSelectorFillFromArray(CXNodeSelectorRef selector, const CXIndex *indices, CXSize count);
/** Returns a pointer to the selector's contiguous index data. */
CX_EXTERN CXIndex* CXNodeSelectorData(CXNodeSelectorRef selector);
/** Returns how many indices are currently stored in the selector. */
CX_EXTERN CXSize CXNodeSelectorCount(CXNodeSelectorRef selector);

/** Creates a selector object for edges. */
CX_EXTERN CXEdgeSelectorRef CXEdgeSelectorCreate(CXSize initialCapacity);
/** Releases all heap memory associated with an edge selector. */
CX_EXTERN void CXEdgeSelectorDestroy(CXEdgeSelectorRef selector);
/** Populates the selector with every active edge. */
CX_EXTERN CXBool CXEdgeSelectorFillAll(CXEdgeSelectorRef selector, CXNetworkRef network);
/** Fills the selector with the provided edge indices. */
CX_EXTERN CXBool CXEdgeSelectorFillFromArray(CXEdgeSelectorRef selector, const CXIndex *indices, CXSize count);
/** Returns a pointer to the selector's index data. */
CX_EXTERN CXIndex* CXEdgeSelectorData(CXEdgeSelectorRef selector);
/** Returns how many indices are currently stored in the selector. */
CX_EXTERN CXSize CXEdgeSelectorCount(CXEdgeSelectorRef selector);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetwork_h */
