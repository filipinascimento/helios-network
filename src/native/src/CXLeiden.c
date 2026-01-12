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

typedef struct {
	const CXLeidenGraph *graph;
	uint32_t *community;
	const uint32_t *restriction;
	double resolution;
	CXLeidenRng *rng;
	CXSize maxPasses;

	CXIndex *order;
	CXSize orderPos;
	CXSize pass;
	CXSize movedInPass;
	CXSize movedTotal;
	CXBool active;

	uint32_t epoch;
	uint32_t *stamp;
	uint32_t *position;
	double *totOut;
	double *totIn;
	uint32_t *sizes;

	uint32_t *candidate;
	double *candOutW;
	double *candInW;
	CXSize candidateCap;
} CXLeidenMoveState;

static CXSize CXLeidenGraphMaxCandidateCount(const CXLeidenGraph *graph) {
	if (!graph) {
		return 0;
	}
	CXSize max = 0;
	for (CXSize u = 0; u < graph->nodeCount; u++) {
		CXSize c = (CXSize)(graph->outOffsets[u + 1] - graph->outOffsets[u]);
		if (graph->isDirected) {
			c += (CXSize)(graph->inOffsets[u + 1] - graph->inOffsets[u]);
		}
		if (c > max) {
			max = c;
		}
	}
	return max;
}

static void CXLeidenMoveStateClear(CXLeidenMoveState *state) {
	if (!state) {
		return;
	}
	memset(state, 0, sizeof(*state));
	state->epoch = 1;
}

static void CXLeidenMoveStateDestroy(CXLeidenMoveState *state) {
	if (!state) {
		return;
	}
	free(state->order);
	free(state->stamp);
	free(state->position);
	free(state->totOut);
	free(state->totIn);
	free(state->sizes);
	free(state->candidate);
	free(state->candOutW);
	free(state->candInW);
	CXLeidenMoveStateClear(state);
}

static CXBool CXLeidenMoveStateInit(
	CXLeidenMoveState *state,
	const CXLeidenGraph *graph,
	uint32_t *community,
	const uint32_t *restriction,
	double resolution,
	CXLeidenRng *rng,
	CXSize maxPasses
) {
	if (!state || !graph || !community || !rng || maxPasses == 0) {
		return CXFalse;
	}
	CXLeidenMoveStateDestroy(state);
	state->graph = graph;
	state->community = community;
	state->restriction = restriction;
	state->resolution = resolution;
	state->rng = rng;
	state->maxPasses = maxPasses;
	state->active = CXTrue;
	state->epoch = 1;

	const CXSize n = graph->nodeCount;
	state->order = malloc(sizeof(CXIndex) * n);
	state->stamp = malloc(sizeof(uint32_t) * n);
	state->position = malloc(sizeof(uint32_t) * n);
	state->totOut = malloc(sizeof(double) * n);
	state->totIn = graph->isDirected ? malloc(sizeof(double) * n) : NULL;
	state->sizes = malloc(sizeof(uint32_t) * n);
	if (!state->order || !state->stamp || !state->position || !state->totOut || (graph->isDirected && !state->totIn) || !state->sizes) {
		return CXFalse;
	}
	for (CXSize i = 0; i < n; i++) {
		state->order[i] = (CXIndex)i;
		state->stamp[i] = 0;
		state->position[i] = 0;
	}
	if (!CXLeidenInitCommunityTotals(graph, community, state->totOut, state->totIn, state->sizes)) {
		return CXFalse;
	}
	state->candidateCap = CXLeidenGraphMaxCandidateCount(graph);
	if (state->candidateCap > 0) {
		state->candidate = malloc(sizeof(uint32_t) * state->candidateCap);
		state->candOutW = malloc(sizeof(double) * state->candidateCap);
		state->candInW = graph->isDirected ? malloc(sizeof(double) * state->candidateCap) : NULL;
		if (!state->candidate || !state->candOutW || (graph->isDirected && !state->candInW)) {
			return CXFalse;
		}
	}
	CXLeidenShuffle(rng, state->order, n);
	return CXTrue;
}

static CXBool CXLeidenMoveStateStep(CXLeidenMoveState *state, CXSize budget) {
	if (!state || !state->active || !state->graph || !state->community || !state->rng) {
		return CXTrue;
	}
	const CXLeidenGraph *graph = state->graph;
	const CXSize n = graph->nodeCount;
	if (n == 0 || graph->totalOutWeight <= 0.0) {
		state->active = CXFalse;
		return CXTrue;
	}

	const double invTotal = 1.0 / graph->totalOutWeight;
	if (budget == 0) {
		budget = 1;
	}

	CXSize steps = 0;
	while (steps < budget && state->pass < state->maxPasses) {
		if (state->orderPos >= n) {
			state->movedTotal += state->movedInPass;
			if (state->movedInPass == 0) {
				state->active = CXFalse;
				return CXTrue;
			}
			state->pass += 1;
			if (state->pass >= state->maxPasses) {
				state->active = CXFalse;
				return CXTrue;
			}
			state->orderPos = 0;
			state->movedInPass = 0;
			CXLeidenShuffle(state->rng, state->order, n);
			continue;
		}

		CXSize u = (CXSize)state->order[state->orderPos++];
		steps += 1;

		uint32_t current = state->community[u];
		const uint32_t restrictLabel = state->restriction ? state->restriction[u] : UINT32_MAX;

		double degOut = graph->outDegree[u];
		double degIn = graph->isDirected ? graph->inDegree[u] : 0.0;

		state->totOut[current] -= degOut;
		if (graph->isDirected) {
			state->totIn[current] -= degIn;
		}
		state->sizes[current] -= 1;

		CXSize candidateCount = 0;
		state->epoch += 1;
		if (state->epoch == 0) {
			for (CXSize i = 0; i < n; i++) {
				state->stamp[i] = 0;
			}
			state->epoch = 1;
		}

		for (CXIndex idx = graph->outOffsets[u]; idx < graph->outOffsets[u + 1]; idx++) {
			uint32_t v = (uint32_t)graph->outNeighbors[idx];
			uint32_t c = state->community[v];
			if (state->restriction && state->restriction[v] != restrictLabel) {
				continue;
			}
			if (state->stamp[c] != state->epoch) {
				state->stamp[c] = state->epoch;
				state->position[c] = (uint32_t)candidateCount;
				state->candidate[candidateCount] = c;
				state->candOutW[candidateCount] = graph->outWeights[idx];
				if (graph->isDirected) {
					state->candInW[candidateCount] = 0.0;
				}
				candidateCount++;
			} else {
				state->candOutW[state->position[c]] += graph->outWeights[idx];
			}
		}

		if (graph->isDirected) {
			for (CXIndex idx = graph->inOffsets[u]; idx < graph->inOffsets[u + 1]; idx++) {
				uint32_t v = (uint32_t)graph->inNeighbors[idx];
				uint32_t c = state->community[v];
				if (state->restriction && state->restriction[v] != restrictLabel) {
					continue;
				}
				if (state->stamp[c] != state->epoch) {
					state->stamp[c] = state->epoch;
					state->position[c] = (uint32_t)candidateCount;
					state->candidate[candidateCount] = c;
					state->candOutW[candidateCount] = 0.0;
					state->candInW[candidateCount] = graph->inWeights[idx];
					candidateCount++;
				} else {
					state->candInW[state->position[c]] += graph->inWeights[idx];
				}
			}
		}

		uint32_t bestCommunity = current;
		double bestGain = 0.0;
		for (CXSize ci = 0; ci < candidateCount; ci++) {
			uint32_t c = state->candidate[ci];
			double gain = 0.0;
			if (graph->isDirected) {
				double wOut = state->candOutW[ci];
				double wIn = state->candInW[ci];
				gain = (wOut + wIn) - state->resolution * ((degOut * state->totIn[c] + degIn * state->totOut[c]) * invTotal);
			} else {
				double w = state->candOutW[ci];
				gain = w - state->resolution * (degOut * state->totOut[c] * invTotal);
			}
			if (gain > bestGain + 1e-12 || (fabs(gain - bestGain) <= 1e-12 && CXLeidenRngUnit(state->rng) < 0.5)) {
				bestGain = gain;
				bestCommunity = c;
			}
		}

		state->community[u] = bestCommunity;
		state->totOut[bestCommunity] += degOut;
		if (graph->isDirected) {
			state->totIn[bestCommunity] += degIn;
		}
		state->sizes[bestCommunity] += 1;
		if (bestCommunity != current) {
			state->movedInPass += 1;
		}
	}

	if (state->pass >= state->maxPasses) {
		state->active = CXFalse;
		return CXTrue;
	}
	return CXFalse;
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

struct CXLeidenSession {
	CXNetworkRef network;
	CXLeidenEdgeWeights weights;
	double resolution;
	CXSize maxLevels;
	CXSize maxPasses;
	CXSize level;
	CXLeidenPhase phase;
	CXLeidenRng rng;

	CXLeidenGraph *baseGraph;
	CXLeidenGraph *graph;
	CXIndex *compactToNode;
	uint32_t *origToNode;
	CXSize originalCount;

	uint32_t *coarse;
	uint32_t coarseCount;
	uint32_t *refined;
	uint32_t refinedCount;

	CXLeidenMoveState moveState;
};

static void CXLeidenSessionReleaseLevelState(CXLeidenSession *session) {
	if (!session) {
		return;
	}
	free(session->coarse);
	free(session->refined);
	session->coarse = NULL;
	session->refined = NULL;
	session->coarseCount = 0;
	session->refinedCount = 0;
	CXLeidenMoveStateDestroy(&session->moveState);
}

CXLeidenSessionRef CXLeidenSessionCreate(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	double resolution,
	uint32_t seed,
	CXSize maxLevels,
	CXSize maxPasses
) {
	if (!network || resolution <= 0.0 || maxLevels == 0 || maxPasses == 0) {
		return NULL;
	}

	CXLeidenSession *session = calloc(1, sizeof(CXLeidenSession));
	if (!session) {
		return NULL;
	}
	session->network = network;
	session->resolution = resolution;
	session->maxLevels = maxLevels;
	session->maxPasses = maxPasses;
	session->level = 0;
	session->phase = CXLeidenPhaseBuildGraph;
	CXLeidenRngSeed(&session->rng, seed);
	CXLeidenMoveStateClear(&session->moveState);

	if (!CXLeidenResolveEdgeWeights(network, edgeWeightAttribute, &session->weights)) {
		session->phase = CXLeidenPhaseFailed;
		return session;
	}

	CXIndex *nodeToCompact = NULL;
	session->baseGraph = CXLeidenGraphFromNetwork(network, &session->weights, &session->compactToNode, &nodeToCompact);
	free(nodeToCompact);
	if (!session->baseGraph) {
		session->phase = CXLeidenPhaseFailed;
		return session;
	}
	session->graph = session->baseGraph;
	session->originalCount = session->baseGraph->nodeCount;
	if (session->originalCount == 0) {
		session->phase = CXLeidenPhaseFailed;
		return session;
	}
	session->origToNode = malloc(sizeof(uint32_t) * session->originalCount);
	if (!session->origToNode) {
		session->phase = CXLeidenPhaseFailed;
		return session;
	}
	for (CXSize i = 0; i < session->originalCount; i++) {
		session->origToNode[i] = (uint32_t)i;
	}

	session->phase = CXLeidenPhaseCoarseMove;
	return session;
}

void CXLeidenSessionDestroy(CXLeidenSessionRef sessionRef) {
	CXLeidenSession *session = (CXLeidenSession *)sessionRef;
	if (!session) {
		return;
	}
	CXLeidenSessionReleaseLevelState(session);
	if (session->graph && session->graph != session->baseGraph) {
		CXLeidenGraphDestroy(session->graph);
	}
	CXLeidenGraphDestroy(session->baseGraph);
	free(session->compactToNode);
	free(session->origToNode);
	free(session);
}

static CXBool CXLeidenSessionStartCoarse(CXLeidenSession *session) {
	if (!session || !session->graph) {
		return CXFalse;
	}
	CXLeidenSessionReleaseLevelState(session);
	const CXSize n = session->graph->nodeCount;
	session->coarse = malloc(sizeof(uint32_t) * n);
	if (!session->coarse) {
		return CXFalse;
	}
	for (CXSize i = 0; i < n; i++) {
		session->coarse[i] = (uint32_t)i;
	}
	if (!CXLeidenMoveStateInit(&session->moveState, session->graph, session->coarse, NULL, session->resolution, &session->rng, session->maxPasses)) {
		return CXFalse;
	}
	session->phase = CXLeidenPhaseCoarseMove;
	return CXTrue;
}

static CXBool CXLeidenSessionFinishCoarse(CXLeidenSession *session) {
	if (!session || !session->coarse || !session->graph) {
		return CXFalse;
	}
	session->coarseCount = CXLeidenRelabelCommunities(session->coarse, session->graph->nodeCount);
	return session->coarseCount > 0;
}

static CXBool CXLeidenSessionStartRefine(CXLeidenSession *session) {
	if (!session || !session->graph || !session->coarse) {
		return CXFalse;
	}
	CXLeidenMoveStateDestroy(&session->moveState);
	const CXSize n = session->graph->nodeCount;
	session->refined = malloc(sizeof(uint32_t) * n);
	if (!session->refined) {
		return CXFalse;
	}
	for (CXSize i = 0; i < n; i++) {
		session->refined[i] = (uint32_t)i;
	}
	if (!CXLeidenMoveStateInit(&session->moveState, session->graph, session->refined, session->coarse, session->resolution, &session->rng, session->maxPasses)) {
		return CXFalse;
	}
	session->phase = CXLeidenPhaseRefineMove;
	return CXTrue;
}

static CXBool CXLeidenSessionFinishRefine(CXLeidenSession *session) {
	if (!session || !session->refined || !session->graph) {
		return CXFalse;
	}
	session->refinedCount = CXLeidenRelabelCommunities(session->refined, session->graph->nodeCount);
	if (session->refinedCount == 0) {
		return CXFalse;
	}
	for (CXSize i = 0; i < session->originalCount; i++) {
		uint32_t nodeId = session->origToNode[i];
		if (nodeId < session->graph->nodeCount) {
			session->origToNode[i] = session->refined[nodeId];
		}
	}
	return CXTrue;
}

static CXBool CXLeidenSessionAggregate(CXLeidenSession *session) {
	if (!session || !session->graph || !session->refined || session->refinedCount == 0) {
		return CXFalse;
	}
	CXLeidenGraph *next = CXLeidenGraphAggregate(session->graph, session->refined, session->refinedCount);
	if (!next) {
		return CXFalse;
	}
	if (session->graph != session->baseGraph) {
		CXLeidenGraphDestroy(session->graph);
	}
	session->graph = next;
	session->level += 1;
	return CXTrue;
}

CXLeidenPhase CXLeidenSessionStep(CXLeidenSessionRef sessionRef, CXSize budget) {
	CXLeidenSession *session = (CXLeidenSession *)sessionRef;
	if (!session) {
		return CXLeidenPhaseFailed;
	}
	if (session->phase == CXLeidenPhaseFailed || session->phase == CXLeidenPhaseDone) {
		return session->phase;
	}
	if (session->level >= session->maxLevels) {
		session->phase = CXLeidenPhaseDone;
		return session->phase;
	}

	if (session->phase == CXLeidenPhaseCoarseMove && !session->moveState.active) {
		if (!CXLeidenSessionStartCoarse(session)) {
			session->phase = CXLeidenPhaseFailed;
			return session->phase;
		}
	}

	if (session->phase == CXLeidenPhaseCoarseMove) {
		if (!CXLeidenMoveStateStep(&session->moveState, budget)) {
			return session->phase;
		}
		CXLeidenMoveStateDestroy(&session->moveState);
		if (!CXLeidenSessionFinishCoarse(session) || !CXLeidenSessionStartRefine(session)) {
			session->phase = CXLeidenPhaseFailed;
			return session->phase;
		}
		return session->phase;
	}

	if (session->phase == CXLeidenPhaseRefineMove) {
		if (!CXLeidenMoveStateStep(&session->moveState, budget)) {
			return session->phase;
		}
		CXLeidenMoveStateDestroy(&session->moveState);
		if (!CXLeidenSessionFinishRefine(session)) {
			session->phase = CXLeidenPhaseFailed;
			return session->phase;
		}
		if (session->refinedCount == session->graph->nodeCount) {
			session->phase = CXLeidenPhaseDone;
			return session->phase;
		}
		session->phase = CXLeidenPhaseAggregate;
	}

	if (session->phase == CXLeidenPhaseAggregate) {
		if (!CXLeidenSessionAggregate(session)) {
			session->phase = CXLeidenPhaseFailed;
			return session->phase;
		}
		if (session->level >= session->maxLevels) {
			session->phase = CXLeidenPhaseDone;
			return session->phase;
		}
		session->phase = CXLeidenPhaseCoarseMove;
		return session->phase;
	}

	session->phase = CXLeidenPhaseFailed;
	return session->phase;
}

void CXLeidenSessionGetProgress(
	CXLeidenSessionRef sessionRef,
	double *outProgress01,
	CXLeidenPhase *outPhase,
	CXSize *outLevel,
	CXSize *outMaxLevels,
	CXSize *outPass,
	CXSize *outMaxPasses,
	CXSize *outVisitedThisPass,
	CXSize *outNodeCount,
	uint32_t *outCommunityCount
) {
	CXLeidenSession *session = (CXLeidenSession *)sessionRef;
	if (!session) {
		if (outProgress01) *outProgress01 = 0.0;
		if (outPhase) *outPhase = CXLeidenPhaseFailed;
		if (outLevel) *outLevel = 0;
		if (outMaxLevels) *outMaxLevels = 0;
		if (outPass) *outPass = 0;
		if (outMaxPasses) *outMaxPasses = 0;
		if (outVisitedThisPass) *outVisitedThisPass = 0;
		if (outNodeCount) *outNodeCount = 0;
		if (outCommunityCount) *outCommunityCount = 0;
		return;
	}

	const CXSize n = session->graph ? session->graph->nodeCount : 0;
	const double levelProgress = (double)session->level / (double)(session->maxLevels ? session->maxLevels : 1);
	double phaseBase = 0.0;
	double phaseSpan = 1.0;
	switch (session->phase) {
		case CXLeidenPhaseCoarseMove:
			phaseBase = 0.15;
			phaseSpan = 0.45;
			break;
		case CXLeidenPhaseRefineMove:
			phaseBase = 0.60;
			phaseSpan = 0.30;
			break;
		case CXLeidenPhaseAggregate:
			phaseBase = 0.90;
			phaseSpan = 0.10;
			break;
		case CXLeidenPhaseDone:
			phaseBase = 1.0;
			phaseSpan = 0.0;
			break;
		default:
			phaseBase = 0.0;
			phaseSpan = 0.15;
			break;
	}
	double withinPhase = 0.0;
	if (session->moveState.active && n > 0) {
		withinPhase = (double)session->moveState.orderPos / (double)n;
	}
	double progress01 = CXMIN(1.0, levelProgress + (phaseBase + phaseSpan * withinPhase) / (double)(session->maxLevels ? session->maxLevels : 1));

	if (outProgress01) *outProgress01 = progress01;
	if (outPhase) *outPhase = session->phase;
	if (outLevel) *outLevel = session->level;
	if (outMaxLevels) *outMaxLevels = session->maxLevels;
	if (outPass) *outPass = session->moveState.active ? session->moveState.pass : 0;
	if (outMaxPasses) *outMaxPasses = session->maxPasses;
	if (outVisitedThisPass) *outVisitedThisPass = session->moveState.active ? session->moveState.orderPos : 0;
	if (outNodeCount) *outNodeCount = n;
	if (outCommunityCount) *outCommunityCount = session->refinedCount ? session->refinedCount : session->coarseCount;
}

CXBool CXLeidenSessionFinalize(
	CXLeidenSessionRef sessionRef,
	const CXString outNodeCommunityAttribute,
	double *outModularity,
	uint32_t *outCommunityCount
) {
	CXLeidenSession *session = (CXLeidenSession *)sessionRef;
	if (!session || session->phase != CXLeidenPhaseDone || !outNodeCommunityAttribute) {
		return CXFalse;
	}
	if (!session->baseGraph || !session->origToNode || session->originalCount == 0) {
		return CXFalse;
	}

	uint32_t communityCount = 0;
	for (CXSize i = 0; i < session->originalCount; i++) {
		uint32_t c = session->origToNode[i];
		if (c + 1 > communityCount) {
			communityCount = c + 1;
		}
	}
	if (communityCount == 0) {
		return CXFalse;
	}

	if (outModularity) {
		*outModularity = CXLeidenModularity(session->baseGraph, session->origToNode, communityCount, session->resolution);
	}

	CXAttributeRef attr = CXNetworkGetNodeAttribute(session->network, outNodeCommunityAttribute);
	if (!attr) {
		if (!CXNetworkDefineNodeAttribute(session->network, outNodeCommunityAttribute, CXUnsignedIntegerAttributeType, 1)) {
			return CXFalse;
		}
		attr = CXNetworkGetNodeAttribute(session->network, outNodeCommunityAttribute);
	}
	if (!attr || attr->type != CXUnsignedIntegerAttributeType || attr->dimension != 1 || !attr->data) {
		return CXFalse;
	}

	uint32_t *out = (uint32_t *)attr->data;
	for (CXIndex i = 0; i < session->network->nodeCapacity; i++) {
		out[i] = 0;
	}
	for (CXSize i = 0; i < session->originalCount; i++) {
		CXIndex nodeIndex = session->compactToNode[i];
		out[nodeIndex] = session->origToNode[i];
	}

	CXNetworkBumpNodeAttributeVersion(session->network, outNodeCommunityAttribute);
	if (outCommunityCount) {
		*outCommunityCount = communityCount;
	}
	return CXTrue;
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

	CXLeidenSessionRef session = CXLeidenSessionCreate(network, edgeWeightAttribute, resolution, seed, maxLevels, maxPasses);
	if (!session) {
		return 0;
	}
	CXLeidenPhase phase = CXLeidenPhaseBuildGraph;
	while (phase != CXLeidenPhaseDone && phase != CXLeidenPhaseFailed) {
		phase = CXLeidenSessionStep(session, 1000000);
	}
	uint32_t communityCount = 0;
	CXBool ok = (phase == CXLeidenPhaseDone) && CXLeidenSessionFinalize(session, outNodeCommunityAttribute, outModularity, &communityCount);
	CXLeidenSessionDestroy(session);
	return ok ? (CXSize)communityCount : 0;
}
