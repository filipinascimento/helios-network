#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CXNetwork.h"
#include "CXNeighborStorage.h"

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
		const uint64_t *orig = (const uint64_t *)nodeAttr->data;
		for (CXSize i = 0; i < nodeActiveCount; i++) {
			assert(orig[i] == (uint64_t)activeNodes[i]);
		}
	}
	CXAttributeRef edgeAttr = CXNetworkGetEdgeAttribute(net, "__orig_edge");
	if (edgeActiveCount > 0) {
		assert(edgeAttr && edgeAttr->data);
		const uint64_t *edgeOrig = (const uint64_t *)edgeAttr->data;
		const uint64_t *nodeOrig = nodeAttr && nodeAttr->data ? (const uint64_t *)nodeAttr->data : NULL;
		for (CXSize i = 0; i < edgeActiveCount; i++) {
			assert(edgeOrig[i] == (uint64_t)activeEdges[i].index);
			CXEdge expected = activeEdges[i].edge;
			CXEdge actual = net->edges[i];
			if (nodeOrig) {
				assert(nodeOrig[actual.from] == (uint64_t)expected.from);
				assert(nodeOrig[actual.to] == (uint64_t)expected.to);
			} else {
				assert(actual.from == expected.from);
				assert(actual.to == expected.to);
			}
		}
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

int main(void) {
	test_basic_network();
	test_attributes();
	test_serialization_fuzz();
	printf("All native network tests passed.\n");
	return 0;
}
