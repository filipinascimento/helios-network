#include <assert.h>
#include <stdio.h>

#include "CXNetwork.h"
#include "CXNeighborStorage.h"

static void test_basic_network() {
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

	CXNetworkRemoveEdges(net, &edgeIds[0], 1);
	assert(CXNetworkEdgeCount(net) == 1);
	assert(!CXNetworkIsEdgeActive(net, edgeIds[0]));

	CXNetworkRemoveNodes(net, &nodes[1], 1);
	assert(!CXNetworkIsNodeActive(net, nodes[1]));

	CXFreeNetwork(net);
}

static void test_attributes() {
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

int main(void) {
	test_basic_network();
	test_attributes();
	printf("All native network tests passed.\n");
	return 0;
}
