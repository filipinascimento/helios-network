#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CXNetwork.h"
#include "CXNeighborStorage.h"
#include "CXNetworkGML.h"
#include "CXNetworkGT.h"
#include "CXNetworkNodeLinkJSON.h"
#include "CXNetworkXNet.h"

static void free_attribute_strings(CXAttributeRef attr, CXSize count) {
	if (!attr || !attr->data || attr->type != CXStringAttributeType) {
		return;
	}
	CXString *values = (CXString *)attr->data;
	for (CXSize i = 0; i < count; i++) {
		if (values[i]) {
			free(values[i]);
			values[i] = NULL;
		}
	}
}

static void release_all_string_attributes(CXNetworkRef net) {
	if (!net) {
		return;
	}
	CXStringDictionaryFOR(entry, net->nodeAttributes) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		free_attribute_strings(attr, attr ? attr->capacity : 0);
	}
	CXStringDictionaryFOR(entry, net->edgeAttributes) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		free_attribute_strings(attr, attr ? attr->capacity : 0);
	}
	CXStringDictionaryFOR(entry, net->networkAttributes) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		free_attribute_strings(attr, attr ? attr->capacity : 0);
	}
}

static CXBool lookup_category_id(CXStringDictionaryRef dictionary, const char *label, int32_t *outId) {
	if (!dictionary || !label || !outId) {
		return CXFalse;
	}
	CXStringDictionaryFOR(entry, dictionary) {
		if (entry->key && strcmp(entry->key, label) == 0) {
			uintptr_t raw = (uintptr_t)entry->data;
			if (raw == 0u) {
				return CXFalse;
			}
			if (raw == 1u) {
				*outId = -1;
				return CXTrue;
			}
			*outId = (int32_t)(uint32_t)(raw - 2u);
			return CXTrue;
		}
	}
	return CXFalse;
}

static CXSize find_node_position(const CXIndex *nodes, CXSize count, uint64_t needle);

static void test_basic_network(void) {
	CXNetworkRef net = CXNewNetwork(CXTrue);
	assert(net);
	assert(CXNetworkNodeCount(net) == 0);
	assert(CXNetworkEdgeCount(net) == 0);

	CXIndex nodes[3] = {0};
	assert(CXNetworkAddNodes(net, 3, nodes));
	assert(CXNetworkNodeCount(net) == 3);
	for (int i = 0; i < 3; i++) {
		assert(CXNetworkIsNodeActive(net, nodes[i]));
	}

	CXEdge edges[2] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[1], .to = nodes[2] },
	};
	CXIndex edgeIds[2];
	assert(CXNetworkAddEdges(net, edges, 2, edgeIds));
	assert(CXNetworkEdgeCount(net) == 2);
	for (int i = 0; i < 2; i++) {
		assert(CXNetworkIsEdgeActive(net, edgeIds[i]));
	}

	CXNeighborContainer *out0 = CXNetworkOutNeighbors(net, nodes[0]);
	assert(out0);
	assert(CXNeighborContainerCount(out0) == 1);

	assert(CXNetworkRemoveEdges(net, &edgeIds[0], 1));
	assert(CXNetworkEdgeCount(net) == 1);
	assert(!CXNetworkIsEdgeActive(net, edgeIds[0]));

	assert(CXNetworkRemoveNodes(net, &nodes[1], 1));
	assert(!CXNetworkIsNodeActive(net, nodes[1]));

	CXFreeNetwork(net);
}

static void test_neighbor_collection(void) {
	CXNetworkRef net = CXNewNetwork(CXTrue);
	assert(net);

	CXIndex nodes[6] = {0};
	assert(CXNetworkAddNodes(net, 6, nodes));
	CXEdge edges[6] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[0], .to = nodes[2] },
		{ .from = nodes[1], .to = nodes[3] },
		{ .from = nodes[2], .to = nodes[4] },
		{ .from = nodes[3], .to = nodes[5] },
		{ .from = nodes[4], .to = nodes[5] },
	};
	CXIndex edgeIds[6] = {0};
	assert(CXNetworkAddEdges(net, edges, 6, edgeIds));

	CXNodeSelectorRef nodeSelector = CXNodeSelectorCreate(0);
	CXEdgeSelectorRef edgeSelector = CXEdgeSelectorCreate(0);
	assert(nodeSelector);
	assert(edgeSelector);

	CXIndex source0[1] = { nodes[0] };
	assert(CXNetworkCollectNeighbors(
		net,
		source0,
		1,
		CXNeighborDirectionOut,
		CXFalse,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 2);
	assert(CXEdgeSelectorCount(edgeSelector) == 2);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[1]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[2]) != (CXSize)-1);

	CXIndex source01[2] = { nodes[0], nodes[1] };
	assert(CXNetworkCollectNeighbors(
		net,
		source01,
		2,
		CXNeighborDirectionOut,
		CXFalse,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 2);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[2]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[3]) != (CXSize)-1);

	assert(CXNetworkCollectNeighbors(
		net,
		source01,
		2,
		CXNeighborDirectionOut,
		CXTrue,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 3);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[1]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[2]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[3]) != (CXSize)-1);

	assert(CXNetworkCollectNeighborsAtLevel(
		net,
		source0,
		1,
		CXNeighborDirectionOut,
		2,
		CXFalse,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 2);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[3]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[4]) != (CXSize)-1);

	assert(CXNetworkCollectNeighborsUpToLevel(
		net,
		source0,
		1,
		CXNeighborDirectionOut,
		2,
		CXFalse,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 4);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[1]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[2]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[3]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[4]) != (CXSize)-1);

	assert(CXNetworkCollectNeighborsAtLevel(
		net,
		source0,
		1,
		CXNeighborDirectionOut,
		0,
		CXTrue,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 1);
	assert(CXNodeSelectorData(nodeSelector)[0] == nodes[0]);
	assert(CXEdgeSelectorCount(edgeSelector) == 0);

	CXIndex source5[1] = { nodes[5] };
	assert(CXNetworkCollectNeighborsAtLevel(
		net,
		source5,
		1,
		CXNeighborDirectionIn,
		1,
		CXFalse,
		nodeSelector,
		edgeSelector
	));
	assert(CXNodeSelectorCount(nodeSelector) == 2);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[3]) != (CXSize)-1);
	assert(find_node_position(CXNodeSelectorData(nodeSelector), CXNodeSelectorCount(nodeSelector), nodes[4]) != (CXSize)-1);

	CXNodeSelectorDestroy(nodeSelector);
	CXEdgeSelectorDestroy(edgeSelector);
	CXFreeNetwork(net);
}

static void test_attributes(void) {
	CXNetworkRef net = CXNewNetwork(CXFalse);
	assert(net);
	assert(CXNetworkDefineNodeAttribute(net, "weight", CXDoubleAttributeType, 1));
	assert(CXNetworkDefineEdgeAttribute(net, "flag", CXBooleanAttributeType, 1));

	CXIndex nodeIds[2];
	assert(CXNetworkAddNodes(net, 2, nodeIds));
	CXEdge edge = { .from = nodeIds[0], .to = nodeIds[1] };
	CXIndex edgeId;
	assert(CXNetworkAddEdges(net, &edge, 1, &edgeId));

	double *weights = (double *)CXNetworkGetNodeAttributeBuffer(net, "weight");
	assert(weights);
	weights[nodeIds[0]] = 3.14;
	weights[nodeIds[1]] = 2.71;

	uint8_t *flags = (uint8_t *)CXNetworkGetEdgeAttributeBuffer(net, "flag");
	assert(flags);
	flags[edgeId] = 1;

	CXFreeNetwork(net);
}

typedef struct {
	CXEdge edge;
	CXIndex index;
} EdgeRecord;

static CXSize find_node_position(const CXIndex *nodes, CXSize count, uint64_t needle) {
	for (CXSize i = 0; i < count; i++) {
		if ((uint64_t)nodes[i] == needle) {
			return i;
		}
	}
	return (CXSize)-1;
}

static const EdgeRecord *find_edge_record(const EdgeRecord *edges, CXSize count, uint64_t needle) {
	for (CXSize i = 0; i < count; i++) {
		if ((uint64_t)edges[i].index == needle) {
			return &edges[i];
		}
	}
	return NULL;
}

static double random_unit(void) {
	return (double)rand() / (double)RAND_MAX;
}

static void randomize_attribute_block(uint8_t *base, CXSize dimension, CXSize elementStride, double seed) {
	for (CXSize d = 0; d < dimension; d++) {
		double value = seed + (double)d * 0.373;
		memcpy(base + d * elementStride, &value, elementStride);
	}
}

static void randomize_node_attributes(CXNetworkRef net, CXAttributeRef attr) {
	if (!attr || !attr->data) {
		return;
	}
	for (CXSize i = 0; i < net->nodeCapacity; i++) {
		if (!net->nodeActive || !net->nodeActive[i]) {
			continue;
		}
		uint8_t *dest = (uint8_t *)attr->data + (size_t)i * attr->stride;
		randomize_attribute_block(dest, attr->dimension, attr->elementSize, random_unit());
	}
}

static void randomize_edge_attributes(CXNetworkRef net, CXAttributeRef attr) {
	if (!attr || !attr->data) {
		return;
	}
	for (CXSize i = 0; i < net->edgeCapacity; i++) {
		if (!net->edgeActive || !net->edgeActive[i]) {
			continue;
		}
		uint8_t *dest = (uint8_t *)attr->data + (size_t)i * attr->stride;
		randomize_attribute_block(dest, attr->dimension, attr->elementSize, random_unit());
	}
}

static void randomize_network_attributes(CXAttributeRef attr) {
	if (!attr || !attr->data) {
		return;
	}
	randomize_attribute_block((uint8_t *)attr->data, attr->dimension, attr->elementSize, random_unit());
}

static void build_random_network(CXNetworkRef net, CXSize targetNodes) {
	CXIndex *nodeIds = NULL;
	if (targetNodes > 0) {
		nodeIds = malloc(sizeof(CXIndex) * targetNodes);
		assert(nodeIds);
		assert(CXNetworkAddNodes(net, targetNodes, nodeIds));
	}

	assert(CXNetworkDefineNodeAttribute(net, "node_weight", CXDoubleAttributeType, 3));
	assert(CXNetworkDefineNodeAttribute(net, "node_flag", CXUnsignedIntegerAttributeType, 1));
	assert(CXNetworkDefineEdgeAttribute(net, "edge_weight", CXDoubleAttributeType, 2));
	assert(CXNetworkDefineEdgeAttribute(net, "edge_flag", CXUnsignedIntegerAttributeType, 1));
	assert(CXNetworkDefineNetworkAttribute(net, "graph_score", CXDoubleAttributeType, 2));

	CXEdge *edges = NULL;
	CXSize edgeCount = 0;
	if (targetNodes > 1) {
		CXSize capacity = targetNodes * 2;
		edges = malloc(sizeof(CXEdge) * capacity);
		assert(edges);
		for (CXSize i = 0; i < targetNodes; i++) {
			for (CXSize j = 0; j < targetNodes; j++) {
				if (i == j) {
					continue;
				}
				if (random_unit() < 0.3) {
					if (edgeCount >= capacity) {
						capacity *= 2;
						edges = realloc(edges, sizeof(CXEdge) * capacity);
						assert(edges);
					}
					edges[edgeCount].from = nodeIds[i];
					edges[edgeCount].to = nodeIds[j];
					edgeCount++;
				}
			}
		}
	}
	if (edgeCount > 0) {
		assert(CXNetworkAddEdges(net, edges, edgeCount, NULL));
	}
	free(edges);

	CXAttributeRef nodeWeight = CXNetworkGetNodeAttribute(net, "node_weight");
	randomize_node_attributes(net, nodeWeight);
	CXAttributeRef nodeFlag = CXNetworkGetNodeAttribute(net, "node_flag");
	randomize_node_attributes(net, nodeFlag);
	CXAttributeRef edgeWeight = CXNetworkGetEdgeAttribute(net, "edge_weight");
	randomize_edge_attributes(net, edgeWeight);
	CXAttributeRef edgeFlag = CXNetworkGetEdgeAttribute(net, "edge_flag");
	randomize_edge_attributes(net, edgeFlag);
	CXAttributeRef score = CXNetworkGetNetworkAttribute(net, "graph_score");
	randomize_network_attributes(score);

	if (targetNodes > 0) {
		CXIndex *toRemove = malloc(sizeof(CXIndex) * targetNodes);
		assert(toRemove);
		CXSize removeCount = 0;
		for (CXSize i = 0; i < targetNodes; i++) {
			if (random_unit() < 0.2) {
				toRemove[removeCount++] = nodeIds[i];
			}
		}
		if (removeCount > 0) {
			assert(CXNetworkRemoveNodes(net, toRemove, removeCount));
		}
		free(toRemove);
	}

	if (net->edgeCapacity > 0) {
		CXIndex *edgesToRemove = malloc(sizeof(CXIndex) * net->edgeCapacity);
		assert(edgesToRemove);
		CXSize removeEdgeCount = 0;
		for (CXSize i = 0; i < net->edgeCapacity; i++) {
			if (net->edgeActive && net->edgeActive[i] && random_unit() < 0.15) {
				edgesToRemove[removeEdgeCount++] = (CXIndex)i;
			}
		}
		if (removeEdgeCount > 0) {
			assert(CXNetworkRemoveEdges(net, edgesToRemove, removeEdgeCount));
		}
		free(edgesToRemove);
	}

	free(nodeIds);
}

static CXIndex* collect_active_nodes(CXNetworkRef net, CXSize *outCount) {
	CXSize count = 0;
	for (CXSize i = 0; i < net->nodeCapacity; i++) {
		if (net->nodeActive && net->nodeActive[i]) {
			count++;
		}
	}
	CXIndex *nodes = NULL;
	if (count > 0) {
		nodes = malloc(sizeof(CXIndex) * count);
		assert(nodes);
	}
	CXSize write = 0;
	for (CXSize i = 0; i < net->nodeCapacity; i++) {
		if (net->nodeActive && net->nodeActive[i]) {
			nodes[write++] = (CXIndex)i;
		}
	}
	*outCount = count;
	return nodes;
}

static EdgeRecord* collect_active_edges(CXNetworkRef net, CXSize *outCount) {
	CXSize count = 0;
	for (CXSize i = 0; i < net->edgeCapacity; i++) {
		if (net->edgeActive && net->edgeActive[i]) {
			count++;
		}
	}
	EdgeRecord *edges = NULL;
	if (count > 0) {
		edges = malloc(sizeof(EdgeRecord) * count);
		assert(edges);
	}
	CXSize write = 0;
	for (CXSize i = 0; i < net->edgeCapacity; i++) {
		if (net->edgeActive && net->edgeActive[i]) {
			edges[write].edge = net->edges[i];
			edges[write].index = (CXIndex)i;
			write++;
		}
	}
	*outCount = count;
	return edges;
}

static void ensure_attribute_layouts_match(CXStringDictionaryRef original, CXStringDictionaryRef reloaded) {
	CXStringDictionaryFOR(entry, original) {
		CXAttributeRef attr = (CXAttributeRef)entry->data;
		CXAttributeRef other = (CXAttributeRef)CXStringDictionaryEntryForKey(reloaded, entry->key);
		assert(other);
		assert(attr->type == other->type);
		assert(attr->dimension == other->dimension);
		assert(attr->elementSize == other->elementSize);
		assert(attr->stride == other->stride);
	}
}

static void compare_attributes(CXNetworkRef original, CXNetworkRef reloaded) {
	ensure_attribute_layouts_match(original->nodeAttributes, reloaded->nodeAttributes);
	ensure_attribute_layouts_match(original->edgeAttributes, reloaded->edgeAttributes);
	ensure_attribute_layouts_match(original->networkAttributes, reloaded->networkAttributes);

	CXStringDictionaryFOR(entry, original->nodeAttributes) {
		CXAttributeRef a = (CXAttributeRef)entry->data;
		CXAttributeRef b = CXNetworkGetNodeAttribute(reloaded, entry->key);
		if (!a->data || !b || !b->data) {
			continue;
		}
		for (CXSize i = 0; i < original->nodeCapacity; i++) {
			if (!original->nodeActive || !original->nodeActive[i]) {
				continue;
			}
			assert(memcmp((uint8_t *)b->data + (size_t)i * b->stride, (uint8_t *)a->data + (size_t)i * a->stride, a->stride) == 0);
		}
	}

	CXStringDictionaryFOR(entry, original->edgeAttributes) {
		CXAttributeRef a = (CXAttributeRef)entry->data;
		CXAttributeRef b = CXNetworkGetEdgeAttribute(reloaded, entry->key);
		if (!a->data || !b || !b->data) {
			continue;
		}
		for (CXSize i = 0; i < original->edgeCapacity; i++) {
			if (!original->edgeActive || !original->edgeActive[i]) {
				continue;
			}
			assert(memcmp((uint8_t *)b->data + (size_t)i * b->stride, (uint8_t *)a->data + (size_t)i * a->stride, a->stride) == 0);
		}
	}

	CXStringDictionaryFOR(entry, original->networkAttributes) {
		CXAttributeRef a = (CXAttributeRef)entry->data;
		CXAttributeRef b = CXNetworkGetNetworkAttribute(reloaded, entry->key);
		if (!a->data || !b || !b->data) {
			continue;
		}
		assert(memcmp(b->data, a->data, a->stride) == 0);
	}
}

static void verify_compaction(CXNetworkRef net, const CXIndex *activeNodes, CXSize nodeActiveCount, const EdgeRecord *activeEdges, CXSize edgeActiveCount) {
	assert(CXNetworkCompact(net, "__orig_node", "__orig_edge"));
	if (nodeActiveCount > 0) {
		if (net->nodeCapacity != nodeActiveCount) {
			fprintf(stderr, "Node capacity mismatch: %zu vs %zu\n", (size_t)net->nodeCapacity, (size_t)nodeActiveCount);
		}
		assert(net->nodeCapacity == nodeActiveCount);
	} else {
		if (net->nodeCapacity != 0) {
			fprintf(stderr, "Expected zero node capacity but found %zu\n", (size_t)net->nodeCapacity);
		}
		assert(net->nodeCapacity == 0);
	}
	if (edgeActiveCount > 0) {
		if (net->edgeCapacity != edgeActiveCount) {
			fprintf(stderr, "Edge capacity mismatch: %zu vs %zu\n", (size_t)net->edgeCapacity, (size_t)edgeActiveCount);
		}
		assert(net->edgeCapacity == edgeActiveCount);
	} else {
		if (net->edgeCapacity != 0) {
			fprintf(stderr, "Expected zero edge capacity but found %zu\n", (size_t)net->edgeCapacity);
		}
		assert(net->edgeCapacity == 0);
	}
	for (CXSize i = 0; i < net->nodeCapacity; i++) {
		assert(net->nodeActive[i]);
	}
	for (CXSize i = 0; i < net->edgeCapacity; i++) {
		assert(net->edgeActive[i]);
	}

	CXAttributeRef nodeAttr = CXNetworkGetNodeAttribute(net, "__orig_node");
	if (nodeActiveCount > 0) {
		assert(nodeAttr && nodeAttr->data);
		uint8_t *seen = calloc(nodeActiveCount, sizeof(uint8_t));
		assert(seen);
		for (CXSize i = 0; i < nodeActiveCount; i++) {
			uint64_t originalIndex = 0;
			if (nodeAttr->elementSize == sizeof(uint32_t)) {
				originalIndex = (uint64_t)((const uint32_t *)nodeAttr->data)[i];
			} else {
				originalIndex = ((const uint64_t *)nodeAttr->data)[i];
			}
			CXSize pos = find_node_position(activeNodes, nodeActiveCount, originalIndex);
			assert(pos != (CXSize)-1);
			assert(!seen[pos]);
			seen[pos] = 1;
		}
		for (CXSize i = 0; i < nodeActiveCount; i++) {
			assert(seen[i]);
		}
		free(seen);
	}
	CXAttributeRef edgeAttr = CXNetworkGetEdgeAttribute(net, "__orig_edge");
	if (edgeActiveCount > 0) {
		assert(edgeAttr && edgeAttr->data);
		const void *nodeOrigData = nodeAttr && nodeAttr->data ? nodeAttr->data : NULL;
		uint8_t *seen = calloc(edgeActiveCount, sizeof(uint8_t));
		assert(seen);
		for (CXSize i = 0; i < edgeActiveCount; i++) {
			uint64_t originalEdgeIndex = 0;
			if (edgeAttr->elementSize == sizeof(uint32_t)) {
				originalEdgeIndex = (uint64_t)((const uint32_t *)edgeAttr->data)[i];
			} else {
				originalEdgeIndex = ((const uint64_t *)edgeAttr->data)[i];
			}
			const EdgeRecord *record = find_edge_record(activeEdges, edgeActiveCount, originalEdgeIndex);
			assert(record);
			CXSize pos = (CXSize)(record - activeEdges);
			assert(pos < edgeActiveCount);
			assert(!seen[pos]);
			seen[pos] = 1;

			CXEdge expected = record->edge;
			CXEdge actual = net->edges[i];
			if (nodeOrigData) {
				uint64_t fromOrig = 0;
				uint64_t toOrig = 0;
				if (nodeAttr->elementSize == sizeof(uint32_t)) {
					const uint32_t *nodeOrig = (const uint32_t *)nodeOrigData;
					fromOrig = (uint64_t)nodeOrig[actual.from];
					toOrig = (uint64_t)nodeOrig[actual.to];
				} else {
					const uint64_t *nodeOrig = (const uint64_t *)nodeOrigData;
					fromOrig = nodeOrig[actual.from];
					toOrig = nodeOrig[actual.to];
				}
				assert(fromOrig == (uint64_t)expected.from);
				assert(toOrig == (uint64_t)expected.to);
			} else {
				assert(actual.from == expected.from);
				assert(actual.to == expected.to);
			}
		}
		for (CXSize i = 0; i < edgeActiveCount; i++) {
			assert(seen[i]);
		}
		free(seen);
	}
}

static void verify_round_trip(CXNetworkRef net) {
	CXSize nodeActiveCount = 0;
	CXIndex *activeNodes = collect_active_nodes(net, &nodeActiveCount);
	CXSize edgeActiveCount = 0;
	EdgeRecord *activeEdges = collect_active_edges(net, &edgeActiveCount);

	char bxTemplate[] = "/tmp/cxnet-bx-XXXXXX";
	int bxFd = mkstemp(bxTemplate);
	assert(bxFd >= 0);
	close(bxFd);

	char zxTemplate[] = "/tmp/cxnet-zx-XXXXXX";
	int zxFd = mkstemp(zxTemplate);
	assert(zxFd >= 0);
	close(zxFd);

	assert(CXNetworkWriteBXNet(net, bxTemplate));
	assert(CXNetworkWriteZXNet(net, zxTemplate, 4));

	CXNetworkRef loadedBx = CXNetworkReadBXNet(bxTemplate);
	assert(loadedBx);
	CXNetworkRef loadedZx = CXNetworkReadZXNet(zxTemplate);
	assert(loadedZx);

	unlink(bxTemplate);
	unlink(zxTemplate);

	assert(loadedBx->nodeCount == net->nodeCount);
	assert(loadedBx->edgeCount == net->edgeCount);
	assert(loadedBx->isDirected == net->isDirected);
	assert(loadedZx->nodeCount == net->nodeCount);
	assert(loadedZx->edgeCount == net->edgeCount);
	assert(loadedZx->isDirected == net->isDirected);

	compare_attributes(net, loadedBx);
	compare_attributes(net, loadedZx);

	verify_compaction(loadedZx, activeNodes, nodeActiveCount, activeEdges, edgeActiveCount);

	free(activeNodes);
	free(activeEdges);

	CXFreeNetwork(loadedBx);
	CXFreeNetwork(loadedZx);
}

static void test_xnet_round_trip(void) {
	CXNetworkRef net = CXNewNetwork(CXTrue);
	assert(net);

	CXIndex nodes[3];
	assert(CXNetworkAddNodes(net, 3, nodes));

	CXEdge edges[2] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[1], .to = nodes[2] },
	};
	CXIndex edgeIds[2];
	assert(CXNetworkAddEdges(net, edges, 2, edgeIds));

	assert(CXNetworkDefineNodeAttribute(net, "score", CXFloatAttributeType, 1));
	float *scores = (float *)CXNetworkGetNodeAttributeBuffer(net, "score");
	scores[nodes[0]] = 0.5f;
	scores[nodes[1]] = 1.5f;
	scores[nodes[2]] = 2.5f;

	assert(CXNetworkDefineNodeAttribute(net, "label", CXStringAttributeType, 1));
	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "label");
	labels[nodes[0]] = CXNewStringFromString("Alpha");
	labels[nodes[1]] = CXNewStringFromString("Beta Value");
	labels[nodes[2]] = CXNewStringFromString("Gamma#Tag");

	assert(CXNetworkDefineNodeAttribute(net, "coord", CXIntegerAttributeType, 3));
	int32_t *coords = (int32_t *)CXNetworkGetNodeAttributeBuffer(net, "coord");
	for (int i = 0; i < 3; i++) {
		coords[nodes[i] * 3 + 0] = i;
		coords[nodes[i] * 3 + 1] = i + 10;
		coords[nodes[i] * 3 + 2] = i + 20;
	}

	assert(CXNetworkDefineEdgeAttribute(net, "weight", CXFloatAttributeType, 1));
	float *edgeWeights = (float *)CXNetworkGetEdgeAttributeBuffer(net, "weight");
	edgeWeights[edgeIds[0]] = 3.25f;
	edgeWeights[edgeIds[1]] = 4.75f;

	assert(CXNetworkDefineEdgeAttribute(net, "tag", CXStringAttributeType, 1));
	CXString *tags = (CXString *)CXNetworkGetEdgeAttributeBuffer(net, "tag");
	tags[edgeIds[0]] = CXNewStringFromString("fast");
	tags[edgeIds[1]] = CXNewStringFromString("slow\npath");

	assert(CXNetworkDefineNetworkAttribute(net, "description", CXStringAttributeType, 1));
	CXString *desc = (CXString *)CXNetworkGetNetworkAttributeBuffer(net, "description");
	desc[0] = CXNewStringFromString("Round trip test");

	char xnetTemplate[] = "/tmp/cxnet-xnet-XXXXXX";
	int xnetFd = mkstemp(xnetTemplate);
	assert(xnetFd >= 0);
	close(xnetFd);

	assert(CXNetworkWriteXNet(net, xnetTemplate));
	release_all_string_attributes(net);
	CXFreeNetwork(net);

	CXNetworkRef loaded = CXNetworkReadXNet(xnetTemplate);
	assert(loaded);
	unlink(xnetTemplate);

	assert(loaded->nodeCount == 3);
	assert(loaded->edgeCount == 2);
	assert(CXNetworkIsDirected(loaded));

	float *loadedScores = (float *)CXNetworkGetNodeAttributeBuffer(loaded, "score");
	assert(loadedScores);
	assert(fabsf(loadedScores[0] - 0.5f) < 1e-6f);
	assert(fabsf(loadedScores[1] - 1.5f) < 1e-6f);
	assert(fabsf(loadedScores[2] - 2.5f) < 1e-6f);

	CXString *loadedLabels = (CXString *)CXNetworkGetNodeAttributeBuffer(loaded, "label");
	assert(loadedLabels);
	assert(strcmp(loadedLabels[0], "Alpha") == 0);
	assert(strcmp(loadedLabels[1], "Beta Value") == 0);
	assert(strcmp(loadedLabels[2], "Gamma#Tag") == 0);

	int32_t *loadedCoords = (int32_t *)CXNetworkGetNodeAttributeBuffer(loaded, "coord");
	assert(loadedCoords);
	for (int i = 0; i < 3; i++) {
		assert(loadedCoords[i * 3 + 0] == i);
		assert(loadedCoords[i * 3 + 1] == i + 10);
		assert(loadedCoords[i * 3 + 2] == i + 20);
	}

	float *loadedEdgeWeights = (float *)CXNetworkGetEdgeAttributeBuffer(loaded, "weight");
	assert(loadedEdgeWeights);
	assert(fabsf(loadedEdgeWeights[0] - 3.25f) < 1e-6f);
	assert(fabsf(loadedEdgeWeights[1] - 4.75f) < 1e-6f);

	CXString *loadedTags = (CXString *)CXNetworkGetEdgeAttributeBuffer(loaded, "tag");
	assert(loadedTags);
	assert(strcmp(loadedTags[0], "fast") == 0);
	assert(strcmp(loadedTags[1], "slow\npath") == 0);

	CXString *loadedDesc = (CXString *)CXNetworkGetNetworkAttributeBuffer(loaded, "description");
	assert(loadedDesc && loadedDesc[0]);
	assert(strcmp(loadedDesc[0], "Round trip test") == 0);

	CXString *originalIds = (CXString *)CXNetworkGetNodeAttributeBuffer(loaded, "_original_ids_");
	assert(originalIds);
	assert(strcmp(originalIds[0], "0") == 0);
	assert(strcmp(originalIds[1], "1") == 0);
	assert(strcmp(originalIds[2], "2") == 0);

	release_all_string_attributes(loaded);
	CXFreeNetwork(loaded);
}

static void test_categorical_helpers(void) {
	CXNetworkRef net = CXNewNetwork(CXFalse);
	assert(net);

	CXIndex nodes[3];
	assert(CXNetworkAddNodes(net, 3, nodes));
	assert(CXNetworkDefineNodeAttribute(net, "group", CXStringAttributeType, 1));

	CXString *values = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "group");
	values[nodes[0]] = CXNewStringFromString("alpha");
	values[nodes[1]] = CXNewStringFromString("");
	values[nodes[2]] = CXNewStringFromString("__NA__");

	assert(CXNetworkCategorizeAttribute(net, CXAttributeScopeNode, "group", CX_CATEGORY_SORT_FREQUENCY, "__NA__"));
	CXAttributeRef attr = CXNetworkGetNodeAttribute(net, "group");
	assert(attr && attr->type == CXDataAttributeCategoryType);
	int32_t *codes = (int32_t *)CXNetworkGetNodeAttributeBuffer(net, "group");
	assert(codes);
	assert(codes[nodes[0]] >= 0);
	assert(codes[nodes[1]] == -1);
	assert(codes[nodes[2]] == -1);

	assert(CXNetworkDecategorizeAttribute(net, CXAttributeScopeNode, "group", "__NA__"));
	CXString *restored = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "group");
	assert(restored);
	assert(strcmp(restored[nodes[0]], "alpha") == 0);
	assert(strcmp(restored[nodes[1]], "__NA__") == 0);
	assert(strcmp(restored[nodes[2]], "__NA__") == 0);

	release_all_string_attributes(net);
	CXFreeNetwork(net);
}

static void test_categorical_serialization(void) {
	CXNetworkRef net = CXNewNetwork(CXFalse);
	assert(net);

	CXIndex nodes[3];
	assert(CXNetworkAddNodes(net, 3, nodes));

	CXEdge edges[2] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[1], .to = nodes[2] },
	};
	CXIndex edgeIds[2];
	assert(CXNetworkAddEdges(net, edges, 2, edgeIds));

	assert(CXNetworkDefineNodeAttribute(net, "kind", CXDataAttributeCategoryType, 1));
	int32_t *kinds = (int32_t *)CXNetworkGetNodeAttributeBuffer(net, "kind");
	assert(kinds);
	kinds[nodes[0]] = 0;
	kinds[nodes[1]] = 2;
	kinds[nodes[2]] = 0;
	const char *kindLabels[] = { "Apple", "Banana" };
	int32_t kindIds[] = { 0, 2 };
	assert(CXNetworkSetAttributeCategoryDictionary(net, CXAttributeScopeNode, "kind", kindLabels, kindIds, 2, CXFalse));

	assert(CXNetworkDefineEdgeAttribute(net, "etype", CXDataAttributeCategoryType, 1));
	int32_t *etypes = (int32_t *)CXNetworkGetEdgeAttributeBuffer(net, "etype");
	assert(etypes);
	etypes[edgeIds[0]] = 1;
	etypes[edgeIds[1]] = 3;
	const char *etypeLabels[] = { "Fast", "Slow" };
	int32_t etypeIds[] = { 1, 3 };
	assert(CXNetworkSetAttributeCategoryDictionary(net, CXAttributeScopeEdge, "etype", etypeLabels, etypeIds, 2, CXFalse));

	assert(CXNetworkDefineNetworkAttribute(net, "family", CXDataAttributeCategoryType, 1));
	int32_t *family = (int32_t *)CXNetworkGetNetworkAttributeBuffer(net, "family");
	assert(family);
	family[0] = 7;
	const char *familyLabels[] = { "Group7" };
	int32_t familyIds[] = { 7 };
	assert(CXNetworkSetAttributeCategoryDictionary(net, CXAttributeScopeNetwork, "family", familyLabels, familyIds, 1, CXFalse));

	char xnetPath[] = "/tmp/cxnet-cat-XXXXXX";
	int xnetFd = mkstemp(xnetPath);
	assert(xnetFd >= 0);
	close(xnetFd);
	assert(CXNetworkWriteXNet(net, xnetPath));
	CXNetworkRef xnetLoaded = CXNetworkReadXNet(xnetPath);
	assert(xnetLoaded);
	unlink(xnetPath);

	CXAttributeRef loadedKind = CXNetworkGetNodeAttribute(xnetLoaded, "kind");
	assert(loadedKind && loadedKind->categoricalDictionary);
	assert(CXStringDictionaryCount(loadedKind->categoricalDictionary) == 2);
	int32_t id = 0;
	assert(lookup_category_id(loadedKind->categoricalDictionary, "Apple", &id) && id == 0);
	assert(lookup_category_id(loadedKind->categoricalDictionary, "Banana", &id) && id == 2);
	int32_t *loadedKinds = (int32_t *)CXNetworkGetNodeAttributeBuffer(xnetLoaded, "kind");
	assert(loadedKinds);
	assert(loadedKinds[0] == 0);
	assert(loadedKinds[1] == 2);
	assert(loadedKinds[2] == 0);

	CXAttributeRef loadedEtype = CXNetworkGetEdgeAttribute(xnetLoaded, "etype");
	assert(loadedEtype && loadedEtype->categoricalDictionary);
	assert(CXStringDictionaryCount(loadedEtype->categoricalDictionary) == 2);
	assert(lookup_category_id(loadedEtype->categoricalDictionary, "Fast", &id) && id == 1);
	assert(lookup_category_id(loadedEtype->categoricalDictionary, "Slow", &id) && id == 3);
	int32_t *loadedEtypes = (int32_t *)CXNetworkGetEdgeAttributeBuffer(xnetLoaded, "etype");
	assert(loadedEtypes);
	assert(loadedEtypes[0] == 1);
	assert(loadedEtypes[1] == 3);

	CXAttributeRef loadedFamily = CXNetworkGetNetworkAttribute(xnetLoaded, "family");
	assert(loadedFamily && loadedFamily->categoricalDictionary);
	assert(CXStringDictionaryCount(loadedFamily->categoricalDictionary) == 1);
	assert(lookup_category_id(loadedFamily->categoricalDictionary, "Group7", &id) && id == 7);
	int32_t *loadedFamilyValues = (int32_t *)CXNetworkGetNetworkAttributeBuffer(xnetLoaded, "family");
	assert(loadedFamilyValues && loadedFamilyValues[0] == 7);

	release_all_string_attributes(xnetLoaded);
	CXFreeNetwork(xnetLoaded);

	char bxPath[] = "/tmp/cxnet-cat-bx-XXXXXX";
	int bxFd = mkstemp(bxPath);
	assert(bxFd >= 0);
	close(bxFd);
	assert(CXNetworkWriteBXNet(net, bxPath));
	CXNetworkRef bxLoaded = CXNetworkReadBXNet(bxPath);
	assert(bxLoaded);
	unlink(bxPath);

	CXAttributeRef bxKind = CXNetworkGetNodeAttribute(bxLoaded, "kind");
	assert(bxKind && bxKind->categoricalDictionary);
	assert(CXStringDictionaryCount(bxKind->categoricalDictionary) == 2);
	assert(lookup_category_id(bxKind->categoricalDictionary, "Apple", &id) && id == 0);
	assert(lookup_category_id(bxKind->categoricalDictionary, "Banana", &id) && id == 2);
	int32_t *bxKinds = (int32_t *)CXNetworkGetNodeAttributeBuffer(bxLoaded, "kind");
	assert(bxKinds);
	assert(bxKinds[0] == 0);
	assert(bxKinds[1] == 2);
	assert(bxKinds[2] == 0);

	CXAttributeRef bxFamily = CXNetworkGetNetworkAttribute(bxLoaded, "family");
	assert(bxFamily && bxFamily->categoricalDictionary);
	assert(CXStringDictionaryCount(bxFamily->categoricalDictionary) == 1);
	assert(lookup_category_id(bxFamily->categoricalDictionary, "Group7", &id) && id == 7);
	int32_t *bxFamilyValues = (int32_t *)CXNetworkGetNetworkAttributeBuffer(bxLoaded, "family");
	assert(bxFamilyValues && bxFamilyValues[0] == 7);

	CXAttributeRef bxEtype = CXNetworkGetEdgeAttribute(bxLoaded, "etype");
	assert(bxEtype && bxEtype->categoricalDictionary);
	assert(CXStringDictionaryCount(bxEtype->categoricalDictionary) == 2);
	assert(lookup_category_id(bxEtype->categoricalDictionary, "Fast", &id) && id == 1);
	assert(lookup_category_id(bxEtype->categoricalDictionary, "Slow", &id) && id == 3);
	int32_t *bxEtypes = (int32_t *)CXNetworkGetEdgeAttributeBuffer(bxLoaded, "etype");
	assert(bxEtypes);
	assert(bxEtypes[0] == 1);
	assert(bxEtypes[1] == 3);

	CXFreeNetwork(bxLoaded);

	char zxPath[] = "/tmp/cxnet-cat-zx-XXXXXX";
	int zxFd = mkstemp(zxPath);
	assert(zxFd >= 0);
	close(zxFd);
	assert(CXNetworkWriteZXNet(net, zxPath, 4));
	CXNetworkRef zxLoaded = CXNetworkReadZXNet(zxPath);
	assert(zxLoaded);
	unlink(zxPath);

	CXAttributeRef zxKind = CXNetworkGetNodeAttribute(zxLoaded, "kind");
	assert(zxKind && zxKind->categoricalDictionary);
	assert(CXStringDictionaryCount(zxKind->categoricalDictionary) == 2);
	assert(lookup_category_id(zxKind->categoricalDictionary, "Apple", &id) && id == 0);
	assert(lookup_category_id(zxKind->categoricalDictionary, "Banana", &id) && id == 2);
	int32_t *zxKinds = (int32_t *)CXNetworkGetNodeAttributeBuffer(zxLoaded, "kind");
	assert(zxKinds);
	assert(zxKinds[0] == 0);
	assert(zxKinds[1] == 2);
	assert(zxKinds[2] == 0);

	CXAttributeRef zxFamily = CXNetworkGetNetworkAttribute(zxLoaded, "family");
	assert(zxFamily && zxFamily->categoricalDictionary);
	assert(CXStringDictionaryCount(zxFamily->categoricalDictionary) == 1);
	assert(lookup_category_id(zxFamily->categoricalDictionary, "Group7", &id) && id == 7);
	int32_t *zxFamilyValues = (int32_t *)CXNetworkGetNetworkAttributeBuffer(zxLoaded, "family");
	assert(zxFamilyValues && zxFamilyValues[0] == 7);

	CXAttributeRef zxEtype = CXNetworkGetEdgeAttribute(zxLoaded, "etype");
	assert(zxEtype && zxEtype->categoricalDictionary);
	assert(CXStringDictionaryCount(zxEtype->categoricalDictionary) == 2);
	assert(lookup_category_id(zxEtype->categoricalDictionary, "Fast", &id) && id == 1);
	assert(lookup_category_id(zxEtype->categoricalDictionary, "Slow", &id) && id == 3);
	int32_t *zxEtypes = (int32_t *)CXNetworkGetEdgeAttributeBuffer(zxLoaded, "etype");
	assert(zxEtypes);
	assert(zxEtypes[0] == 1);
	assert(zxEtypes[1] == 3);

	CXFreeNetwork(zxLoaded);
	CXFreeNetwork(net);
}

static void test_multicategory_serialization(void) {
	CXNetworkRef net = CXNewNetwork(CXFalse);
	assert(net);

	CXIndex nodes[3];
	assert(CXNetworkAddNodes(net, 3, nodes));

	CXEdge edges[2] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[1], .to = nodes[2] },
	};
	CXIndex edgeIds[2];
	assert(CXNetworkAddEdges(net, edges, 2, edgeIds));

	assert(CXNetworkDefineMultiCategoryAttribute(net, CXAttributeScopeNode, "tags", CXFalse));
	const char *tags0[] = { "alpha", "beta" };
	assert(CXNetworkSetMultiCategoryEntryByLabels(net, CXAttributeScopeNode, "tags", nodes[0], tags0, 2, NULL));
	assert(CXNetworkSetMultiCategoryEntryByLabels(net, CXAttributeScopeNode, "tags", nodes[1], NULL, 0, NULL));
	const char *tags2[] = { "beta" };
	assert(CXNetworkSetMultiCategoryEntryByLabels(net, CXAttributeScopeNode, "tags", nodes[2], tags2, 1, NULL));

	assert(CXNetworkDefineMultiCategoryAttribute(net, CXAttributeScopeEdge, "topics", CXTrue));
	const char *topics0[] = { "x", "y" };
	float topicWeights0[] = { 0.2f, 0.8f };
	assert(CXNetworkSetMultiCategoryEntryByLabels(net, CXAttributeScopeEdge, "topics", edgeIds[0], topics0, 2, topicWeights0));
	const char *topics1[] = { "y" };
	float topicWeights1[] = { 1.0f };
	assert(CXNetworkSetMultiCategoryEntryByLabels(net, CXAttributeScopeEdge, "topics", edgeIds[1], topics1, 1, topicWeights1));

	char xnetPath[] = "/tmp/cxnet-mc-XXXXXX";
	int xnetFd = mkstemp(xnetPath);
	assert(xnetFd >= 0);
	close(xnetFd);
	assert(CXNetworkWriteXNet(net, xnetPath));
	CXNetworkRef xnetLoaded = CXNetworkReadXNet(xnetPath);
	assert(xnetLoaded);
	unlink(xnetPath);

	CXAttributeRef tagsAttr = CXNetworkGetNodeAttribute(xnetLoaded, "tags");
	assert(tagsAttr && tagsAttr->type == CXDataAttributeMultiCategoryType);
	int32_t alphaId = 0;
	int32_t betaId = 0;
	assert(lookup_category_id(tagsAttr->categoricalDictionary, "alpha", &alphaId));
	assert(lookup_category_id(tagsAttr->categoricalDictionary, "beta", &betaId));
	uint32_t *tagOffsets = CXNetworkGetMultiCategoryOffsets(xnetLoaded, CXAttributeScopeNode, "tags");
	uint32_t *tagIds = CXNetworkGetMultiCategoryIds(xnetLoaded, CXAttributeScopeNode, "tags");
	assert(tagOffsets && tagIds);
	assert(tagOffsets[0] == 0 && tagOffsets[1] == 2 && tagOffsets[2] == 2 && tagOffsets[3] == 3);
	assert(tagIds[0] == (uint32_t)alphaId);
	assert(tagIds[1] == (uint32_t)betaId);
	assert(tagIds[2] == (uint32_t)betaId);

	CXAttributeRef topicsAttr = CXNetworkGetEdgeAttribute(xnetLoaded, "topics");
	assert(topicsAttr && topicsAttr->type == CXDataAttributeMultiCategoryType);
	assert(CXNetworkMultiCategoryHasWeights(xnetLoaded, CXAttributeScopeEdge, "topics"));
	int32_t xId = 0;
	int32_t yId = 0;
	assert(lookup_category_id(topicsAttr->categoricalDictionary, "x", &xId));
	assert(lookup_category_id(topicsAttr->categoricalDictionary, "y", &yId));
	uint32_t *topicOffsets = CXNetworkGetMultiCategoryOffsets(xnetLoaded, CXAttributeScopeEdge, "topics");
	uint32_t *topicIds = CXNetworkGetMultiCategoryIds(xnetLoaded, CXAttributeScopeEdge, "topics");
	float *topicWeights = CXNetworkGetMultiCategoryWeights(xnetLoaded, CXAttributeScopeEdge, "topics");
	assert(topicOffsets && topicIds && topicWeights);
	assert(topicOffsets[0] == 0 && topicOffsets[1] == 2 && topicOffsets[2] == 3);
	assert(topicIds[0] == (uint32_t)xId);
	assert(topicIds[1] == (uint32_t)yId);
	assert(topicIds[2] == (uint32_t)yId);
	assert(fabsf(topicWeights[0] - 0.2f) < 1e-6f);
	assert(fabsf(topicWeights[1] - 0.8f) < 1e-6f);
	assert(fabsf(topicWeights[2] - 1.0f) < 1e-6f);

	CXFreeNetwork(xnetLoaded);

	char bxPath[] = "/tmp/cxnet-mc-bx-XXXXXX";
	int bxFd = mkstemp(bxPath);
	assert(bxFd >= 0);
	close(bxFd);
	assert(CXNetworkWriteBXNet(net, bxPath));
	CXNetworkRef bxLoaded = CXNetworkReadBXNet(bxPath);
	assert(bxLoaded);
	unlink(bxPath);

	uint32_t *bxTagOffsets = CXNetworkGetMultiCategoryOffsets(bxLoaded, CXAttributeScopeNode, "tags");
	uint32_t *bxTagIds = CXNetworkGetMultiCategoryIds(bxLoaded, CXAttributeScopeNode, "tags");
	assert(bxTagOffsets && bxTagIds);
	assert(bxTagOffsets[0] == 0 && bxTagOffsets[1] == 2 && bxTagOffsets[2] == 2 && bxTagOffsets[3] == 3);

	uint32_t *bxTopicOffsets = CXNetworkGetMultiCategoryOffsets(bxLoaded, CXAttributeScopeEdge, "topics");
	uint32_t *bxTopicIds = CXNetworkGetMultiCategoryIds(bxLoaded, CXAttributeScopeEdge, "topics");
	float *bxTopicWeights = CXNetworkGetMultiCategoryWeights(bxLoaded, CXAttributeScopeEdge, "topics");
	assert(bxTopicOffsets && bxTopicIds && bxTopicWeights);
	assert(bxTopicOffsets[0] == 0 && bxTopicOffsets[1] == 2 && bxTopicOffsets[2] == 3);

	CXFreeNetwork(bxLoaded);

	char zxPath[] = "/tmp/cxnet-mc-zx-XXXXXX";
	int zxFd = mkstemp(zxPath);
	assert(zxFd >= 0);
	close(zxFd);
	assert(CXNetworkWriteZXNet(net, zxPath, 4));
	CXNetworkRef zxLoaded = CXNetworkReadZXNet(zxPath);
	assert(zxLoaded);
	unlink(zxPath);

	uint32_t *zxTagOffsets = CXNetworkGetMultiCategoryOffsets(zxLoaded, CXAttributeScopeNode, "tags");
	uint32_t *zxTagIds = CXNetworkGetMultiCategoryIds(zxLoaded, CXAttributeScopeNode, "tags");
	assert(zxTagOffsets && zxTagIds);
	assert(zxTagOffsets[0] == 0 && zxTagOffsets[1] == 2 && zxTagOffsets[2] == 2 && zxTagOffsets[3] == 3);

	uint32_t *zxTopicOffsets = CXNetworkGetMultiCategoryOffsets(zxLoaded, CXAttributeScopeEdge, "topics");
	uint32_t *zxTopicIds = CXNetworkGetMultiCategoryIds(zxLoaded, CXAttributeScopeEdge, "topics");
	float *zxTopicWeights = CXNetworkGetMultiCategoryWeights(zxLoaded, CXAttributeScopeEdge, "topics");
	assert(zxTopicOffsets && zxTopicIds && zxTopicWeights);
	assert(zxTopicOffsets[0] == 0 && zxTopicOffsets[1] == 2 && zxTopicOffsets[2] == 3);

	CXFreeNetwork(zxLoaded);
	CXFreeNetwork(net);
}

static void test_xnet_legacy_multicategory(void) {
	const char *legacy =
		"#vertices 2\n"
		"\"First\"\n"
		"\"Second\"\n"
		"#edges nonweighted undirected\n"
		"0 1\n"
		"1 0\n"
		"#v \"__multicategoryTags\" s\n"
		"\"alpha;beta%3Bomega\"\n"
		"\"beta%3Bomega\"\n"
		"#e \"__multicategory_weightedTopics\" s\n"
		"\"x:0.1;y%3Aalt:0.9\"\n"
		"\"y%3Aalt:1.0\"\n";

	char legacyPath[] = "/tmp/cxnet-legacy-mc-XXXXXX";
	int fd = mkstemp(legacyPath);
	assert(fd >= 0);
	size_t legacyLen = strlen(legacy);
	assert(write(fd, legacy, legacyLen) == (ssize_t)legacyLen);
	close(fd);

	CXNetworkRef net = CXNetworkReadXNet(legacyPath);
	assert(net);
	unlink(legacyPath);

	CXAttributeRef tagsAttr = CXNetworkGetNodeAttribute(net, "Tags");
	assert(tagsAttr && tagsAttr->type == CXDataAttributeMultiCategoryType);
	int32_t alphaId = 0;
	int32_t betaOmegaId = 0;
	assert(lookup_category_id(tagsAttr->categoricalDictionary, "alpha", &alphaId));
	assert(lookup_category_id(tagsAttr->categoricalDictionary, "beta;omega", &betaOmegaId));
	uint32_t *tagOffsets = CXNetworkGetMultiCategoryOffsets(net, CXAttributeScopeNode, "Tags");
	uint32_t *tagIds = CXNetworkGetMultiCategoryIds(net, CXAttributeScopeNode, "Tags");
	assert(tagOffsets && tagIds);
	assert(tagOffsets[0] == 0 && tagOffsets[1] == 2 && tagOffsets[2] == 3);
	assert(tagIds[0] == (uint32_t)alphaId);
	assert(tagIds[1] == (uint32_t)betaOmegaId);
	assert(tagIds[2] == (uint32_t)betaOmegaId);

	CXAttributeRef topicsAttr = CXNetworkGetEdgeAttribute(net, "Topics");
	assert(topicsAttr && topicsAttr->type == CXDataAttributeMultiCategoryType);
	assert(CXNetworkMultiCategoryHasWeights(net, CXAttributeScopeEdge, "Topics"));
	int32_t xId = 0;
	int32_t yAltId = 0;
	assert(lookup_category_id(topicsAttr->categoricalDictionary, "x", &xId));
	assert(lookup_category_id(topicsAttr->categoricalDictionary, "y:alt", &yAltId));
	uint32_t *topicOffsets = CXNetworkGetMultiCategoryOffsets(net, CXAttributeScopeEdge, "Topics");
	uint32_t *topicIds = CXNetworkGetMultiCategoryIds(net, CXAttributeScopeEdge, "Topics");
	float *topicWeights = CXNetworkGetMultiCategoryWeights(net, CXAttributeScopeEdge, "Topics");
	assert(topicOffsets && topicIds && topicWeights);
	assert(topicOffsets[0] == 0 && topicOffsets[1] == 2 && topicOffsets[2] == 3);
	assert(topicIds[0] == (uint32_t)xId);
	assert(topicIds[1] == (uint32_t)yAltId);
	assert(topicIds[2] == (uint32_t)yAltId);
	assert(fabsf(topicWeights[0] - 0.1f) < 1e-6f);
	assert(fabsf(topicWeights[1] - 0.9f) < 1e-6f);
	assert(fabsf(topicWeights[2] - 1.0f) < 1e-6f);

	CXFreeNetwork(net);
}

static void test_xnet_legacy_upgrade(void) {
	const char *legacy =
		"#vertices 3\n"
		"First\n"
		"Second\n"
		"Third\n"
		"#edges weighted directed\n"
		"0 1 1.25\n"
		"1 2 2.5\n"
		"#v \"Legacy numeric\" n\n"
		"1\n"
		"2\n"
		"3\n"
		"#v \"Legacy strings\" s\n"
		"alpha\n"
		"beta\n"
		"gamma\n"
		"#v \"Group__category\" s\n"
		"alpha\n"
		"\"__NA__\"\n"
		"alpha\n"
		"#e \"kind\" s\n"
		"forward\n"
		"back\n";

	char legacyPath[] = "/tmp/cxnet-legacy-XXXXXX";
	int fd = mkstemp(legacyPath);
	assert(fd >= 0);
	size_t legacyLen = strlen(legacy);
	assert(write(fd, legacy, legacyLen) == (ssize_t)legacyLen);
	close(fd);

	CXNetworkRef net = CXNetworkReadXNet(legacyPath);
	assert(net);
	unlink(legacyPath);

	assert(net->nodeCount == 3);
	assert(net->edgeCount == 2);
	assert(CXNetworkIsDirected(net));

	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "Label");
	assert(labels);
	assert(strcmp(labels[0], "First") == 0);
	assert(strcmp(labels[1], "Second") == 0);
	assert(strcmp(labels[2], "Third") == 0);

	float *weights = (float *)CXNetworkGetEdgeAttributeBuffer(net, "weight");
	assert(weights);
	assert(fabsf(weights[0] - 1.25f) < 1e-6f);
	assert(fabsf(weights[1] - 2.5f) < 1e-6f);

	float *legacyNumeric = (float *)CXNetworkGetNodeAttributeBuffer(net, "Legacy numeric");
	assert(legacyNumeric);
	assert(fabsf(legacyNumeric[0] - 1.0f) < 1e-6f);
	assert(fabsf(legacyNumeric[1] - 2.0f) < 1e-6f);
	assert(fabsf(legacyNumeric[2] - 3.0f) < 1e-6f);

	CXString *edgeKinds = (CXString *)CXNetworkGetEdgeAttributeBuffer(net, "kind");
	assert(edgeKinds);
	assert(strcmp(edgeKinds[0], "forward") == 0);
	assert(strcmp(edgeKinds[1], "back") == 0);

	CXAttributeRef groupAttr = CXNetworkGetNodeAttribute(net, "Group");
	assert(groupAttr);
	assert(groupAttr->type == CXDataAttributeCategoryType);
	assert(groupAttr->categoricalDictionary);
	assert(CXStringDictionaryCount(groupAttr->categoricalDictionary) == 2);
	int32_t groupId = 0;
	assert(lookup_category_id(groupAttr->categoricalDictionary, "alpha", &groupId) && groupId == 0);
	assert(lookup_category_id(groupAttr->categoricalDictionary, "__NA__", &groupId) && groupId == -1);
	int32_t *groupCodes = (int32_t *)CXNetworkGetNodeAttributeBuffer(net, "Group");
	assert(groupCodes);
	assert(groupCodes[0] == 0);
	assert(groupCodes[1] == -1);
	assert(groupCodes[2] == 0);

	char upgradePath[] = "/tmp/cxnet-upgrade-XXXXXX";
	int upgradeFd = mkstemp(upgradePath);
	assert(upgradeFd >= 0);
	close(upgradeFd);
	assert(CXNetworkWriteXNet(net, upgradePath));
	unlink(upgradePath);

	release_all_string_attributes(net);
	CXFreeNetwork(net);
}

static void test_xnet_legacy_vertices_tokens_and_unescaped_strings(void) {
	const char *legacy =
		"#vertices 3 nonweighted\n"
		"\"Monkey\"\n"
		"\"Fish\"\n"
		"\"Cat\\Dog\"\n"
		"#edges nonweighted undirected\n"
		"0 1\n"
		"#v \"Category\" s\n"
		"\"Monkey\"\n"
		"\"Fish\"\n"
		"\"Cat\\Dog\"\n";

	char legacyPath[] = "/tmp/cxnet-legacy-tokens-XXXXXX";
	int fd = mkstemp(legacyPath);
	assert(fd >= 0);
	size_t legacyLen = strlen(legacy);
	assert(write(fd, legacy, legacyLen) == (ssize_t)legacyLen);
	close(fd);

	CXNetworkRef net = CXNetworkReadXNet(legacyPath);
	assert(net);
	unlink(legacyPath);

	assert(net->nodeCount == 3);
	assert(net->edgeCount == 1);
	assert(!CXNetworkIsDirected(net));

	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "Label");
	assert(labels);
	assert(strcmp(labels[0], "Monkey") == 0);
	assert(strcmp(labels[1], "Fish") == 0);
	assert(strcmp(labels[2], "Cat\\Dog") == 0);

	CXString *categories = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "Category");
	assert(categories);
	assert(strcmp(categories[0], "Monkey") == 0);
	assert(strcmp(categories[1], "Fish") == 0);
	assert(strcmp(categories[2], "Cat\\Dog") == 0);

	release_all_string_attributes(net);
	CXFreeNetwork(net);
}

static void test_xnet_string_escaping(void) {
	const char *content =
		"#XNET 1.0.0\n"
		"#vertices 2\n"
		"#edges undirected\n"
		"0 1\n"
		"#v \"Label\" s\n"
		"\"Line1\\nLine2\"\n"
		"\"#Hashtag\"\n";

	char path[] = "/tmp/cxnet-escape-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	size_t len = strlen(content);
	assert(write(fd, content, len) == (ssize_t)len);
	close(fd);

	CXNetworkRef net = CXNetworkReadXNet(path);
	assert(net);

	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "Label");
	assert(labels);
	assert(strcmp(labels[0], "Line1\nLine2") == 0);
	assert(strcmp(labels[1], "#Hashtag") == 0);

	char outPath[] = "/tmp/cxnet-escape-out-XXXXXX";
	int outFd = mkstemp(outPath);
	assert(outFd >= 0);
	close(outFd);
	assert(CXNetworkWriteXNet(net, outPath));

	FILE *outFile = fopen(outPath, "rb");
	assert(outFile);
	fseek(outFile, 0, SEEK_END);
	long outSize = ftell(outFile);
	assert(outSize > 0);
	rewind(outFile);
	char *buffer = malloc((size_t)outSize + 1);
	assert(buffer);
	size_t readBytes = fread(buffer, 1, (size_t)outSize, outFile);
	assert(readBytes == (size_t)outSize);
	buffer[outSize] = '\0';
	fclose(outFile);

	assert(strstr(buffer, "Line1\\nLine2") != NULL);
	assert(strstr(buffer, "\"#Hashtag\"") != NULL);

	free(buffer);
	unlink(path);
	unlink(outPath);
	release_all_string_attributes(net);
	CXFreeNetwork(net);
}

static void test_xnet_invalid_inputs(void) {
	struct {
		const char *name;
		const char *payload;
	} cases[] = {
		{
			"edge out of range",
			"#XNET 1.0.0\n#vertices 2\n#edges undirected\n0 2\n",
		},
		{
			"invalid escape sequence",
			"#XNET 1.0.0\n#vertices 1\n#edges undirected\n#v \"Label\" s\n\"Cat\\Dog\"\n",
		},
		{
			"vector arity mismatch",
			"#XNET 1.0.0\n#vertices 2\n#edges undirected\n0 1\n#v \"Vec\" f3\n1 2\n3 4 5\n",
		},
		{
			"attribute count mismatch",
			"#XNET 1.0.0\n#vertices 2\n#edges undirected\n0 1\n#v \"Value\" f\n1\n",
		},
		{
			"comment inside block",
			"#XNET 1.0.0\n#vertices 1\n#edges undirected\n#v \"Value\" f\n## nope\n0.5\n",
		},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char path[] = "/tmp/cxnet-invalid-XXXXXX";
		int fd = mkstemp(path);
		assert(fd >= 0);
		size_t len = strlen(cases[i].payload);
		assert(write(fd, cases[i].payload, len) == (ssize_t)len);
		close(fd);
		CXNetworkRef net = CXNetworkReadXNet(path);
		unlink(path);
		if (net) {
			release_all_string_attributes(net);
			CXFreeNetwork(net);
		}
		assert(net == NULL);
	}
}

static void test_xnet_compaction_mapping(void) {
	CXNetworkRef net = CXNewNetwork(CXFalse);
	assert(net);

	CXIndex nodes[5];
	assert(CXNetworkAddNodes(net, 5, nodes));

	CXEdge edges[3] = {
		{ .from = nodes[0], .to = nodes[2] },
		{ .from = nodes[2], .to = nodes[4] },
		{ .from = nodes[4], .to = nodes[0] },
	};
	assert(CXNetworkAddEdges(net, edges, 3, NULL));

	assert(CXNetworkDefineNodeAttribute(net, "value", CXIntegerAttributeType, 1));
	int32_t *values = (int32_t *)CXNetworkGetNodeAttributeBuffer(net, "value");
	for (int i = 0; i < 5; i++) {
		values[nodes[i]] = i * 10;
	}

	CXIndex toRemove[2] = { nodes[1], nodes[3] };
	assert(CXNetworkRemoveNodes(net, toRemove, 2));

	char path[] = "/tmp/cxnet-compact-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);
	assert(CXNetworkWriteXNet(net, path));
	release_all_string_attributes(net);
	CXFreeNetwork(net);

	CXNetworkRef compact = CXNetworkReadXNet(path);
	assert(compact);
	unlink(path);

	assert(compact->nodeCount == 3);
	assert(compact->edgeCount == 3);
	assert(!CXNetworkIsDirected(compact));

	int32_t *loadedValues = (int32_t *)CXNetworkGetNodeAttributeBuffer(compact, "value");
	assert(loadedValues);
	assert(loadedValues[0] == 0);
	assert(loadedValues[1] == 20);
	assert(loadedValues[2] == 40);

	CXString *originalIds = (CXString *)CXNetworkGetNodeAttributeBuffer(compact, "_original_ids_");
	assert(originalIds);
	assert(strcmp(originalIds[0], "0") == 0);
	assert(strcmp(originalIds[1], "2") == 0);
	assert(strcmp(originalIds[2], "4") == 0);

	CXSize observedEdges = 0;
	for (CXSize i = 0; i < compact->edgeCapacity; i++) {
		if (compact->edgeActive && compact->edgeActive[i]) {
			CXEdge e = compact->edges[i];
			if (observedEdges == 0) {
				assert(e.from == 0 && e.to == 1);
			} else if (observedEdges == 1) {
				assert(e.from == 1 && e.to == 2);
			} else if (observedEdges == 2) {
				assert(e.from == 2 && e.to == 0);
			}
			observedEdges++;
		}
	}
	assert(observedEdges == 3);

	release_all_string_attributes(compact);
	CXFreeNetwork(compact);
}

static void test_gml_round_trip_and_warnings(void) {
	CXNetworkRef net = CXNewNetwork(CXTrue);
	assert(net);

	CXIndex nodes[3];
	assert(CXNetworkAddNodes(net, 3, nodes));
	CXEdge edges[2] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[1], .to = nodes[2] },
	};
	CXIndex edgeIds[2];
	assert(CXNetworkAddEdges(net, edges, 2, edgeIds));

	assert(CXNetworkDefineNodeAttribute(net, "score", CXFloatAttributeType, 1));
	float *scores = (float *)CXNetworkGetNodeAttributeBuffer(net, "score");
	assert(scores);
	scores[nodes[0]] = 1.25f;
	scores[nodes[1]] = 2.5f;
	scores[nodes[2]] = 3.75f;

	assert(CXNetworkDefineNodeAttribute(net, "safe_label", CXStringAttributeType, 1));
	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "safe_label");
	assert(labels);
	labels[nodes[0]] = CXNewStringFromString("Alpha");
	labels[nodes[1]] = CXNewStringFromString("Beta Value");
	labels[nodes[2]] = CXNewStringFromString("Gamma");

	assert(CXNetworkDefineNodeAttribute(net, "label with spaces", CXStringAttributeType, 1));
	CXString *renamed = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "label with spaces");
	assert(renamed);
	renamed[nodes[0]] = CXNewStringFromString("A");
	renamed[nodes[1]] = CXNewStringFromString("B");
	renamed[nodes[2]] = CXNewStringFromString("C");

	assert(CXNetworkDefineEdgeAttribute(net, "relation", CXStringAttributeType, 1));
	CXString *relations = (CXString *)CXNetworkGetEdgeAttributeBuffer(net, "relation");
	assert(relations);
	relations[edgeIds[0]] = CXNewStringFromString("forward");
	relations[edgeIds[1]] = CXNewStringFromString("return");

	assert(CXNetworkDefineNetworkAttribute(net, "title", CXStringAttributeType, 1));
	CXString *titles = (CXString *)CXNetworkGetNetworkAttributeBuffer(net, "title");
	assert(titles);
	titles[0] = CXNewStringFromString("GML Round Trip");

	char path[] = "/tmp/cxnet-gml-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);
	assert(CXNetworkWriteGML(net, path));
	const char *warning = CXNetworkSerializationLastWarningMessage();
	assert(warning && strstr(warning, "label with spaces") != NULL);
	release_all_string_attributes(net);
	CXFreeNetwork(net);

	CXNetworkRef loaded = CXNetworkReadGML(path);
	assert(loaded);
	unlink(path);

	assert(loaded->nodeCount == 3);
	assert(loaded->edgeCount == 2);
	assert(CXNetworkIsDirected(loaded));

	float *loadedScores = (float *)CXNetworkGetNodeAttributeBuffer(loaded, "score");
	assert(loadedScores);
	assert(fabsf(loadedScores[2] - 3.75f) < 1e-6f);

	CXString *loadedLabels = (CXString *)CXNetworkGetNodeAttributeBuffer(loaded, "safe_label");
	assert(loadedLabels && strcmp(loadedLabels[1], "Beta Value") == 0);

	CXString *loadedOriginal = (CXString *)CXNetworkGetNodeAttributeBuffer(loaded, "_original_ids_");
	assert(loadedOriginal);
	assert(strcmp(loadedOriginal[0], "0") == 0);
	assert(strcmp(loadedOriginal[1], "1") == 0);
	assert(strcmp(loadedOriginal[2], "2") == 0);

	CXString *loadedTitle = (CXString *)CXNetworkGetNetworkAttributeBuffer(loaded, "title");
	assert(loadedTitle && strcmp(loadedTitle[0], "GML Round Trip") == 0);

	release_all_string_attributes(loaded);
	CXFreeNetwork(loaded);
}

static void test_gml_loose_loader(void) {
	const char *payload =
		"graph [\n"
		"  directed 1\n"
		"  \"graph note\" \"Loose GML\"\n"
		"  node [ id 10 \"node label\" \"Alpha\" rating 3.5 ]\n"
		"  node [ id 20 \"node label\" \"Beta\" ]\n"
		"  edge [ source 10 target 20 relation friend ]\n"
		"]\n";

	char path[] = "/tmp/cxnet-gml-loose-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	size_t len = strlen(payload);
	assert(write(fd, payload, len) == (ssize_t)len);
	close(fd);

	CXNetworkRef net = CXNetworkReadGML(path);
	assert(net);
	unlink(path);

	assert(CXNetworkIsDirected(net));
	assert(net->nodeCount == 2);
	assert(net->edgeCount == 1);

	CXString *graphNote = (CXString *)CXNetworkGetNetworkAttributeBuffer(net, "graph note");
	assert(graphNote && strcmp(graphNote[0], "Loose GML") == 0);

	CXString *nodeLabels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "node label");
	assert(nodeLabels);
	assert(strcmp(nodeLabels[0], "Alpha") == 0);
	assert(strcmp(nodeLabels[1], "Beta") == 0);

	double *rating = (double *)CXNetworkGetNodeAttributeBuffer(net, "rating");
	assert(rating && fabs(rating[0] - 3.5) < 1e-9);

	CXString *relation = (CXString *)CXNetworkGetEdgeAttributeBuffer(net, "relation");
	assert(relation && strcmp(relation[0], "friend") == 0);

	CXString *originalIds = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "_original_ids_");
	assert(originalIds);
	assert(strcmp(originalIds[0], "10") == 0);
	assert(strcmp(originalIds[1], "20") == 0);

	release_all_string_attributes(net);
	CXFreeNetwork(net);
}

static void test_gt_round_trip(void) {
	CXNetworkRef net = CXNewNetwork(CXTrue);
	assert(net);

	CXIndex nodes[3];
	assert(CXNetworkAddNodes(net, 3, nodes));
	CXEdge edges[3] = {
		{ .from = nodes[0], .to = nodes[1] },
		{ .from = nodes[1], .to = nodes[2] },
		{ .from = nodes[0], .to = nodes[0] },
	};
	CXIndex edgeIds[3];
	assert(CXNetworkAddEdges(net, edges, 3, edgeIds));

	assert(CXNetworkDefineNetworkAttribute(net, "title", CXStringAttributeType, 1));
	CXString *titles = (CXString *)CXNetworkGetNetworkAttributeBuffer(net, "title");
	assert(titles);
	titles[0] = CXNewStringFromString("GT Round Trip");

	assert(CXNetworkDefineNodeAttribute(net, "score", CXDoubleAttributeType, 1));
	double *scores = (double *)CXNetworkGetNodeAttributeBuffer(net, "score");
	assert(scores);
	scores[nodes[0]] = 1.25;
	scores[nodes[1]] = 2.5;
	scores[nodes[2]] = 3.75;

	assert(CXNetworkDefineNodeAttribute(net, "coords", CXDoubleAttributeType, 2));
	double *coords = (double *)CXNetworkGetNodeAttributeBuffer(net, "coords");
	assert(coords);
	coords[nodes[0] * 2] = 1.0;
	coords[nodes[0] * 2 + 1] = 2.0;
	coords[nodes[1] * 2] = 3.0;
	coords[nodes[1] * 2 + 1] = 4.0;
	coords[nodes[2] * 2] = 5.0;
	coords[nodes[2] * 2 + 1] = 6.0;

	assert(CXNetworkDefineNodeAttribute(net, "label", CXStringAttributeType, 1));
	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "label");
	assert(labels);
	labels[nodes[0]] = CXNewStringFromString("Alpha");
	labels[nodes[1]] = CXNewStringFromString("Beta");
	labels[nodes[2]] = CXNewStringFromString("Gamma");

	assert(CXNetworkDefineEdgeAttribute(net, "weight", CXDoubleAttributeType, 1));
	double *weights = (double *)CXNetworkGetEdgeAttributeBuffer(net, "weight");
	assert(weights);
	weights[edgeIds[0]] = 0.5;
	weights[edgeIds[1]] = 1.5;
	weights[edgeIds[2]] = 2.5;

	char path[] = "/tmp/cxnet-gt-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);
	assert(CXNetworkWriteGT(net, path));
	release_all_string_attributes(net);
	CXFreeNetwork(net);

	CXNetworkRef loaded = CXNetworkReadGT(path);
	assert(loaded);
	unlink(path);

	assert(CXNetworkIsDirected(loaded));
	assert(loaded->nodeCount == 3);
	assert(loaded->edgeCount == 3);

	double *loadedScores = (double *)CXNetworkGetNodeAttributeBuffer(loaded, "score");
	assert(loadedScores && fabs(loadedScores[2] - 3.75) < 1e-9);
	double *loadedCoords = (double *)CXNetworkGetNodeAttributeBuffer(loaded, "coords");
	assert(loadedCoords && fabs(loadedCoords[4] - 5.0) < 1e-9 && fabs(loadedCoords[5] - 6.0) < 1e-9);
	CXString *loadedLabels = (CXString *)CXNetworkGetNodeAttributeBuffer(loaded, "label");
	assert(loadedLabels && strcmp(loadedLabels[1], "Beta") == 0);
	CXString *loadedTitles = (CXString *)CXNetworkGetNetworkAttributeBuffer(loaded, "title");
	assert(loadedTitles && strcmp(loadedTitles[0], "GT Round Trip") == 0);

	double *loadedWeights = (double *)CXNetworkGetEdgeAttributeBuffer(loaded, "weight");
	assert(loadedWeights);
	assert(fabs(loadedWeights[0] - 0.5) < 1e-9);
	assert(fabs(loadedWeights[1] - 2.5) < 1e-9);
	assert(fabs(loadedWeights[2] - 1.5) < 1e-9);

	release_all_string_attributes(loaded);
	CXFreeNetwork(loaded);
}

static void test_gt_zst_read(void) {
	static const unsigned char compressedGT[] = {
		0x28, 0xb5, 0x2f, 0xfd, 0x00, 0x68, 0x7d, 0x08,
		0x00, 0xf2, 0x8d, 0x30, 0x2a, 0x90, 0x3b, 0x07,
		0x58, 0xb5, 0xa8, 0xbb, 0xdd, 0x2f, 0x67, 0xf5,
		0xdf, 0x02, 0x6d, 0x3c, 0x91, 0x30, 0xf9, 0xf3,
		0x43, 0x52, 0xa2, 0x24, 0x09, 0x76, 0xca, 0x3f,
		0xc2, 0xe0, 0x12, 0x5a, 0xbd, 0x61, 0x39, 0x9f,
		0x6b, 0xe7, 0xf0, 0x04, 0x6a, 0x24, 0x49, 0xda,
		0xe8, 0x43, 0xfd, 0x46, 0xab, 0x63, 0xaa, 0x09,
		0x1e, 0x0c, 0x05, 0x28, 0x80, 0x5a, 0x48, 0x9a,
		0x06, 0x79, 0xe2, 0x7b, 0x65, 0xf5, 0x20, 0x01,
		0x52, 0x9a, 0xc8, 0xf1, 0x95, 0xf9, 0x45, 0x8e,
		0x8b, 0x92, 0xf8, 0x1e, 0xa1, 0xe2, 0xd6, 0x59,
		0x48, 0x65, 0x7d, 0x12, 0xed, 0xbb, 0x9c, 0xcf,
		0xeb, 0xf3, 0xe6, 0xfa, 0x41, 0xe5, 0xf6, 0xeb,
		0x07, 0xa4, 0xe9, 0x01, 0xde, 0x22, 0xe3, 0x56,
		0xee, 0x2e, 0xee, 0xba, 0x17, 0x6d, 0x4d, 0x7f,
		0xe9, 0xb8, 0x4e, 0x3a, 0xbd, 0x1b, 0x6d, 0x65,
		0x97, 0xf5, 0x1b, 0x9d, 0xe5, 0x8f, 0x9f, 0x3b,
		0xd1, 0x56, 0x7f, 0xe5, 0xb8, 0x2b, 0x11, 0xed,
		0x78, 0x60, 0x8c, 0xb6, 0xdc, 0x9a, 0xb1, 0x5f,
		0x5a, 0x33, 0xe9, 0xe9, 0x1f, 0xb1, 0x5b, 0x9c,
		0x76, 0xc2, 0x27, 0x36, 0x1d, 0xe3, 0x95, 0x8c,
		0x29, 0x14, 0x59, 0xd6, 0xfd, 0x6f, 0x88, 0x76,
		0x5b, 0x29, 0xad, 0xc1, 0xd1, 0x52, 0x27, 0x6d,
		0xef, 0xfd, 0x6b, 0xeb, 0x22, 0xa6, 0x52, 0x47,
		0x7a, 0x4f, 0xdb, 0xc3, 0x01, 0x05, 0x22, 0x28,
		0x90, 0x46, 0xec, 0x6e, 0x03, 0x10, 0x18, 0xe3,
		0x28, 0x0d, 0x0a, 0x85, 0x0e, 0x8b, 0x8c, 0x13,
		0x66, 0xb9, 0x91, 0x10, 0x87, 0x18, 0xa1, 0x2d,
		0x17, 0x14, 0x14, 0x63, 0x1c, 0x4b, 0x34, 0x9e,
		0x60, 0x15, 0xe9, 0xb0, 0xf0, 0x12, 0x00, 0x25,
		0xb6, 0x94, 0xd0, 0xcf, 0xb5, 0xc6, 0x50, 0x30,
		0xcd, 0x96, 0xb1, 0x9c, 0xd0, 0x71, 0x58, 0x4d,
		0x6c, 0xa6, 0xc5, 0xb0, 0x7d, 0xae, 0x12, 0x09,
		0x49, 0x0b, 0x00, 0x62, 0xd8, 0xe3, 0x17, 0x09
	};
	char path[] = "/tmp/cxnet-gt-zst-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	FILE *file = fdopen(fd, "wb");
	assert(file);
	assert(fwrite(compressedGT, 1, sizeof(compressedGT), file) == sizeof(compressedGT));
	fclose(file);

	CXNetworkRef loaded = CXNetworkReadGT(path);
	unlink(path);
	assert(loaded);
	assert(CXNetworkIsDirected(loaded));
	assert(loaded->nodeCount == 3);
	assert(loaded->edgeCount == 3);
	CXEdge edge = loaded->edges[0];
	assert(edge.from == 0);
	assert(edge.to == 1);

	CXString *title = (CXString *)CXNetworkGetNetworkAttributeBuffer(loaded, "title");
	assert(title && strcmp(title[0], "gt-zst-demo") == 0);

	CXString *labels = (CXString *)CXNetworkGetNodeAttributeBuffer(loaded, "label");
	assert(labels && strcmp(labels[1], "Beta") == 0);

	double *scores = (double *)CXNetworkGetNodeAttributeBuffer(loaded, "score");
	assert(scores && fabs(scores[2] - 3.75) < 1e-9);

	double *coords = (double *)CXNetworkGetNodeAttributeBuffer(loaded, "coords");
	assert(coords && fabs(coords[4] - 5.0) < 1e-9 && fabs(coords[5] - 6.0) < 1e-9);

	double *weights = (double *)CXNetworkGetEdgeAttributeBuffer(loaded, "weight");
	assert(weights && fabs(weights[0] - 0.5) < 1e-9);
	assert(fabs(weights[1] - 1.5) < 1e-9);
	assert(fabs(weights[2] - 2.5) < 1e-9);

	release_all_string_attributes(loaded);
	CXFreeNetwork(loaded);
}

static void test_node_link_json_export(void) {
	CXNetworkRef net = CXNewNetwork(CXFalse);
	assert(net);

	CXIndex nodes[2];
	assert(CXNetworkAddNodes(net, 2, nodes));
	CXEdge edge = { .from = nodes[0], .to = nodes[1] };
	CXIndex edgeId = 0;
	assert(CXNetworkAddEdges(net, &edge, 1, &edgeId));

	assert(CXNetworkDefineNodeAttribute(net, "coords", CXFloatAttributeType, 2));
	float *coords = (float *)CXNetworkGetNodeAttributeBuffer(net, "coords");
	assert(coords);
	coords[nodes[0] * 2 + 0] = 1.0f;
	coords[nodes[0] * 2 + 1] = 2.0f;
	coords[nodes[1] * 2 + 0] = 3.0f;
	coords[nodes[1] * 2 + 1] = 4.0f;

	assert(CXNetworkDefineNodeAttribute(net, "id", CXStringAttributeType, 1));
	CXString *ids = (CXString *)CXNetworkGetNodeAttributeBuffer(net, "id");
	assert(ids);
	ids[nodes[0]] = CXNewStringFromString("left");
	ids[nodes[1]] = CXNewStringFromString("right");

	assert(CXNetworkDefineEdgeAttribute(net, "weight", CXFloatAttributeType, 1));
	float *weights = (float *)CXNetworkGetEdgeAttributeBuffer(net, "weight");
	assert(weights);
	weights[edgeId] = 2.5f;

	char path[] = "/tmp/cxnet-node-link-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);
	assert(CXNetworkWriteNodeLinkJSON(net, path));
	const char *warning = CXNetworkSerializationLastWarningMessage();
	assert(warning && strstr(warning, "\"attributes\"") != NULL);

	FILE *file = fopen(path, "rb");
	assert(file);
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	assert(size >= 0);
	fseek(file, 0, SEEK_SET);
	char *text = calloc((size_t)size + 1u, sizeof(char));
	assert(text);
	assert(fread(text, 1, (size_t)size, file) == (size_t)size);
	fclose(file);
	unlink(path);

	assert(strstr(text, "\"nodes\"") != NULL);
	assert(strstr(text, "\"links\"") != NULL);
	assert(strstr(text, "\"coords\": [1") != NULL);
	assert(strstr(text, "\"attributes\": {\"id\": \"left\"}") != NULL);
	assert(strstr(text, "\"weight\": 2.5") != NULL);

	free(text);
	release_all_string_attributes(net);
	CXFreeNetwork(net);
}

static void test_serialization_fuzz(void) {
	srand(42);
	CXSize sizes[] = {0, 1, 4, 12};
	for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
		for (int run = 0; run < 5; run++) {
			CXNetworkRef net = CXNewNetwork(run % 2 == 0 ? CXTrue : CXFalse);
			build_random_network(net, sizes[s]);
			verify_round_trip(net);
			CXFreeNetwork(net);
		}
	}
	printf("Serialization fuzz tests passed.\n");
}

static void test_network_generators(void) {
	CXNetworkRef ws = CXNetworkGenerateWattsStrogatz(10, 2, 0.0, CXFalse, 7);
	assert(ws);
	assert(CXNetworkNodeCount(ws) == 10);
	assert(CXNetworkEdgeCount(ws) == 20);
	CXFreeNetwork(ws);

	CXNetworkRef lattice = CXNetworkGenerateLattice2D(3, 4, 1, CXFalse, CXFalse);
	assert(lattice);
	assert(CXNetworkNodeCount(lattice) == 12);
	assert(CXNetworkEdgeCount(lattice) == 17);
	CXFreeNetwork(lattice);

	CXNetworkRef ba = CXNetworkGenerateBarabasiAlbert(10, 2, 3, CXFalse, 11);
	assert(ba);
	assert(CXNetworkNodeCount(ba) == 10);
	assert(CXNetworkEdgeCount(ba) == 17);
	CXFreeNetwork(ba);

	CXSize blocks[] = {2, 3};
	double probabilities[] = {1.0, 1.0, 1.0, 1.0};
	CXNetworkRef sbm = CXNetworkGenerateStochasticBlockModel(2, blocks, probabilities, CXFalse, 13);
	assert(sbm);
	assert(CXNetworkNodeCount(sbm) == 5);
	assert(CXNetworkEdgeCount(sbm) == 10);
	CXFreeNetwork(sbm);

	CXSize degrees[] = {2, 2, 2, 2};
	CXNetworkRef config = CXNetworkGenerateConfigurationModel(4, degrees, CXFalse, CXTrue, CXTrue, 17);
	assert(config);
	assert(CXNetworkNodeCount(config) == 4);
	assert(CXNetworkEdgeCount(config) == 4);
	CXFreeNetwork(config);

	CXNetworkRef geometric = CXNetworkGenerateRandomGeometric(5, 2.0, CXFalse, 19);
	assert(geometric);
	assert(CXNetworkNodeCount(geometric) == 5);
	assert(CXNetworkEdgeCount(geometric) == 10);
	assert(CXNetworkGetNodeAttribute(geometric, "_helios_generator_position"));
	CXFreeNetwork(geometric);
}

int main(void) {
	test_basic_network();
	test_neighbor_collection();
	test_attributes();
	test_xnet_round_trip();
	test_categorical_serialization();
	test_categorical_helpers();
	test_multicategory_serialization();
	test_xnet_legacy_upgrade();
	test_xnet_legacy_multicategory();
	test_xnet_legacy_vertices_tokens_and_unescaped_strings();
	test_xnet_string_escaping();
	test_xnet_invalid_inputs();
	test_xnet_compaction_mapping();
	test_gml_round_trip_and_warnings();
	test_gml_loose_loader();
	test_gt_round_trip();
	test_gt_zst_read();
	test_node_link_json_export();
	test_serialization_fuzz();
	test_network_generators();
	printf("All native network tests passed.\n");
	return 0;
}
