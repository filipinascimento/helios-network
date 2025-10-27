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

typedef struct {
	CXIndex *indices;
	CXSize count;
	CXSize capacity;
} CXSelector;

typedef CXSelector* CXNodeSelectorRef;
typedef CXSelector* CXEdgeSelectorRef;

// Network lifecycle
CX_EXTERN CXNetworkRef CXNewNetwork(CXBool isDirected);
CX_EXTERN CXNetworkRef CXNewNetworkWithCapacity(CXBool isDirected, CXSize initialNodeCapacity, CXSize initialEdgeCapacity);
CX_EXTERN void CXFreeNetwork(CXNetworkRef network);

// Capacity queries
CX_EXTERN CXSize CXNetworkNodeCount(CXNetworkRef network);
CX_EXTERN CXSize CXNetworkEdgeCount(CXNetworkRef network);
CX_EXTERN CXSize CXNetworkNodeCapacity(CXNetworkRef network);
CX_EXTERN CXSize CXNetworkEdgeCapacity(CXNetworkRef network);

// Node management
CX_EXTERN CXBool CXNetworkAddNodes(CXNetworkRef network, CXSize count, CXIndex *outIndices);
CX_EXTERN CXBool CXNetworkRemoveNodes(CXNetworkRef network, const CXIndex *indices, CXSize count);
CX_EXTERN CXBool CXNetworkIsNodeActive(CXNetworkRef network, CXIndex node);
CX_EXTERN const CXBool* CXNetworkNodeActivityBuffer(CXNetworkRef network);

// Edge management
CX_EXTERN CXBool CXNetworkAddEdges(CXNetworkRef network, const CXEdge *edges, CXSize count, CXIndex *outIndices);
CX_EXTERN CXBool CXNetworkRemoveEdges(CXNetworkRef network, const CXIndex *indices, CXSize count);
CX_EXTERN CXBool CXNetworkIsEdgeActive(CXNetworkRef network, CXIndex edge);
CX_EXTERN const CXBool* CXNetworkEdgeActivityBuffer(CXNetworkRef network);
CX_EXTERN CXEdge* CXNetworkEdgesBuffer(CXNetworkRef network);

// Adjacency access
CX_EXTERN CXNeighborContainer* CXNetworkOutNeighbors(CXNetworkRef network, CXIndex node);
CX_EXTERN CXNeighborContainer* CXNetworkInNeighbors(CXNetworkRef network, CXIndex node);

// Attribute management
CX_EXTERN CXBool CXNetworkDefineNodeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension);
CX_EXTERN CXBool CXNetworkDefineEdgeAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension);
CX_EXTERN CXBool CXNetworkDefineNetworkAttribute(CXNetworkRef network, const CXString name, CXAttributeType type, CXSize dimension);

CX_EXTERN CXAttributeRef CXNetworkGetNodeAttribute(CXNetworkRef network, const CXString name);
CX_EXTERN CXAttributeRef CXNetworkGetEdgeAttribute(CXNetworkRef network, const CXString name);
CX_EXTERN CXAttributeRef CXNetworkGetNetworkAttribute(CXNetworkRef network, const CXString name);

CX_EXTERN void* CXNetworkGetNodeAttributeBuffer(CXNetworkRef network, const CXString name);
CX_EXTERN void* CXNetworkGetEdgeAttributeBuffer(CXNetworkRef network, const CXString name);
CX_EXTERN void* CXNetworkGetNetworkAttributeBuffer(CXNetworkRef network, const CXString name);

CX_EXTERN CXSize CXAttributeStride(CXAttributeRef attribute);

// Selector utilities
CX_EXTERN CXNodeSelectorRef CXNodeSelectorCreate(CXSize initialCapacity);
CX_EXTERN void CXNodeSelectorDestroy(CXNodeSelectorRef selector);
CX_EXTERN CXBool CXNodeSelectorFillAll(CXNodeSelectorRef selector, CXNetworkRef network);
CX_EXTERN CXBool CXNodeSelectorFillFromArray(CXNodeSelectorRef selector, const CXIndex *indices, CXSize count);
CX_EXTERN CXIndex* CXNodeSelectorData(CXNodeSelectorRef selector);
CX_EXTERN CXSize CXNodeSelectorCount(CXNodeSelectorRef selector);

CX_EXTERN CXEdgeSelectorRef CXEdgeSelectorCreate(CXSize initialCapacity);
CX_EXTERN void CXEdgeSelectorDestroy(CXEdgeSelectorRef selector);
CX_EXTERN CXBool CXEdgeSelectorFillAll(CXEdgeSelectorRef selector, CXNetworkRef network);
CX_EXTERN CXBool CXEdgeSelectorFillFromArray(CXEdgeSelectorRef selector, const CXIndex *indices, CXSize count);
CX_EXTERN CXIndex* CXEdgeSelectorData(CXEdgeSelectorRef selector);
CX_EXTERN CXSize CXEdgeSelectorCount(CXEdgeSelectorRef selector);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetwork_h */
