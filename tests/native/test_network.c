#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CXNetwork.h"
#include "CXNeighborStorage.h"
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
	int64_t *coords = (int64_t *)CXNetworkGetNodeAttributeBuffer(net, "coord");
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

	int64_t *loadedCoords = (int64_t *)CXNetworkGetNodeAttributeBuffer(loaded, "coord");
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

	char upgradePath[] = "/tmp/cxnet-upgrade-XXXXXX";
	int upgradeFd = mkstemp(upgradePath);
	assert(upgradeFd >= 0);
	close(upgradeFd);
	assert(CXNetworkWriteXNet(net, upgradePath));
	unlink(upgradePath);

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
	int64_t *values = (int64_t *)CXNetworkGetNodeAttributeBuffer(net, "value");
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

	int64_t *loadedValues = (int64_t *)CXNetworkGetNodeAttributeBuffer(compact, "value");
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
	test_xnet_round_trip();
	test_xnet_legacy_upgrade();
	test_xnet_string_escaping();
	test_xnet_invalid_inputs();
	test_xnet_compaction_mapping();
	test_serialization_fuzz();
	printf("All native network tests passed.\n");
	return 0;
}
