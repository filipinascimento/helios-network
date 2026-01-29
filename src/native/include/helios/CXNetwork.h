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
#define CXNETWORK_VERSION_MINOR 5
#endif

#ifndef CXNETWORK_VERSION_PATCH
#define CXNETWORK_VERSION_PATCH 6
#endif

#ifndef CXNETWORK_VERSION_STRING
#define CXNETWORK_VERSION_STRING "0.5.6"
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
	CXBigIntegerAttributeType = 9,
	CXUnsignedBigIntegerAttributeType = 10,
	CXDataAttributeMultiCategoryType = 11,
	CXUnknownAttributeType = 255
} CXAttributeType;

typedef enum {
	CXAttributeScopeNode = 0,
	CXAttributeScopeEdge = 1,
	CXAttributeScopeNetwork = 2
} CXAttributeScope;

typedef enum {
	CX_CATEGORY_SORT_NONE = 0,
	CX_CATEGORY_SORT_FREQUENCY = 1,
	CX_CATEGORY_SORT_ALPHABETICAL = 2,
	CX_CATEGORY_SORT_NATURAL = 3
} CXCategorySortOrder;

typedef struct CXMultiCategoryBuffer {
	uint32_t *offsets;
	uint32_t *ids;
	float *weights;
	CXSize entryCount;
	CXSize entryCapacity;
	CXBool hasWeights;
} CXMultiCategoryBuffer;

/**
 * Describes a single attribute buffer stored in linear memory. Attributes can
 * be associated with nodes, edges, or the network as a whole.
 */
typedef struct CXAttribute {
	CXAttributeType type;
	CXSize dimension;
	CXSize elementSize;
	CXSize stride;
	CXSize capacity;
	uint8_t *data;
	CXStringDictionaryRef categoricalDictionary;
	CXMultiCategoryBuffer *multiCategory;
	CXBool usesJavascriptShadow;
	uint64_t version;
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

/**
 * Describes a reusable view of active node or edge indices. The backing buffer
 * remains valid until the network grows beyond the stored capacity, at which
 * point it is resized and the pointer changes.
 */
typedef struct {
	char *name;
	uint8_t *data;
	CXSize count;
	CXSize capacity;
	CXSize stride;
	CXSize validStart;
	CXSize validEnd;
	CXBool dirty;
	CXBool isIndexBuffer;
	uint64_t version;
	uint64_t sourceVersion;
} CXDenseAttributeBuffer;

typedef enum {
	CXDenseColorFormatU8x4 = 0,
	CXDenseColorFormatU32x4 = 1
} CXDenseColorFormat;

typedef struct {
	CXDenseColorFormat format;
} CXDenseColorEncodingOptions;

typedef struct {
	char *encodedName;
	char *sourceName;
	CXDenseColorFormat format;
	CXDenseAttributeBuffer buffer;
	CXBool useIndexSource;
} CXDenseColorEncodedAttribute;

typedef struct CXNetwork {
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

	CXDenseAttributeBuffer *nodeDenseBuffers;
	CXSize nodeDenseBufferCount;
	CXSize nodeDenseBufferCapacity;
	CXDenseAttributeBuffer *edgeDenseBuffers;
	CXSize edgeDenseBufferCount;
	CXSize edgeDenseBufferCapacity;
	CXDenseAttributeBuffer nodeIndexDense;
	CXDenseAttributeBuffer edgeIndexDense;
	CXIndex *nodeDenseOrder;
	CXSize nodeDenseOrderCount;
	CXSize nodeDenseOrderCapacity;
	CXIndex *edgeDenseOrder;
	CXSize edgeDenseOrderCount;
	CXSize edgeDenseOrderCapacity;
	CXDenseColorEncodedAttribute *nodeColorAttributes;
	CXSize nodeColorAttributeCount;
	CXSize nodeColorAttributeCapacity;
	CXDenseColorEncodedAttribute *edgeColorAttributes;
	CXSize edgeColorAttributeCount;
	CXSize edgeColorAttributeCapacity;
	CXSize nodeValidStart;
	CXSize nodeValidEnd;
	CXBool nodeValidRangeDirty;
	CXSize edgeValidStart;
	CXSize edgeValidEnd;
	CXBool edgeValidRangeDirty;
	uint64_t nodeTopologyVersion;
	uint64_t edgeTopologyVersion;
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
/** Returns the current node free-list size (recycled indices). */
CX_EXTERN CXSize CXNetworkNodeFreeListCount(CXNetworkRef network);
/** Returns the allocated node free-list capacity (reserved slots). */
CX_EXTERN CXSize CXNetworkNodeFreeListCapacity(CXNetworkRef network);
/** Returns the current edge free-list size (recycled indices). */
CX_EXTERN CXSize CXNetworkEdgeFreeListCount(CXNetworkRef network);
/** Returns the allocated edge free-list capacity (reserved slots). */
CX_EXTERN CXSize CXNetworkEdgeFreeListCapacity(CXNetworkRef network);
/** Returns CXTrue when the network treats edges as directed. */
CX_EXTERN CXBool CXNetworkIsDirected(CXNetworkRef network);
/**
 * Writes active node indices into caller-provided storage. When `capacity` is
 * insufficient the required size is returned and no writes occur.
 */
CX_EXTERN CXSize CXNetworkWriteActiveNodes(CXNetworkRef network, CXIndex *dst, CXSize capacity);
/**
 * Writes active edge indices into caller-provided storage. When `capacity` is
 * insufficient the required size is returned and no writes occur.
 */
CX_EXTERN CXSize CXNetworkWriteActiveEdges(CXNetworkRef network, CXIndex *dst, CXSize capacity);
/**
 * Writes two position vectors per active edge directly into the provided
 * buffer. `componentsPerNode` describes how many floats to copy per endpoint
 * (commonly 4 for vec4 data). Returns the number of edges that would be
 * written; when `dstCapacityEdges` is too small, the required count is
 * returned and no writes occur.
 */
CX_EXTERN CXSize CXNetworkWriteActiveEdgeSegments(
	CXNetworkRef network,
	const float *positions,
	CXSize componentsPerNode,
	float *dstSegments,
	CXSize dstCapacityEdges
);
/**
 * Writes `componentsPerNode` values for each endpoint of active edges from the
 * provided node attribute buffer into a caller-managed destination. Values are
 * copied verbatim; the caller controls element width (`componentSizeBytes`)
 * and must ensure the typed views line up with the provided byte offsets.
 *
 * Returns the number of edges that would be written; when `dstCapacityEdges`
 * is too small, the required count is returned and no writes occur.
 */
CX_EXTERN CXSize CXNetworkWriteActiveEdgeNodeAttributes(
	CXNetworkRef network,
	const uint8_t *nodeAttributes,
	CXSize componentsPerNode,
	CXSize componentSizeBytes,
	uint8_t *dst,
	CXSize dstCapacityEdges
);
/**
 * Writes node attribute spans for each edge following the stored dense edge
 * order. Layout matches `CXNetworkWriteActiveEdgeNodeAttributes`.
 */
CX_EXTERN CXSize CXNetworkWriteEdgeNodeAttributesInOrder(
	CXNetworkRef network,
	const uint8_t *nodeAttributes,
	CXSize componentsPerNode,
	CXSize componentSizeBytes,
	uint8_t *dst,
	CXSize dstCapacityEdges
);
/**
 * Copies node attributes into an edge attribute buffer using the network's
 * topology. `endpointMode` controls which endpoint is copied: -1 = both,
 * 0 = source only, 1 = destination only. When copying a single endpoint and
 * `duplicateSingleEndpoint` is true, the chosen endpoint is written twice
 * sequentially (for double-width edge attributes).
 */
CX_EXTERN CXSize CXNetworkCopyNodeAttributesToEdgeAttributes(
	CXNetworkRef network,
	const uint8_t *nodeAttributes,
	CXSize nodeStrideBytes,
	uint8_t *edgeAttributes,
	CXSize edgeStrideBytes,
	int endpointMode,
	CXBool duplicateSingleEndpoint
);

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

CX_EXTERN CXStringDictionaryRef CXNetworkGetAttributeCategoryDictionary(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN CXSize CXNetworkGetAttributeCategoryDictionaryCount(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN CXBool CXNetworkGetAttributeCategoryDictionaryEntries(
	CXNetworkRef network,
	CXAttributeScope scope,
	const CXString name,
	int32_t *outIds,
	CXString *outLabels,
	CXSize capacity
);
CX_EXTERN CXBool CXNetworkSetAttributeCategoryDictionary(
	CXNetworkRef network,
	CXAttributeScope scope,
	const CXString name,
	const CXString *labels,
	const int32_t *ids,
	CXSize count,
	CXBool remapExisting
);
CX_EXTERN CXBool CXNetworkCategorizeAttribute(CXNetworkRef network, CXAttributeScope scope, const CXString name, CXCategorySortOrder sortOrder, const CXString missingLabel);
CX_EXTERN CXBool CXNetworkDecategorizeAttribute(CXNetworkRef network, CXAttributeScope scope, const CXString name, const CXString missingLabel);

// Multi-category attribute helpers.
CX_EXTERN CXBool CXNetworkDefineMultiCategoryAttribute(CXNetworkRef network, CXAttributeScope scope, const CXString name, CXBool hasWeights);
CX_EXTERN CXBool CXNetworkSetMultiCategoryEntry(
	CXNetworkRef network,
	CXAttributeScope scope,
	const CXString name,
	CXIndex index,
	const uint32_t *ids,
	CXSize count,
	const float *weights
);
CX_EXTERN CXBool CXNetworkSetMultiCategoryEntryByLabels(
	CXNetworkRef network,
	CXAttributeScope scope,
	const CXString name,
	CXIndex index,
	const CXString *labels,
	CXSize count,
	const float *weights
);
CX_EXTERN CXBool CXNetworkClearMultiCategoryEntry(CXNetworkRef network, CXAttributeScope scope, const CXString name, CXIndex index);
CX_EXTERN CXBool CXNetworkSetMultiCategoryBuffers(
	CXNetworkRef network,
	CXAttributeScope scope,
	const CXString name,
	const uint32_t *offsets,
	CXSize offsetCount,
	const uint32_t *ids,
	CXSize idCount,
	const float *weights
);
CX_EXTERN CXBool CXNetworkGetMultiCategoryEntryRange(
	CXNetworkRef network,
	CXAttributeScope scope,
	const CXString name,
	CXIndex index,
	CXSize *outStart,
	CXSize *outEnd
);
CX_EXTERN uint32_t* CXNetworkGetMultiCategoryOffsets(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN uint32_t* CXNetworkGetMultiCategoryIds(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN float* CXNetworkGetMultiCategoryWeights(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN CXSize CXNetworkGetMultiCategoryOffsetCount(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN CXSize CXNetworkGetMultiCategoryEntryCount(CXNetworkRef network, CXAttributeScope scope, const CXString name);
CX_EXTERN CXBool CXNetworkMultiCategoryHasWeights(CXNetworkRef network, CXAttributeScope scope, const CXString name);

/** Returns a pointer to the raw node attribute buffer for the named attribute. */
CX_EXTERN void* CXNetworkGetNodeAttributeBuffer(CXNetworkRef network, const CXString name);
/** Returns a pointer to the raw edge attribute buffer for the named attribute. */
CX_EXTERN void* CXNetworkGetEdgeAttributeBuffer(CXNetworkRef network, const CXString name);
/** Returns a pointer to the raw network attribute buffer for the named attribute. */
CX_EXTERN void* CXNetworkGetNetworkAttributeBuffer(CXNetworkRef network, const CXString name);

/** Returns the byte stride between consecutive entries of an attribute. */
CX_EXTERN CXSize CXAttributeStride(CXAttributeRef attribute);
/** Returns a pointer to the raw backing buffer for an attribute (or NULL when missing). */
CX_EXTERN void* CXAttributeData(CXAttributeRef attribute);

/**
 * Interpolates a float attribute buffer toward target values and bumps the
 * attribute version. Returns CXTrue when further interpolation steps are
 * recommended based on minDisplacementRatio.
 */
CX_EXTERN CXBool CXAttributeInterpolateFloatBuffer(
	CXAttributeRef attribute,
	const float *target,
	CXSize targetCount,
	float elapsedMs,
	float layoutElapsedMs,
	float smoothing,
	float minDisplacementRatio
);

/**
 * Compacts the network so that node and edge indices become contiguous starting
 * at zero and capacities shrink to match the number of active elements. When
 * `nodeOriginalIndexAttr` or `edgeOriginalIndexAttr` are provided, the function
 * records the previous indices in attributes of type
 * `CXUnsignedIntegerAttributeType`. Returns CXFalse on allocation failure or
 * when incompatible attributes are encountered.
 */
CX_EXTERN CXBool CXNetworkCompact(
	CXNetworkRef network,
	const CXString nodeOriginalIndexAttr,
	const CXString edgeOriginalIndexAttr
);

// Community detection --------------------------------------------------------
/**
 * Runs Leiden community detection optimizing (weighted) modularity.
 *
 * - For undirected graphs, uses the standard modularity objective.
 * - For directed graphs, uses the directed modularity formulation.
 * - `resolution` corresponds to the modularity resolution parameter (gamma).
 * - When `edgeWeightAttribute` is NULL/empty, every edge has weight 1.
 *
 * Writes the resulting community id into a node attribute (created when missing)
 * of type `CXUnsignedIntegerAttributeType` and dimension 1.
 *
 * Returns the number of detected communities, or 0 on failure.
 */
CX_EXTERN CXSize CXNetworkLeidenModularity(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	double resolution,
	uint32_t seed,
	CXSize maxLevels,
	CXSize maxPasses,
	const CXString outNodeCommunityAttribute,
	double *outModularity
);

// Incremental Leiden (steppable) --------------------------------------------
typedef struct CXLeidenSession CXLeidenSession;
typedef CXLeidenSession* CXLeidenSessionRef;

typedef enum {
	CXLeidenPhaseInvalid = 0,
	CXLeidenPhaseBuildGraph = 1,
	CXLeidenPhaseCoarseMove = 2,
	CXLeidenPhaseRefineMove = 3,
	CXLeidenPhaseAggregate = 4,
	CXLeidenPhaseDone = 5,
	CXLeidenPhaseFailed = 6
} CXLeidenPhase;

/**
 * Creates a steppable Leiden session. The network topology and relevant
 * edge weight attribute must not change while the session is active.
 *
 * Returns NULL on failure.
 */
CX_EXTERN CXLeidenSessionRef CXLeidenSessionCreate(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	double resolution,
	uint32_t seed,
	CXSize maxLevels,
	CXSize maxPasses
);

/** Releases all resources held by a Leiden session. */
CX_EXTERN void CXLeidenSessionDestroy(CXLeidenSessionRef session);

/**
 * Advances the session by at most `budget` node-visits (best effort).
 * Returns the current phase after stepping.
 */
CX_EXTERN CXLeidenPhase CXLeidenSessionStep(CXLeidenSessionRef session, CXSize budget);

	/**
	 * Returns current progress metrics. Any output pointer may be NULL.
	 * `outProgressCurrent` and `outProgressTotal` are best-effort and may change
	 * as the algorithm advances (i.e. the total may be revised).
	 */
	CX_EXTERN void CXLeidenSessionGetProgress(
		CXLeidenSessionRef session,
		double *outProgressCurrent,
		double *outProgressTotal,
		CXLeidenPhase *outPhase,
		CXSize *outLevel,
		CXSize *outMaxLevels,
		CXSize *outPass,
	CXSize *outMaxPasses,
	CXSize *outVisitedThisPass,
	CXSize *outNodeCount,
	uint32_t *outCommunityCount
);

/**
 * Finalizes a completed session, writing the resulting community ids into a
 * node attribute of type `CXUnsignedIntegerAttributeType` (dimension 1).
 *
 * Returns CXFalse if the session has not completed or on failure.
 */
CX_EXTERN CXBool CXLeidenSessionFinalize(
	CXLeidenSessionRef session,
	const CXString outNodeCommunityAttribute,
	double *outModularity,
	uint32_t *outCommunityCount
);

// Dense attribute buffers ----------------------------------------------------
/** Registers a dense node attribute buffer that can be refreshed on demand. */
CX_EXTERN CXBool CXNetworkAddDenseNodeAttribute(CXNetworkRef network, const CXString name, CXSize initialCapacity);
/** Registers a dense edge attribute buffer. */
CX_EXTERN CXBool CXNetworkAddDenseEdgeAttribute(CXNetworkRef network, const CXString name, CXSize initialCapacity);
/** Removes a previously registered dense node attribute buffer. */
CX_EXTERN CXBool CXNetworkRemoveDenseNodeAttribute(CXNetworkRef network, const CXString name);
/** Removes a dense edge attribute buffer. */
CX_EXTERN CXBool CXNetworkRemoveDenseEdgeAttribute(CXNetworkRef network, const CXString name);
/** Marks a dense node attribute buffer dirty so it will be repacked. */
CX_EXTERN CXBool CXNetworkMarkDenseNodeAttributeDirty(CXNetworkRef network, const CXString name);
/** Marks a dense edge attribute buffer dirty. */
CX_EXTERN CXBool CXNetworkMarkDenseEdgeAttributeDirty(CXNetworkRef network, const CXString name);
/** Removes a sparse node attribute and its storage. */
CX_EXTERN CXBool CXNetworkRemoveNodeAttribute(CXNetworkRef network, const CXString name);
/** Removes a sparse edge attribute and its storage. */
CX_EXTERN CXBool CXNetworkRemoveEdgeAttribute(CXNetworkRef network, const CXString name);
/** Removes a sparse network attribute and its storage. */
CX_EXTERN CXBool CXNetworkRemoveNetworkAttribute(CXNetworkRef network, const CXString name);
/** Rebuilds a dense node attribute buffer using the provided order (or active order). */
CX_EXTERN const CXDenseAttributeBuffer* CXNetworkUpdateDenseNodeAttribute(CXNetworkRef network, const CXString name);
/** Rebuilds a dense edge attribute buffer. */
CX_EXTERN const CXDenseAttributeBuffer* CXNetworkUpdateDenseEdgeAttribute(CXNetworkRef network, const CXString name);
/** Ensures the dense node index buffer exists and returns it refreshed. */
CX_EXTERN const CXDenseAttributeBuffer* CXNetworkUpdateDenseNodeIndexBuffer(CXNetworkRef network);
/** Ensures the dense edge index buffer exists and returns it refreshed. */
CX_EXTERN const CXDenseAttributeBuffer* CXNetworkUpdateDenseEdgeIndexBuffer(CXNetworkRef network);
/** Registers a color-encoded dense node attribute derived from another integer attribute or the node index. */
CX_EXTERN CXBool CXNetworkDefineDenseColorEncodedNodeAttribute(CXNetworkRef network, const CXString sourceName, const CXString encodedName, CXDenseColorEncodingOptions options);
/** Registers a color-encoded dense edge attribute. */
CX_EXTERN CXBool CXNetworkDefineDenseColorEncodedEdgeAttribute(CXNetworkRef network, const CXString sourceName, const CXString encodedName, CXDenseColorEncodingOptions options);
/** Removes a color-encoded dense node attribute. */
CX_EXTERN CXBool CXNetworkRemoveDenseColorEncodedNodeAttribute(CXNetworkRef network, const CXString encodedName);
/** Removes a color-encoded dense edge attribute. */
CX_EXTERN CXBool CXNetworkRemoveDenseColorEncodedEdgeAttribute(CXNetworkRef network, const CXString encodedName);
/** Marks a color-encoded dense node attribute dirty. */
CX_EXTERN CXBool CXNetworkMarkDenseColorEncodedNodeAttributeDirty(CXNetworkRef network, const CXString encodedName);
/** Marks a color-encoded dense edge attribute dirty. */
CX_EXTERN CXBool CXNetworkMarkDenseColorEncodedEdgeAttributeDirty(CXNetworkRef network, const CXString encodedName);
/** Rebuilds a color-encoded dense node attribute when dirty. */
CX_EXTERN const CXDenseAttributeBuffer* CXNetworkUpdateDenseColorEncodedNodeAttribute(CXNetworkRef network, const CXString encodedName);
/** Rebuilds a color-encoded dense edge attribute when dirty. */
CX_EXTERN const CXDenseAttributeBuffer* CXNetworkUpdateDenseColorEncodedEdgeAttribute(CXNetworkRef network, const CXString encodedName);
/** Returns the version counter for a dense attribute buffer (0 when unavailable). */
CX_EXTERN uint64_t CXDenseAttributeBufferVersion(const CXDenseAttributeBuffer *buffer);
/** Returns the source version last packed into a dense buffer. */
CX_EXTERN uint64_t CXDenseAttributeBufferSourceVersion(const CXDenseAttributeBuffer *buffer);
/** Returns the version counter for an attribute descriptor. */
CX_EXTERN uint64_t CXAttributeVersion(CXAttributeRef attribute);
/** Returns the node topology version (increments on topology edits and repacks). */
CX_EXTERN uint64_t CXNetworkNodeTopologyVersion(CXNetworkRef network);
/** Returns the edge topology version. */
CX_EXTERN uint64_t CXNetworkEdgeTopologyVersion(CXNetworkRef network);
/** Manually bumps a node attribute version and returns the new value. */
CX_EXTERN uint64_t CXNetworkBumpNodeAttributeVersion(CXNetworkRef network, const CXString name);
/** Manually bumps an edge attribute version and returns the new value. */
CX_EXTERN uint64_t CXNetworkBumpEdgeAttributeVersion(CXNetworkRef network, const CXString name);
/** Manually bumps a network attribute version and returns the new value. */
CX_EXTERN uint64_t CXNetworkBumpNetworkAttributeVersion(CXNetworkRef network, const CXString name);
/** Sets a default node order for dense packing (applied to all dense buffers when order is omitted). */
CX_EXTERN CXBool CXNetworkSetDenseNodeOrder(CXNetworkRef network, const CXIndex *order, CXSize count);
/** Sets a default edge order for dense packing. */
CX_EXTERN CXBool CXNetworkSetDenseEdgeOrder(CXNetworkRef network, const CXIndex *order, CXSize count);
/** Returns the min/max active node indices as [start,end). */
CX_EXTERN CXBool CXNetworkGetNodeValidRange(CXNetworkRef network, CXSize *start, CXSize *end);
/** Returns the min/max active edge indices as [start,end). */
CX_EXTERN CXBool CXNetworkGetEdgeValidRange(CXNetworkRef network, CXSize *start, CXSize *end);

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

#include "CXNetworkBXNet.h"

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXNetwork_h */
