#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CXNetwork.h"

static void assert_near_impl(double actual, double expected, double tolerance, const char *file, int line) {
	if (fabs(actual - expected) <= tolerance) {
		return;
	}
	fprintf(
		stderr,
		"assert_near failed at %s:%d (actual=%.12f expected=%.12f tolerance=%.12f)\n",
		file,
		line,
		actual,
		expected,
		tolerance
	);
	abort();
}

#define assert_near(actual, expected, tolerance) \
	assert_near_impl((actual), (expected), (tolerance), __FILE__, __LINE__)

static CXNetworkRef build_network(
	CXBool directed,
	CXSize nodeCount,
	const CXEdge *edges,
	CXSize edgeCount,
	const float *weights,
	const CXString weightAttribute,
	CXIndex *outNodeIndices,
	CXIndex *outEdgeIndices
) {
	CXNetworkRef network = CXNewNetworkWithCapacity(directed, nodeCount + 4, edgeCount + 8);
	assert(network);
	CXIndex *nodeIndices = (CXIndex *)malloc(nodeCount * sizeof(CXIndex));
	assert(nodeIndices);
	assert(CXNetworkAddNodes(network, nodeCount, nodeIndices) == CXTrue);

	CXEdge *mappedEdges = (CXEdge *)malloc(edgeCount * sizeof(CXEdge));
	assert(mappedEdges);
	for (CXSize i = 0; i < edgeCount; i++) {
		assert(edges[i].from < nodeCount);
		assert(edges[i].to < nodeCount);
		mappedEdges[i].from = nodeIndices[edges[i].from];
		mappedEdges[i].to = nodeIndices[edges[i].to];
	}

	assert(CXNetworkAddEdges(network, mappedEdges, edgeCount, outEdgeIndices) == CXTrue);
	if (outNodeIndices) {
		memcpy(outNodeIndices, nodeIndices, nodeCount * sizeof(CXIndex));
	}
	free(mappedEdges);
	free(nodeIndices);

	if (weights && weightAttribute) {
		assert(CXNetworkDefineEdgeAttribute(network, weightAttribute, CXFloatAttributeType, 1) == CXTrue);
		float *buffer = (float *)CXNetworkGetEdgeAttributeBuffer(network, weightAttribute);
		assert(buffer);
		for (CXSize i = 0; i < edgeCount; i++) {
			CXIndex edgeIndex = outEdgeIndices ? outEdgeIndices[i] : (CXIndex)i;
			buffer[edgeIndex] = weights[i];
		}
	}

	return network;
}

static void test_degree_and_strength(void) {
	const CXEdge edges[] = {
		{ .from = 0, .to = 1 },
		{ .from = 0, .to = 2 },
		{ .from = 0, .to = 3 },
		{ .from = 0, .to = 4 }
	};
	const float weights[] = {2.0f, 4.0f, 6.0f, 8.0f};
	CXIndex nodeIndices[5] = {0};
	CXIndex edgeIndices[4] = {0};
	CXNetworkRef network = build_network(
		CXFalse,
		5,
		edges,
		4,
		weights,
		(CXString)"w",
		nodeIndices,
		edgeIndices
	);

	CXSize capacity = CXNetworkNodeCapacity(network);
	float *degree = (float *)calloc(capacity, sizeof(float));
	float *strengthSum = (float *)calloc(capacity, sizeof(float));
	float *strengthAverage = (float *)calloc(capacity, sizeof(float));
	assert(degree);
	assert(strengthSum);
	assert(strengthAverage);
	assert(CXNetworkMeasureDegree(network, CXNeighborDirectionOut, degree) == CXTrue);
	assert(CXNetworkMeasureStrength(network, (CXString)"w", CXNeighborDirectionOut, CXStrengthMeasureSum, strengthSum) == CXTrue);
	assert(CXNetworkMeasureStrength(network, (CXString)"w", CXNeighborDirectionOut, CXStrengthMeasureAverage, strengthAverage) == CXTrue);

	assert_near(degree[nodeIndices[0]], 4.0, 1e-6);
	assert_near(degree[nodeIndices[1]], 1.0, 1e-6);
	assert_near(degree[nodeIndices[4]], 1.0, 1e-6);
	assert_near(strengthSum[nodeIndices[0]], 20.0, 1e-6);
	assert_near(strengthSum[nodeIndices[1]], 2.0, 1e-6);
	assert_near(strengthSum[nodeIndices[2]], 4.0, 1e-6);
	assert_near(strengthAverage[nodeIndices[0]], 5.0, 1e-6);
	assert_near(strengthAverage[nodeIndices[3]], 6.0, 1e-6);

	free(degree);
	free(strengthSum);
	free(strengthAverage);
	CXFreeNetwork(network);
}

static void test_clustering_variants(void) {
	const CXEdge triangleEdges[] = {
		{ .from = 0, .to = 1 },
		{ .from = 1, .to = 2 },
		{ .from = 2, .to = 0 }
	};
	const float triangleWeights[] = {1.0f, 1.0f, 1.0f};
	CXIndex triangleNodeIndices[3] = {0};
	CXIndex triangleEdgeIndices[3] = {0};
	CXNetworkRef triangle = build_network(
		CXFalse,
		3,
		triangleEdges,
		3,
		triangleWeights,
		(CXString)"w",
		triangleNodeIndices,
		triangleEdgeIndices
	);

	CXSize triangleCapacity = CXNetworkNodeCapacity(triangle);
	float *unweighted = (float *)calloc(triangleCapacity, sizeof(float));
	float *onnela = (float *)calloc(triangleCapacity, sizeof(float));
	float *newman = (float *)calloc(triangleCapacity, sizeof(float));
	assert(unweighted);
	assert(onnela);
	assert(newman);
	assert(CXNetworkMeasureLocalClusteringCoefficient(
		triangle,
		NULL,
		CXNeighborDirectionOut,
		CXClusteringCoefficientUnweighted,
		unweighted
	) == CXTrue);
	assert(CXNetworkMeasureLocalClusteringCoefficient(
		triangle,
		(CXString)"w",
		CXNeighborDirectionOut,
		CXClusteringCoefficientOnnela,
		onnela
	) == CXTrue);
	assert(CXNetworkMeasureLocalClusteringCoefficient(
		triangle,
		(CXString)"w",
		CXNeighborDirectionOut,
		CXClusteringCoefficientNewman,
		newman
	) == CXTrue);

	for (CXSize i = 0; i < 3; i++) {
		CXIndex node = triangleNodeIndices[i];
		assert_near(unweighted[node], 1.0, 1e-6);
		assert_near(onnela[node], 1.0, 1e-6);
		assert_near(newman[node], 1.0, 1e-6);
	}
	free(unweighted);
	free(onnela);
	free(newman);
	CXFreeNetwork(triangle);

	const CXEdge pathEdges[] = {
		{ .from = 0, .to = 1 },
		{ .from = 1, .to = 2 }
	};
	CXIndex pathNodeIndices[3] = {0};
	CXNetworkRef path = build_network(CXFalse, 3, pathEdges, 2, NULL, NULL, pathNodeIndices, NULL);
	CXSize pathCapacity = CXNetworkNodeCapacity(path);
	float *pathClustering = (float *)calloc(pathCapacity, sizeof(float));
	assert(pathClustering);
	assert(CXNetworkMeasureLocalClusteringCoefficient(
		path,
		NULL,
		CXNeighborDirectionOut,
		CXClusteringCoefficientUnweighted,
		pathClustering
	) == CXTrue);
	assert_near(pathClustering[pathNodeIndices[0]], 0.0, 1e-6);
	assert_near(pathClustering[pathNodeIndices[1]], 0.0, 1e-6);
	assert_near(pathClustering[pathNodeIndices[2]], 0.0, 1e-6);
	free(pathClustering);
	CXFreeNetwork(path);
}

static void test_eigenvector_centrality_modes(void) {
	const CXEdge edges[] = {
		{ .from = 0, .to = 1 },
		{ .from = 0, .to = 2 },
		{ .from = 0, .to = 3 },
		{ .from = 0, .to = 4 }
	};
	CXIndex nodeIndices[5] = {0};
	CXNetworkRef network = build_network(CXFalse, 5, edges, 4, NULL, NULL, nodeIndices, NULL);

	CXSize capacity = CXNetworkNodeCapacity(network);
	float *single = (float *)calloc(capacity, sizeof(float));
	float *parallel = (float *)calloc(capacity, sizeof(float));
	assert(single);
	assert(parallel);
	double eigenvalueSingle = 0.0;
	double eigenvalueParallel = 0.0;
	double deltaSingle = 0.0;
	double deltaParallel = 0.0;
	CXSize iterationsSingle = 0;
	CXSize iterationsParallel = 0;
	CXBool convergedSingle = CXFalse;
	CXBool convergedParallel = CXFalse;

	assert(CXNetworkMeasureEigenvectorCentrality(
		network,
		NULL,
		CXNeighborDirectionOut,
		CXMeasurementExecutionSingleThread,
		256,
		1e-8,
		NULL,
		single,
		&eigenvalueSingle,
		&deltaSingle,
		&iterationsSingle,
		&convergedSingle
	) == CXTrue);

	assert(CXNetworkMeasureEigenvectorCentrality(
		network,
		NULL,
		CXNeighborDirectionOut,
		CXMeasurementExecutionParallel,
		256,
		1e-8,
		NULL,
		parallel,
		&eigenvalueParallel,
		&deltaParallel,
		&iterationsParallel,
		&convergedParallel
	) == CXTrue);

	assert(iterationsSingle > 0);
	assert(iterationsParallel > 0);
	assert(convergedSingle == CXTrue);
	assert(convergedParallel == CXTrue);
	assert(single[nodeIndices[0]] > single[nodeIndices[1]]);
	assert(parallel[nodeIndices[0]] > parallel[nodeIndices[1]]);
	assert_near(single[nodeIndices[0]] / single[nodeIndices[1]], 2.0, 0.05);
	assert_near(parallel[nodeIndices[0]] / parallel[nodeIndices[1]], 2.0, 0.05);
	for (CXSize i = 0; i < 5; i++) {
		CXIndex node = nodeIndices[i];
		assert_near(single[node], parallel[node], 1e-5);
	}

	free(single);
	free(parallel);
	CXFreeNetwork(network);
}

static void test_betweenness_centrality_modes_and_chunks(void) {
	const CXEdge pathEdges[] = {
		{ .from = 0, .to = 1 },
		{ .from = 1, .to = 2 },
		{ .from = 2, .to = 3 }
	};
	CXIndex pathNodeIndices[4] = {0};
	CXNetworkRef path = build_network(CXFalse, 4, pathEdges, 3, NULL, NULL, pathNodeIndices, NULL);

	CXSize pathCapacity = CXNetworkNodeCapacity(path);
	float *singlePath = (float *)calloc(pathCapacity, sizeof(float));
	float *parallelPath = (float *)calloc(pathCapacity, sizeof(float));
	assert(singlePath);
	assert(parallelPath);
	CXSize processedSingle = CXNetworkMeasureBetweennessCentrality(
		path,
		NULL,
		CXMeasurementExecutionSingleThread,
		NULL,
		0,
		CXTrue,
		CXFalse,
		singlePath
	);
	CXSize processedParallel = CXNetworkMeasureBetweennessCentrality(
		path,
		NULL,
		CXMeasurementExecutionParallel,
		NULL,
		0,
		CXTrue,
		CXFalse,
		parallelPath
	);
	assert(processedSingle == 4);
	assert(processedParallel == 4);
	assert_near(singlePath[pathNodeIndices[0]], 0.0, 1e-6);
	assert_near(singlePath[pathNodeIndices[1]], 2.0 / 3.0, 0.03);
	assert_near(singlePath[pathNodeIndices[2]], 2.0 / 3.0, 0.03);
	assert_near(singlePath[pathNodeIndices[3]], 0.0, 1e-6);
	for (CXSize i = 0; i < 4; i++) {
		CXIndex node = pathNodeIndices[i];
		assert_near(singlePath[node], parallelPath[node], 1e-6);
	}
	free(singlePath);
	free(parallelPath);
	CXFreeNetwork(path);

	const CXEdge weightedEdges[] = {
		{ .from = 0, .to = 1 },
		{ .from = 1, .to = 3 },
		{ .from = 0, .to = 2 },
		{ .from = 2, .to = 3 }
	};
	const float weightedValues[] = {1.0f, 1.0f, 1.0f, 10.0f};
	CXIndex weightedNodeIndices[4] = {0};
	CXIndex weightedEdgeIndices[4] = {0};
	CXNetworkRef weighted = build_network(
		CXFalse,
		4,
		weightedEdges,
		4,
		weightedValues,
		(CXString)"w",
		weightedNodeIndices,
		weightedEdgeIndices
	);

	CXSize weightedCapacity = CXNetworkNodeCapacity(weighted);
	float *full = (float *)calloc(weightedCapacity, sizeof(float));
	float *chunked = (float *)calloc(weightedCapacity, sizeof(float));
	assert(full);
	assert(chunked);
	const CXIndex firstChunkSources[2] = {weightedNodeIndices[0], weightedNodeIndices[1]};
	const CXIndex secondChunkSources[2] = {weightedNodeIndices[2], weightedNodeIndices[3]};
	CXSize fullProcessed = CXNetworkMeasureBetweennessCentrality(
		weighted,
		(CXString)"w",
		CXMeasurementExecutionParallel,
		NULL,
		0,
		CXFalse,
		CXFalse,
		full
	);
	CXSize chunkAProcessed = CXNetworkMeasureBetweennessCentrality(
		weighted,
		(CXString)"w",
		CXMeasurementExecutionParallel,
		firstChunkSources,
		2,
		CXFalse,
		CXFalse,
		chunked
	);
	CXSize chunkBProcessed = CXNetworkMeasureBetweennessCentrality(
		weighted,
		(CXString)"w",
		CXMeasurementExecutionParallel,
		secondChunkSources,
		2,
		CXFalse,
		CXTrue,
		chunked
	);

	assert(fullProcessed == 4);
	assert(chunkAProcessed == 2);
	assert(chunkBProcessed == 2);
	for (CXSize i = 0; i < 4; i++) {
		CXIndex node = weightedNodeIndices[i];
		assert_near(full[node], chunked[node], 1e-5);
	}
	assert(full[weightedNodeIndices[1]] > full[weightedNodeIndices[2]]);

	free(full);
	free(chunked);
	CXFreeNetwork(weighted);
}

int main(void) {
	test_degree_and_strength();
	test_clustering_variants();
	test_eigenvector_centrality_modes();
	test_betweenness_centrality_modes_and_chunks();
	return 0;
}
