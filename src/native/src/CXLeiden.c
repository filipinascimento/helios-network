#include "CXNetwork.h"

typedef struct {
	uint32_t state;
} CXLeidenRng;

static void CXLeidenRngSeed(CXLeidenRng *rng, uint32_t seed) {
	if (!rng) {
		return;
	}
	rng->state = seed ? seed : 0x1234567u;
}

static uint32_t CXLeidenRngNext(CXLeidenRng *rng) {
	uint32_t x = rng->state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rng->state = x;
	return x;
}

static double CXLeidenRngUnit(CXLeidenRng *rng) {
	return (double)CXLeidenRngNext(rng) / (double)UINT32_MAX;
}

static void CXLeidenShuffle(CXLeidenRng *rng, CXIndex *values, CXSize count) {
	if (!rng || !values || count <= 1) {
		return;
	}
	for (CXSize i = count - 1; i > 0; i--) {
		CXSize j = (CXSize)(CXLeidenRngNext(rng) % (uint32_t)(i + 1));
		CXIndex tmp = values[i];
		values[i] = values[j];
		values[j] = tmp;
	}
}

typedef double (*CXLeidenEdgeWeightReader)(const void *base, CXSize stride, CXIndex edge);

static double CXLeidenWeightConstantOne(const void *base, CXSize stride, CXIndex edge) {
	(void)base;
	(void)stride;
	(void)edge;
	return 1.0;
}

static double CXLeidenWeightFloat(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	float value = 0.0f;
	memcpy(&value, ptr, sizeof(float));
	return (double)value;
}

static double CXLeidenWeightDouble(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	double value = 0.0;
	memcpy(&value, ptr, sizeof(double));
	return value;
}

static double CXLeidenWeightI32(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	int32_t value = 0;
	memcpy(&value, ptr, sizeof(int32_t));
	return (double)value;
}

static double CXLeidenWeightU32(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	uint32_t value = 0;
	memcpy(&value, ptr, sizeof(uint32_t));
	return (double)value;
}

static double CXLeidenWeightI64(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	int64_t value = 0;
	memcpy(&value, ptr, sizeof(int64_t));
	return (double)value;
}

static double CXLeidenWeightU64(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	uint64_t value = 0;
	memcpy(&value, ptr, sizeof(uint64_t));
	return (double)value;
}

typedef struct {
	const void *base;
	CXSize stride;
	CXLeidenEdgeWeightReader read;
} CXLeidenEdgeWeights;

static CXBool CXLeidenResolveEdgeWeights(CXNetworkRef network, const CXString name, CXLeidenEdgeWeights *outWeights) {
	if (!outWeights) {
		return CXFalse;
	}
	outWeights->base = NULL;
	outWeights->stride = 0;
	outWeights->read = CXLeidenWeightConstantOne;

	if (!network || !name || !name[0]) {
		return CXTrue;
	}

	CXAttributeRef attribute = CXNetworkGetEdgeAttribute(network, name);
	if (!attribute || !attribute->data || attribute->dimension != 1) {
		return CXFalse;
	}

	outWeights->base = attribute->data;
	outWeights->stride = attribute->stride;

	switch (attribute->type) {
		case CXFloatAttributeType:
			outWeights->read = CXLeidenWeightFloat;
			return CXTrue;
		case CXDoubleAttributeType:
			outWeights->read = CXLeidenWeightDouble;
			return CXTrue;
		case CXIntegerAttributeType:
			outWeights->read = CXLeidenWeightI32;
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
		case CXDataAttributeCategoryType:
			outWeights->read = CXLeidenWeightU32;
			return CXTrue;
		case CXBigIntegerAttributeType:
			outWeights->read = CXLeidenWeightI64;
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			outWeights->read = CXLeidenWeightU64;
			return CXTrue;
		default:
			return CXFalse;
	}
}

typedef struct {
	CXSize nodeCount;
	CXSize outEdgeCount;
	CXIndex *outOffsets;   /* nodeCount + 1 */
	CXIndex *outNeighbors; /* outEdgeCount */
	double *outWeights;    /* outEdgeCount */
	CXSize inEdgeCount;
	CXIndex *inOffsets;   /* nodeCount + 1 */
	CXIndex *inNeighbors; /* inEdgeCount */
	double *inWeights;    /* inEdgeCount */
	double *outDegree;    /* nodeCount */
	double *inDegree;     /* nodeCount */
	double totalOutWeight;
	CXBool isDirected;
} CXLeidenGraph;

static void CXLeidenGraphDestroy(CXLeidenGraph *graph) {
	if (!graph) {
		return;
	}
	free(graph->outOffsets);
	free(graph->outNeighbors);
	free(graph->outWeights);
	free(graph->inOffsets);
	free(graph->inNeighbors);
	free(graph->inWeights);
	free(graph->outDegree);
	free(graph->inDegree);
	free(graph);
}

static CXLeidenGraph* CXLeidenGraphCreate(CXSize nodeCount, CXBool directed) {
	CXLeidenGraph *graph = calloc(1, sizeof(CXLeidenGraph));
	if (!graph) {
		return NULL;
	}
	graph->nodeCount = nodeCount;
	graph->isDirected = directed;
	graph->outOffsets = calloc(nodeCount + 1, sizeof(CXIndex));
	graph->outDegree = calloc(nodeCount, sizeof(double));
	if (!graph->outOffsets || !graph->outDegree) {
		CXLeidenGraphDestroy(graph);
		return NULL;
	}
	if (directed) {
		graph->inOffsets = calloc(nodeCount + 1, sizeof(CXIndex));
		graph->inDegree = calloc(nodeCount, sizeof(double));
		if (!graph->inOffsets || !graph->inDegree) {
			CXLeidenGraphDestroy(graph);
			return NULL;
		}
	}
	return graph;
}

static CXLeidenGraph* CXLeidenGraphFromNetwork(
	CXNetworkRef network,
	const CXLeidenEdgeWeights *weights,
	CXIndex **outCompactToNode,
	CXIndex **outNodeToCompact
) {
	if (!network || !weights) {
		return NULL;
	}

	CXSize activeCount = 0;
	for (CXIndex i = 0; i < network->nodeCapacity; i++) {
		if (network->nodeActive[i]) {
			activeCount++;
		}
	}

	CXIndex *compactToNode = NULL;
	CXIndex *nodeToCompact = NULL;
	if (activeCount > 0) {
		compactToNode = malloc(sizeof(CXIndex) * activeCount);
		nodeToCompact = malloc(sizeof(CXIndex) * network->nodeCapacity);
		if (!compactToNode || !nodeToCompact) {
			free(compactToNode);
			free(nodeToCompact);
			return NULL;
		}
		for (CXIndex i = 0; i < network->nodeCapacity; i++) {
			nodeToCompact[i] = CXIndexMAX;
		}
		CXIndex cursor = 0;
		for (CXIndex i = 0; i < network->nodeCapacity; i++) {
			if (!network->nodeActive[i]) {
				continue;
			}
			nodeToCompact[i] = cursor;
			compactToNode[cursor] = i;
			cursor++;
		}
	}

	CXLeidenGraph *graph = CXLeidenGraphCreate(activeCount, network->isDirected);
	if (!graph) {
		free(compactToNode);
		free(nodeToCompact);
		return NULL;
	}

	CXSize outEdgeCount = 0;
	CXSize inEdgeCount = 0;
	for (CXSize u = 0; u < activeCount; u++) {
		CXIndex node = compactToNode[u];
		outEdgeCount += CXNeighborContainerCount(&network->nodes[node].outNeighbors);
		if (network->isDirected) {
			inEdgeCount += CXNeighborContainerCount(&network->nodes[node].inNeighbors);
		}
	}
	graph->outEdgeCount = outEdgeCount;
	graph->inEdgeCount = network->isDirected ? inEdgeCount : 0;
	graph->outNeighbors = outEdgeCount ? malloc(sizeof(CXIndex) * outEdgeCount) : NULL;
	graph->outWeights = outEdgeCount ? malloc(sizeof(double) * outEdgeCount) : NULL;
	if ((outEdgeCount && (!graph->outNeighbors || !graph->outWeights))) {
		CXLeidenGraphDestroy(graph);
		free(compactToNode);
		free(nodeToCompact);
		return NULL;
	}
	if (network->isDirected) {
		graph->inNeighbors = inEdgeCount ? malloc(sizeof(CXIndex) * inEdgeCount) : NULL;
		graph->inWeights = inEdgeCount ? malloc(sizeof(double) * inEdgeCount) : NULL;
		if ((inEdgeCount && (!graph->inNeighbors || !graph->inWeights))) {
			CXLeidenGraphDestroy(graph);
			free(compactToNode);
			free(nodeToCompact);
			return NULL;
		}
	}

	CXIndex outCursor = 0;
	for (CXSize u = 0; u < activeCount; u++) {
		CXIndex node = compactToNode[u];
		graph->outOffsets[u] = outCursor;
		CXNeighborIterator iterator;
		CXNeighborIteratorInit(&iterator, &network->nodes[node].outNeighbors);
		while (CXNeighborIteratorNext(&iterator)) {
			CXIndex neighNode = iterator.node;
			CXIndex neighEdge = iterator.edge;
			CXIndex v = nodeToCompact[neighNode];
			if (v == CXIndexMAX) {
				continue;
			}
			double w = weights->read(weights->base, weights->stride, neighEdge);
			graph->outNeighbors[outCursor] = v;
			graph->outWeights[outCursor] = w;
			graph->outDegree[u] += w;
			outCursor++;
		}
		graph->totalOutWeight += graph->outDegree[u];
	}
	graph->outOffsets[activeCount] = outCursor;
	graph->outEdgeCount = outCursor;

	if (network->isDirected) {
		CXIndex inCursor = 0;
		for (CXSize u = 0; u < activeCount; u++) {
			CXIndex node = compactToNode[u];
			graph->inOffsets[u] = inCursor;
			CXNeighborIterator iterator;
			CXNeighborIteratorInit(&iterator, &network->nodes[node].inNeighbors);
			while (CXNeighborIteratorNext(&iterator)) {
				CXIndex neighNode = iterator.node;
				CXIndex neighEdge = iterator.edge;
				CXIndex v = nodeToCompact[neighNode];
				if (v == CXIndexMAX) {
					continue;
				}
				double w = weights->read(weights->base, weights->stride, neighEdge);
				graph->inNeighbors[inCursor] = v;
				graph->inWeights[inCursor] = w;
				graph->inDegree[u] += w;
				inCursor++;
			}
		}
		graph->inOffsets[activeCount] = inCursor;
		graph->inEdgeCount = inCursor;
	}

	if (outCompactToNode) {
		*outCompactToNode = compactToNode;
	} else {
		free(compactToNode);
	}
	if (outNodeToCompact) {
		*outNodeToCompact = nodeToCompact;
	} else {
		free(nodeToCompact);
	}
	return graph;
}

static uint32_t CXLeidenRelabelCommunities(uint32_t *community, CXSize nodeCount) {
	if (!community || nodeCount == 0) {
		return 0;
	}
	uint32_t *map = malloc(sizeof(uint32_t) * nodeCount);
	if (!map) {
		return 0;
	}
	for (CXSize i = 0; i < nodeCount; i++) {
		map[i] = UINT32_MAX;
	}
	uint32_t next = 0;
	for (CXSize i = 0; i < nodeCount; i++) {
		uint32_t old = community[i];
		if (old >= nodeCount) {
			free(map);
			return 0;
		}
		if (map[old] == UINT32_MAX) {
			map[old] = next++;
		}
	}
	for (CXSize i = 0; i < nodeCount; i++) {
		community[i] = map[community[i]];
	}
	free(map);
	return next;
}

static CXBool CXLeidenInitCommunityTotals(
	const CXLeidenGraph *graph,
	const uint32_t *community,
	double *totOut,
	double *totIn,
	uint32_t *sizes
) {
	if (!graph || !community || !totOut || !sizes) {
		return CXFalse;
	}
	for (CXSize i = 0; i < graph->nodeCount; i++) {
		totOut[i] = 0.0;
		sizes[i] = 0;
		if (totIn) {
			totIn[i] = 0.0;
		}
	}

	for (CXSize i = 0; i < graph->nodeCount; i++) {
		uint32_t c = community[i];
		if (c >= graph->nodeCount) {
			return CXFalse;
		}
		totOut[c] += graph->outDegree[i];
		if (totIn) {
			totIn[c] += graph->inDegree[i];
		}
		sizes[c] += 1;
	}
	return CXTrue;
}

static CXSize CXLeidenMoveNodes(
	const CXLeidenGraph *graph,
	uint32_t *community,
	const uint32_t *restriction,
	double resolution,
	CXLeidenRng *rng,
	CXSize maxPasses
) {
	if (!graph || !community || !rng) {
		return 0;
	}

	const CXSize n = graph->nodeCount;
	if (n == 0 || graph->totalOutWeight <= 0.0) {
		return 0;
	}

	CXIndex *order = malloc(sizeof(CXIndex) * n);
	uint32_t *candidate = NULL;
	double *candOutW = NULL;
	double *candInW = NULL;
	CXSize candidateCap = 0;
	uint32_t *stamp = malloc(sizeof(uint32_t) * n);
	uint32_t *position = malloc(sizeof(uint32_t) * n);
	double *totOut = malloc(sizeof(double) * n);
	double *totIn = graph->isDirected ? malloc(sizeof(double) * n) : NULL;
	uint32_t *sizes = malloc(sizeof(uint32_t) * n);

	if (!order || !stamp || !position || !totOut || (graph->isDirected && !totIn) || !sizes) {
		free(order);
		free(candidate);
		free(candOutW);
		free(candInW);
		free(stamp);
		free(position);
		free(totOut);
		free(totIn);
		free(sizes);
		return 0;
	}

	for (CXSize i = 0; i < n; i++) {
		order[i] = (CXIndex)i;
		stamp[i] = 0;
		position[i] = 0;
	}

	if (!CXLeidenInitCommunityTotals(graph, community, totOut, totIn, sizes)) {
		free(order);
		free(stamp);
		free(position);
		free(totOut);
		free(totIn);
		free(sizes);
		return 0;
	}

	uint32_t epoch = 1;
	CXSize movedTotal = 0;
	const double invTotal = 1.0 / graph->totalOutWeight;
	for (CXSize pass = 0; pass < maxPasses; pass++) {
		CXLeidenShuffle(rng, order, n);
		CXSize moved = 0;

		for (CXSize oi = 0; oi < n; oi++) {
			CXSize u = order[oi];
			uint32_t current = community[u];
			const uint32_t restrictLabel = restriction ? restriction[u] : UINT32_MAX;

			double degOut = graph->outDegree[u];
			double degIn = graph->isDirected ? graph->inDegree[u] : 0.0;

			if (sizes[current] == 0) {
				continue;
			}

			totOut[current] -= degOut;
			if (graph->isDirected) {
				totIn[current] -= degIn;
			}
			sizes[current] -= 1;

			CXSize maxCandidates = graph->outOffsets[u + 1] - graph->outOffsets[u];
			if (graph->isDirected) {
				maxCandidates += graph->inOffsets[u + 1] - graph->inOffsets[u];
			}
			if (maxCandidates == 0) {
				totOut[current] += degOut;
				if (graph->isDirected) {
					totIn[current] += degIn;
				}
				sizes[current] += 1;
				continue;
			}
			if (maxCandidates > candidateCap) {
				free(candidate);
				free(candOutW);
				free(candInW);
				candidate = malloc(sizeof(uint32_t) * maxCandidates);
				candOutW = malloc(sizeof(double) * maxCandidates);
				if (graph->isDirected) {
					candInW = malloc(sizeof(double) * maxCandidates);
				}
				if (!candidate || !candOutW || (graph->isDirected && !candInW)) {
					free(order);
					free(candidate);
					free(candOutW);
					free(candInW);
					free(stamp);
					free(position);
					free(totOut);
					free(totIn);
					free(sizes);
					return movedTotal;
				}
				candidateCap = maxCandidates;
			}

			epoch += 1;
			if (epoch == 0) {
				for (CXSize i = 0; i < n; i++) {
					stamp[i] = 0;
				}
				epoch = 1;
			}

			CXSize candidateCount = 0;

			for (CXIndex idx = graph->outOffsets[u]; idx < graph->outOffsets[u + 1]; idx++) {
				uint32_t v = graph->outNeighbors[idx];
				uint32_t c = community[v];
				if (restriction && restriction[v] != restrictLabel) {
					continue;
				}
				if (stamp[c] != epoch) {
					stamp[c] = epoch;
					position[c] = (uint32_t)candidateCount;
					candidate[candidateCount] = c;
					candOutW[candidateCount] = graph->outWeights[idx];
					if (graph->isDirected) {
						candInW[candidateCount] = 0.0;
					}
					candidateCount++;
				} else {
					candOutW[position[c]] += graph->outWeights[idx];
				}
			}

			if (graph->isDirected) {
				for (CXIndex idx = graph->inOffsets[u]; idx < graph->inOffsets[u + 1]; idx++) {
					uint32_t v = graph->inNeighbors[idx];
					uint32_t c = community[v];
					if (restriction && restriction[v] != restrictLabel) {
						continue;
					}
					if (stamp[c] != epoch) {
						stamp[c] = epoch;
						position[c] = (uint32_t)candidateCount;
						candidate[candidateCount] = c;
						candOutW[candidateCount] = 0.0;
						candInW[candidateCount] = graph->inWeights[idx];
						candidateCount++;
					} else {
						candInW[position[c]] += graph->inWeights[idx];
					}
				}
			}

			uint32_t bestCommunity = current;
			double bestGain = 0.0;

			for (CXSize ci = 0; ci < candidateCount; ci++) {
				uint32_t c = candidate[ci];
				double gain = 0.0;
				if (graph->isDirected) {
					double wOut = candOutW[ci];
					double wIn = candInW[ci];
					gain = (wOut + wIn) - resolution * ((degOut * totIn[c] + degIn * totOut[c]) * invTotal);
				} else {
					double w = candOutW[ci];
					gain = w - resolution * (degOut * totOut[c] * invTotal);
				}
				if (gain > bestGain + 1e-12 || (fabs(gain - bestGain) <= 1e-12 && CXLeidenRngUnit(rng) < 0.5)) {
					bestGain = gain;
					bestCommunity = c;
				}
			}

			community[u] = bestCommunity;
			totOut[bestCommunity] += degOut;
			if (graph->isDirected) {
				totIn[bestCommunity] += degIn;
			}
			sizes[bestCommunity] += 1;

			if (bestCommunity != current) {
				moved++;
			}
		}

		movedTotal += moved;
		if (moved == 0) {
			break;
		}
	}

	free(order);
	free(candidate);
	free(candOutW);
	free(candInW);
	free(stamp);
	free(position);
	free(totOut);
	free(totIn);
	free(sizes);
	return movedTotal;
}

static uint32_t* CXLeidenRefinePartition(
	const CXLeidenGraph *graph,
	const uint32_t *coarse,
	double resolution,
	CXLeidenRng *rng,
	CXSize maxPasses
) {
	if (!graph || !coarse || !rng) {
		return NULL;
	}
	uint32_t *refined = malloc(sizeof(uint32_t) * graph->nodeCount);
	if (!refined) {
		return NULL;
	}
	for (CXSize i = 0; i < graph->nodeCount; i++) {
		refined[i] = (uint32_t)i;
	}
	CXLeidenMoveNodes(graph, refined, coarse, resolution, rng, maxPasses);
	return refined;
}

static CXLeidenGraph* CXLeidenGraphAggregate(const CXLeidenGraph *graph, const uint32_t *community, uint32_t communityCount) {
	if (!graph || !community || communityCount == 0) {
		return NULL;
	}

	const CXSize approxEdges = graph->outEdgeCount ? graph->outEdgeCount : 1;
	CXSize cap = 1;
	while (cap < approxEdges * 2) {
		cap <<= 1;
	}
	uint64_t *keys = malloc(sizeof(uint64_t) * cap);
	double *values = malloc(sizeof(double) * cap);
	if (!keys || !values) {
		free(keys);
		free(values);
		return NULL;
	}
	for (CXSize i = 0; i < cap; i++) {
		keys[i] = UINT64_MAX;
		values[i] = 0.0;
	}

	for (CXSize u = 0; u < graph->nodeCount; u++) {
		uint32_t cu = community[u];
		for (CXIndex idx = graph->outOffsets[u]; idx < graph->outOffsets[u + 1]; idx++) {
			uint32_t cv = community[graph->outNeighbors[idx]];
			uint64_t key = ((uint64_t)cu << 32) | (uint64_t)cv;
			CXSize slot = (CXSize)(key * 11400714819323198485ull) & (cap - 1);
			while (CXTrue) {
				if (keys[slot] == UINT64_MAX) {
					keys[slot] = key;
					values[slot] = graph->outWeights[idx];
					break;
				}
				if (keys[slot] == key) {
					values[slot] += graph->outWeights[idx];
					break;
				}
				slot = (slot + 1) & (cap - 1);
			}
		}
	}

	uint32_t *outCounts = calloc(communityCount, sizeof(uint32_t));
	uint32_t *inCounts = graph->isDirected ? calloc(communityCount, sizeof(uint32_t)) : NULL;
	if (!outCounts || (graph->isDirected && !inCounts)) {
		free(keys);
		free(values);
		free(outCounts);
		free(inCounts);
		return NULL;
	}

	CXSize pairCount = 0;
	for (CXSize i = 0; i < cap; i++) {
		if (keys[i] == UINT64_MAX) {
			continue;
		}
		uint32_t cu = (uint32_t)(keys[i] >> 32);
		uint32_t cv = (uint32_t)(keys[i] & 0xffffffffu);
		if (cu >= communityCount || cv >= communityCount) {
			continue;
		}
		outCounts[cu] += 1;
		if (graph->isDirected) {
			inCounts[cv] += 1;
		}
		pairCount++;
	}

	CXLeidenGraph *agg = CXLeidenGraphCreate(communityCount, graph->isDirected);
	if (!agg) {
		free(keys);
		free(values);
		free(outCounts);
		free(inCounts);
		return NULL;
	}

	agg->outEdgeCount = pairCount;
	agg->outNeighbors = pairCount ? malloc(sizeof(CXIndex) * pairCount) : NULL;
	agg->outWeights = pairCount ? malloc(sizeof(double) * pairCount) : NULL;
	if (pairCount && (!agg->outNeighbors || !agg->outWeights)) {
		CXLeidenGraphDestroy(agg);
		free(keys);
		free(values);
		free(outCounts);
		free(inCounts);
		return NULL;
	}
	if (graph->isDirected) {
		agg->inEdgeCount = pairCount;
		agg->inNeighbors = pairCount ? malloc(sizeof(CXIndex) * pairCount) : NULL;
		agg->inWeights = pairCount ? malloc(sizeof(double) * pairCount) : NULL;
		if (pairCount && (!agg->inNeighbors || !agg->inWeights)) {
			CXLeidenGraphDestroy(agg);
			free(keys);
			free(values);
			free(outCounts);
			free(inCounts);
			return NULL;
		}
	}

	CXIndex outCursor = 0;
	agg->outOffsets[0] = 0;
	for (uint32_t c = 0; c < communityCount; c++) {
		outCursor += outCounts[c];
		agg->outOffsets[c + 1] = outCursor;
	}
	CXIndex inCursor = 0;
	if (graph->isDirected) {
		agg->inOffsets[0] = 0;
		for (uint32_t c = 0; c < communityCount; c++) {
			inCursor += inCounts[c];
			agg->inOffsets[c + 1] = inCursor;
		}
	}

	memset(outCounts, 0, sizeof(uint32_t) * communityCount);
	if (graph->isDirected) {
		memset(inCounts, 0, sizeof(uint32_t) * communityCount);
	}

	for (CXSize i = 0; i < cap; i++) {
		if (keys[i] == UINT64_MAX) {
			continue;
		}
		uint32_t cu = (uint32_t)(keys[i] >> 32);
		uint32_t cv = (uint32_t)(keys[i] & 0xffffffffu);
		double w = values[i];
		CXIndex outPos = agg->outOffsets[cu] + outCounts[cu]++;
		agg->outNeighbors[outPos] = cv;
		agg->outWeights[outPos] = w;
		agg->outDegree[cu] += w;
		if (graph->isDirected) {
			CXIndex inPos = agg->inOffsets[cv] + inCounts[cv]++;
			agg->inNeighbors[inPos] = cu;
			agg->inWeights[inPos] = w;
			agg->inDegree[cv] += w;
		}
	}

	agg->totalOutWeight = 0.0;
	for (CXSize i = 0; i < communityCount; i++) {
		agg->totalOutWeight += agg->outDegree[i];
	}

	free(keys);
	free(values);
	free(outCounts);
	free(inCounts);
	return agg;
}

static double CXLeidenModularity(const CXLeidenGraph *graph, const uint32_t *community, uint32_t communityCount, double resolution) {
	if (!graph || !community || communityCount == 0 || graph->totalOutWeight <= 0.0) {
		return 0.0;
	}

	double *totOut = calloc(communityCount, sizeof(double));
	double *totIn = graph->isDirected ? calloc(communityCount, sizeof(double)) : NULL;
	double *inWeight = calloc(communityCount, sizeof(double));
	if (!totOut || (graph->isDirected && !totIn) || !inWeight) {
		free(totOut);
		free(totIn);
		free(inWeight);
		return 0.0;
	}

	for (CXSize u = 0; u < graph->nodeCount; u++) {
		uint32_t c = community[u];
		if (c >= communityCount) {
			continue;
		}
		totOut[c] += graph->outDegree[u];
		if (graph->isDirected) {
			totIn[c] += graph->inDegree[u];
		}
		for (CXIndex idx = graph->outOffsets[u]; idx < graph->outOffsets[u + 1]; idx++) {
			if (community[graph->outNeighbors[idx]] == c) {
				inWeight[c] += graph->outWeights[idx];
			}
		}
	}

	double q = 0.0;
	const double m = graph->totalOutWeight;
	if (graph->isDirected) {
		for (uint32_t c = 0; c < communityCount; c++) {
			q += inWeight[c] / m - resolution * (totOut[c] / m) * (totIn[c] / m);
		}
	} else {
		for (uint32_t c = 0; c < communityCount; c++) {
			q += inWeight[c] / m - resolution * (totOut[c] / m) * (totOut[c] / m);
		}
	}

	free(totOut);
	free(totIn);
	free(inWeight);
	return q;
}

CXSize CXNetworkLeidenModularity(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	double resolution,
	uint32_t seed,
	CXSize maxLevels,
	CXSize maxPasses,
	const CXString outNodeCommunityAttribute,
	double *outModularity
) {
	if (!network || maxLevels == 0 || maxPasses == 0 || resolution <= 0.0 || !outNodeCommunityAttribute) {
		return 0;
	}

	CXLeidenEdgeWeights weights;
	if (!CXLeidenResolveEdgeWeights(network, edgeWeightAttribute, &weights)) {
		return 0;
	}

	CXIndex *compactToNode = NULL;
	CXIndex *nodeToCompact = NULL;
	CXLeidenGraph *baseGraph = CXLeidenGraphFromNetwork(network, &weights, &compactToNode, &nodeToCompact);
	free(nodeToCompact);
	nodeToCompact = NULL;
	if (!baseGraph) {
		free(compactToNode);
		return 0;
	}

	const CXSize originalCount = baseGraph->nodeCount;
	if (originalCount == 0) {
		CXLeidenGraphDestroy(baseGraph);
		free(compactToNode);
		return 0;
	}

	uint32_t *origToNode = malloc(sizeof(uint32_t) * originalCount);
	if (!origToNode) {
		CXLeidenGraphDestroy(baseGraph);
		free(compactToNode);
		return 0;
	}
	for (CXSize i = 0; i < originalCount; i++) {
		origToNode[i] = (uint32_t)i;
	}

	CXLeidenRng rng;
	CXLeidenRngSeed(&rng, seed);

	CXLeidenGraph *graph = baseGraph;
	uint32_t finalCommunityCount = 0;

	for (CXSize level = 0; level < maxLevels; level++) {
		const CXSize n = graph->nodeCount;
		uint32_t *coarse = malloc(sizeof(uint32_t) * n);
		if (!coarse) {
			break;
		}
		for (CXSize i = 0; i < n; i++) {
			coarse[i] = (uint32_t)i;
		}

		CXSize moved = CXLeidenMoveNodes(graph, coarse, NULL, resolution, &rng, maxPasses);
		uint32_t coarseCount = CXLeidenRelabelCommunities(coarse, n);
		if (coarseCount == 0) {
			free(coarse);
			break;
		}

		uint32_t *refined = CXLeidenRefinePartition(graph, coarse, resolution, &rng, maxPasses);
		free(coarse);
		if (!refined) {
			break;
		}
		uint32_t refinedCount = CXLeidenRelabelCommunities(refined, n);
		if (refinedCount == 0) {
			free(refined);
			break;
		}

		for (CXSize i = 0; i < originalCount; i++) {
			uint32_t nodeId = origToNode[i];
			if (nodeId < n) {
				origToNode[i] = refined[nodeId];
			}
		}

		finalCommunityCount = refinedCount;

		if (moved == 0 && refinedCount == n) {
			free(refined);
			break;
		}
		if (refinedCount == n) {
			free(refined);
			break;
		}

		CXLeidenGraph *next = CXLeidenGraphAggregate(graph, refined, refinedCount);
		free(refined);
		if (!next) {
			break;
		}
		if (graph != baseGraph) {
			CXLeidenGraphDestroy(graph);
		}
		graph = next;
	}

	if (graph != baseGraph) {
		CXLeidenGraphDestroy(graph);
	}

	if (finalCommunityCount == 0) {
		CXLeidenGraphDestroy(baseGraph);
		free(compactToNode);
		free(origToNode);
		return 0;
	}

	if (outModularity) {
		*outModularity = CXLeidenModularity(baseGraph, origToNode, finalCommunityCount, resolution);
	}

	CXAttributeRef attr = CXNetworkGetNodeAttribute(network, outNodeCommunityAttribute);
	if (!attr) {
		if (!CXNetworkDefineNodeAttribute(network, outNodeCommunityAttribute, CXUnsignedIntegerAttributeType, 1)) {
			CXLeidenGraphDestroy(baseGraph);
			free(compactToNode);
			free(origToNode);
			return 0;
		}
		attr = CXNetworkGetNodeAttribute(network, outNodeCommunityAttribute);
	}
	if (!attr || attr->type != CXUnsignedIntegerAttributeType || attr->dimension != 1 || !attr->data) {
		CXLeidenGraphDestroy(baseGraph);
		free(compactToNode);
		free(origToNode);
		return 0;
	}

	uint32_t *out = (uint32_t *)attr->data;
	for (CXIndex i = 0; i < network->nodeCapacity; i++) {
		out[i] = 0;
	}
	for (CXSize i = 0; i < originalCount; i++) {
		CXIndex nodeIndex = compactToNode[i];
		out[nodeIndex] = origToNode[i];
	}

	CXNetworkBumpNodeAttributeVersion(network, outNodeCommunityAttribute);

	CXLeidenGraphDestroy(baseGraph);
	free(compactToNode);
	free(origToNode);
	return finalCommunityCount;
}
