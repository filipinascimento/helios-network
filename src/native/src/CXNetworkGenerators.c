#include "CXNetwork.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint64_t state;
} CXGeneratorRandom;

typedef struct {
	uint64_t *slots;
	CXSize capacity;
	CXSize count;
	CXBool directed;
} CXGeneratorEdgeSet;

typedef struct {
	CXEdge *edges;
	CXSize count;
	CXSize capacity;
	CXGeneratorEdgeSet set;
	CXBool simple;
	CXBool allowSelfLoops;
} CXGeneratorEdges;

static uint64_t CXGeneratorSeed(uint32_t seed) {
	return seed ? seed : 0x9e3779b97f4a7c15ULL;
}

static uint64_t CXGeneratorNext(CXGeneratorRandom *rng) {
	uint64_t x = rng->state;
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	rng->state = x;
	return x * 2685821657736338717ULL;
}

static double CXGeneratorUniform(CXGeneratorRandom *rng) {
	return (double)(CXGeneratorNext(rng) >> 11) * (1.0 / 9007199254740992.0);
}

static CXSize CXGeneratorUniformIndex(CXGeneratorRandom *rng, CXSize upperExclusive) {
	if (upperExclusive == 0) {
		return 0;
	}
	return (CXSize)(CXGeneratorUniform(rng) * (double)upperExclusive);
}

static CXSize CXGeneratorRingDistance(CXIndex a, CXIndex b, CXSize nodeCount) {
	if (nodeCount == 0) {
		return 0;
	}
	CXSize forward = ((CXSize)b + nodeCount - (CXSize)a) % nodeCount;
	CXSize backward = ((CXSize)a + nodeCount - (CXSize)b) % nodeCount;
	return forward < backward ? forward : backward;
}

static uint64_t CXGeneratorEdgeKey(CXIndex from, CXIndex to, CXBool directed) {
	uint32_t a = (uint32_t)from;
	uint32_t b = (uint32_t)to;
	if (!directed && b < a) {
		uint32_t tmp = a;
		a = b;
		b = tmp;
	}
	return (((uint64_t)a) << 32) | (uint64_t)b;
}

static uint64_t CXGeneratorHash64(uint64_t value) {
	value ^= value >> 33;
	value *= 0xff51afd7ed558ccdULL;
	value ^= value >> 33;
	value *= 0xc4ceb9fe1a85ec53ULL;
	value ^= value >> 33;
	return value;
}

static CXSize CXGeneratorNextPow2(CXSize value) {
	CXSize result = 16;
	while (result < value && result < (CXSize)(CXSizeMAX / 2)) {
		result *= 2;
	}
	return result;
}

static CXBool CXGeneratorEdgeSetInit(CXGeneratorEdgeSet *set, CXSize expectedEdges, CXBool directed) {
	if (!set) {
		return CXFalse;
	}
	set->capacity = CXGeneratorNextPow2(expectedEdges * 4 + 16);
	set->count = 0;
	set->directed = directed;
	set->slots = (uint64_t *)calloc((size_t)set->capacity, sizeof(uint64_t));
	return set->slots ? CXTrue : CXFalse;
}

static void CXGeneratorEdgeSetFree(CXGeneratorEdgeSet *set) {
	if (!set) {
		return;
	}
	free(set->slots);
	set->slots = NULL;
	set->capacity = 0;
	set->count = 0;
}

static CXBool CXGeneratorEdgeSetContains(CXGeneratorEdgeSet *set, CXIndex from, CXIndex to) {
	if (!set || !set->slots || set->capacity == 0) {
		return CXFalse;
	}
	uint64_t stored = CXGeneratorEdgeKey(from, to, set->directed) + 1ULL;
	CXSize mask = set->capacity - 1;
	CXSize index = (CXSize)(CXGeneratorHash64(stored) & mask);
	for (CXSize probe = 0; probe < set->capacity; probe++) {
		uint64_t current = set->slots[index];
		if (current == 0) {
			return CXFalse;
		}
		if (current == stored) {
			return CXTrue;
		}
		index = (index + 1) & mask;
	}
	return CXFalse;
}

static CXBool CXGeneratorEdgeSetInsert(CXGeneratorEdgeSet *set, CXIndex from, CXIndex to) {
	if (!set || !set->slots || set->capacity == 0) {
		return CXFalse;
	}
	uint64_t stored = CXGeneratorEdgeKey(from, to, set->directed) + 1ULL;
	CXSize mask = set->capacity - 1;
	CXSize index = (CXSize)(CXGeneratorHash64(stored) & mask);
	for (CXSize probe = 0; probe < set->capacity; probe++) {
		uint64_t current = set->slots[index];
		if (current == stored) {
			return CXFalse;
		}
		if (current == 0) {
			set->slots[index] = stored;
			set->count++;
			return CXTrue;
		}
		index = (index + 1) & mask;
	}
	return CXFalse;
}

static CXBool CXGeneratorEdgesInit(CXGeneratorEdges *builder, CXSize expectedEdges, CXBool directed, CXBool simple, CXBool allowSelfLoops) {
	if (!builder) {
		return CXFalse;
	}
	memset(builder, 0, sizeof(*builder));
	builder->capacity = expectedEdges > 0 ? expectedEdges : 16;
	builder->edges = (CXEdge *)malloc((size_t)builder->capacity * sizeof(CXEdge));
	builder->simple = simple;
	builder->allowSelfLoops = allowSelfLoops;
	if (!builder->edges) {
		return CXFalse;
	}
	if (simple && !CXGeneratorEdgeSetInit(&builder->set, expectedEdges, directed)) {
		free(builder->edges);
		builder->edges = NULL;
		return CXFalse;
	}
	return CXTrue;
}

static void CXGeneratorEdgesFree(CXGeneratorEdges *builder) {
	if (!builder) {
		return;
	}
	free(builder->edges);
	builder->edges = NULL;
	builder->count = 0;
	builder->capacity = 0;
	CXGeneratorEdgeSetFree(&builder->set);
}

static CXBool CXGeneratorEdgesReserve(CXGeneratorEdges *builder, CXSize required) {
	if (required <= builder->capacity) {
		return CXTrue;
	}
	CXSize next = builder->capacity ? builder->capacity : 16;
	while (next < required) {
		next *= 2;
	}
	CXEdge *edges = (CXEdge *)realloc(builder->edges, (size_t)next * sizeof(CXEdge));
	if (!edges) {
		return CXFalse;
	}
	builder->edges = edges;
	builder->capacity = next;
	return CXTrue;
}

static CXBool CXGeneratorAddEdge(CXGeneratorEdges *builder, CXIndex from, CXIndex to) {
	if (!builder || (!builder->allowSelfLoops && from == to)) {
		return CXFalse;
	}
	if (builder->simple && !CXGeneratorEdgeSetInsert(&builder->set, from, to)) {
		return CXFalse;
	}
	if (!CXGeneratorEdgesReserve(builder, builder->count + 1)) {
		return CXFalse;
	}
	builder->edges[builder->count++] = (CXEdge){ .from = from, .to = to };
	return CXTrue;
}

static CXNetworkRef CXGeneratorBuildNetwork(CXSize nodeCount, CXGeneratorEdges *builder, CXBool directed) {
	CXNetworkRef network = CXNewNetworkWithCapacity(directed, nodeCount > 0 ? nodeCount : 1, builder ? builder->count : 1);
	if (!network) {
		return NULL;
	}
	if (nodeCount > 0 && !CXNetworkAddNodes(network, nodeCount, NULL)) {
		CXFreeNetwork(network);
		return NULL;
	}
	if (builder && builder->count > 0 && !CXNetworkAddEdges(network, builder->edges, builder->count, NULL)) {
		CXFreeNetwork(network);
		return NULL;
	}
	return network;
}

static CXBool CXGeneratorAddPositionAttribute(CXNetworkRef network, const double *x, const double *y, CXSize count) {
	if (!network || !x || !y) {
		return CXFalse;
	}
	if (!CXNetworkDefineNodeAttribute(network, "_helios_generator_position", CXFloatAttributeType, 2)) {
		return CXFalse;
	}
	float *positions = (float *)CXNetworkGetNodeAttributeBuffer(network, "_helios_generator_position");
	if (!positions) {
		return CXFalse;
	}
	for (CXSize i = 0; i < count; i++) {
		positions[i * 2] = (float)x[i];
		positions[i * 2 + 1] = (float)y[i];
	}
	CXNetworkBumpNodeAttributeVersion(network, "_helios_generator_position");
	return CXTrue;
}

CXNetworkRef CXNetworkGenerateWattsStrogatz(CXSize nodeCount, CXSize neighborLevel, double rewiringProbability, CXBool directed, uint32_t seed) {
	if (nodeCount == 0 || neighborLevel == 0) {
		return CXGeneratorBuildNetwork(nodeCount, NULL, directed);
	}
	if (neighborLevel >= nodeCount) {
		neighborLevel = nodeCount - 1;
	}
	if (!directed && neighborLevel > nodeCount / 2) {
		neighborLevel = nodeCount / 2;
	}
	if (rewiringProbability < 0.0) rewiringProbability = 0.0;
	if (rewiringProbability > 1.0) rewiringProbability = 1.0;

	CXGeneratorEdges builder;
	CXSize expected = nodeCount * neighborLevel;
	if (!CXGeneratorEdgesInit(&builder, expected, directed, CXTrue, CXFalse)) {
		return NULL;
	}
	CXGeneratorRandom rng = { .state = CXGeneratorSeed(seed) };
	CXSize totalSlots = nodeCount * neighborLevel;
	CXSize rewiresRemaining = (CXSize)floor((double)totalSlots * rewiringProbability + 0.5);
	CXSize slotsRemaining = totalSlots;
	for (CXSize i = 0; i < nodeCount; i++) {
		for (CXSize j = 1; j <= neighborLevel; j++) {
			CXIndex target = (CXIndex)((i + j) % nodeCount);
			CXBool shouldRewire = CXFalse;
			if (rewiresRemaining > 0 && slotsRemaining > 0) {
				if (rewiresRemaining >= slotsRemaining || CXGeneratorUniform(&rng) * (double)slotsRemaining < (double)rewiresRemaining) {
					shouldRewire = CXTrue;
				}
			}
			if (shouldRewire) {
				CXIndex rewiredTarget = target;
				for (CXSize attempt = 0; attempt < nodeCount * 4 + 16; attempt++) {
					CXIndex candidate = (CXIndex)CXGeneratorUniformIndex(&rng, nodeCount);
					if (
						candidate != i
						&& CXGeneratorRingDistance((CXIndex)i, candidate, nodeCount) > neighborLevel
						&& !CXGeneratorEdgeSetContains(&builder.set, (CXIndex)i, candidate)
					) {
						rewiredTarget = candidate;
						break;
					}
				}
				if (rewiredTarget != target) {
					target = rewiredTarget;
					rewiresRemaining--;
				}
			}
			CXGeneratorAddEdge(&builder, (CXIndex)i, target);
			slotsRemaining--;
		}
	}
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	CXGeneratorEdgesFree(&builder);
	return network;
}

CXNetworkRef CXNetworkGenerateLattice2D(CXSize rows, CXSize columns, CXSize neighborLevel, CXBool periodic, CXBool directed) {
	if (rows == 0 || columns == 0) {
		return CXGeneratorBuildNetwork(0, NULL, directed);
	}
	if (neighborLevel == 0) {
		neighborLevel = 1;
	}
	CXSize nodeCount = rows * columns;
	CXGeneratorEdges builder;
	CXSize expected = nodeCount * neighborLevel * (directed ? 4 : 2);
	if (!CXGeneratorEdgesInit(&builder, expected, directed, CXTrue, CXFalse)) {
		return NULL;
	}
	for (CXSize row = 0; row < rows; row++) {
		for (CXSize col = 0; col < columns; col++) {
			CXIndex from = (CXIndex)(row * columns + col);
			for (CXSize level = 1; level <= neighborLevel; level++) {
				if (col + level < columns || periodic) {
					CXIndex to = (CXIndex)(row * columns + ((col + level) % columns));
					CXGeneratorAddEdge(&builder, from, to);
					if (directed) CXGeneratorAddEdge(&builder, to, from);
				}
				if (row + level < rows || periodic) {
					CXIndex to = (CXIndex)(((row + level) % rows) * columns + col);
					CXGeneratorAddEdge(&builder, from, to);
					if (directed) CXGeneratorAddEdge(&builder, to, from);
				}
			}
		}
	}
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	CXGeneratorEdgesFree(&builder);
	return network;
}

CXNetworkRef CXNetworkGenerateStochasticBlockModel(
	CXSize blockCount,
	const CXSize *blockSizes,
	const double *probabilities,
	CXBool directed,
	uint32_t seed
) {
	if (blockCount == 0 || !blockSizes || !probabilities) {
		return NULL;
	}
	CXSize nodeCount = 0;
	for (CXSize i = 0; i < blockCount; i++) {
		nodeCount += blockSizes[i];
	}
	CXGeneratorEdges builder;
	CXSize expected = nodeCount * 8;
	if (!CXGeneratorEdgesInit(&builder, expected, directed, CXTrue, CXFalse)) {
		return NULL;
	}
	CXSize *nodeBlock = (CXSize *)malloc((size_t)nodeCount * sizeof(CXSize));
	if (!nodeBlock) {
		CXGeneratorEdgesFree(&builder);
		return NULL;
	}
	CXSize offset = 0;
	for (CXSize block = 0; block < blockCount; block++) {
		for (CXSize i = 0; i < blockSizes[block]; i++) {
			nodeBlock[offset++] = block;
		}
	}
	CXGeneratorRandom rng = { .state = CXGeneratorSeed(seed) };
	for (CXSize i = 0; i < nodeCount; i++) {
		CXSize start = directed ? 0 : i + 1;
		for (CXSize j = start; j < nodeCount; j++) {
			if (i == j) {
				continue;
			}
			double p = probabilities[nodeBlock[i] * blockCount + nodeBlock[j]];
			if (p > 0.0 && CXGeneratorUniform(&rng) < p) {
				CXGeneratorAddEdge(&builder, (CXIndex)i, (CXIndex)j);
			}
		}
	}
	free(nodeBlock);
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	CXGeneratorEdgesFree(&builder);
	return network;
}

CXNetworkRef CXNetworkGenerateBarabasiAlbert(CXSize nodeCount, CXSize edgesPerNewNode, CXSize initialCliqueSize, CXBool directed, uint32_t seed) {
	if (nodeCount == 0) {
		return CXGeneratorBuildNetwork(0, NULL, directed);
	}
	if (edgesPerNewNode == 0) {
		edgesPerNewNode = 1;
	}
	if (initialCliqueSize == 0) {
		initialCliqueSize = edgesPerNewNode + 1;
	}
	if (initialCliqueSize > nodeCount) {
		initialCliqueSize = nodeCount;
	}
	if (edgesPerNewNode >= initialCliqueSize) {
		edgesPerNewNode = initialCliqueSize > 1 ? initialCliqueSize - 1 : 1;
	}
	CXGeneratorEdges builder;
	if (!CXGeneratorEdgesInit(&builder, nodeCount * edgesPerNewNode + initialCliqueSize * initialCliqueSize, directed, CXTrue, CXFalse)) {
		return NULL;
	}
	uint32_t *degree = (uint32_t *)calloc((size_t)nodeCount, sizeof(uint32_t));
	CXIndex *chosen = (CXIndex *)calloc((size_t)edgesPerNewNode, sizeof(CXIndex));
	if (!degree || !chosen) {
		free(degree);
		free(chosen);
		CXGeneratorEdgesFree(&builder);
		return NULL;
	}
	for (CXSize i = 0; i < initialCliqueSize; i++) {
		for (CXSize j = i + 1; j < initialCliqueSize; j++) {
			if (CXGeneratorAddEdge(&builder, (CXIndex)i, (CXIndex)j)) {
				degree[i]++;
				degree[j]++;
			}
		}
	}
	CXGeneratorRandom rng = { .state = CXGeneratorSeed(seed) };
	for (CXSize node = initialCliqueSize; node < nodeCount; node++) {
		CXSize selected = 0;
		for (CXSize edge = 0; edge < edgesPerNewNode && selected < node; edge++) {
			uint64_t total = 0;
			for (CXSize i = 0; i < node; i++) {
				total += degree[i] ? degree[i] : 1;
			}
			CXIndex target = 0;
			for (CXSize attempt = 0; attempt < node * 4 + 16; attempt++) {
				uint64_t pick = (uint64_t)(CXGeneratorUniform(&rng) * (double)total);
				uint64_t cumulative = 0;
				for (CXSize i = 0; i < node; i++) {
					cumulative += degree[i] ? degree[i] : 1;
					if (pick < cumulative) {
						target = (CXIndex)i;
						break;
					}
				}
				CXBool duplicate = CXFalse;
				for (CXSize k = 0; k < selected; k++) {
					if (chosen[k] == target) {
						duplicate = CXTrue;
						break;
					}
				}
				if (!duplicate) {
					break;
				}
			}
			chosen[selected++] = target;
			if (CXGeneratorAddEdge(&builder, (CXIndex)node, target)) {
				degree[node]++;
				degree[target]++;
			}
		}
	}
	free(chosen);
	free(degree);
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	CXGeneratorEdgesFree(&builder);
	return network;
}

CXNetworkRef CXNetworkGenerateRandomGeometric(CXSize nodeCount, double radius, CXBool directed, uint32_t seed) {
	if (radius < 0.0) {
		radius = 0.0;
	}
	CXGeneratorRandom rng = { .state = CXGeneratorSeed(seed) };
	double *x = (double *)malloc((size_t)nodeCount * sizeof(double));
	double *y = (double *)malloc((size_t)nodeCount * sizeof(double));
	if ((nodeCount && (!x || !y))) {
		free(x);
		free(y);
		return NULL;
	}
	for (CXSize i = 0; i < nodeCount; i++) {
		x[i] = CXGeneratorUniform(&rng);
		y[i] = CXGeneratorUniform(&rng);
	}
	CXGeneratorEdges builder;
	if (!CXGeneratorEdgesInit(&builder, nodeCount * 8, directed, CXTrue, CXFalse)) {
		free(x);
		free(y);
		return NULL;
	}
	double r2 = radius * radius;
	for (CXSize i = 0; i < nodeCount; i++) {
		CXSize start = directed ? 0 : i + 1;
		for (CXSize j = start; j < nodeCount; j++) {
			if (i == j) continue;
			double dx = x[i] - x[j];
			double dy = y[i] - y[j];
			if (dx * dx + dy * dy <= r2) {
				CXGeneratorAddEdge(&builder, (CXIndex)i, (CXIndex)j);
			}
		}
	}
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	if (network) {
		CXGeneratorAddPositionAttribute(network, x, y, nodeCount);
	}
	CXGeneratorEdgesFree(&builder);
	free(x);
	free(y);
	return network;
}

CXNetworkRef CXNetworkGenerateWaxman(CXSize nodeCount, double alpha, double beta, CXBool directed, uint32_t seed) {
	if (alpha <= 0.0) {
		alpha = 0.4;
	}
	if (beta < 0.0) beta = 0.0;
	if (beta > 1.0) beta = 1.0;
	CXGeneratorRandom rng = { .state = CXGeneratorSeed(seed) };
	double *x = (double *)malloc((size_t)nodeCount * sizeof(double));
	double *y = (double *)malloc((size_t)nodeCount * sizeof(double));
	if ((nodeCount && (!x || !y))) {
		free(x);
		free(y);
		return NULL;
	}
	for (CXSize i = 0; i < nodeCount; i++) {
		x[i] = CXGeneratorUniform(&rng);
		y[i] = CXGeneratorUniform(&rng);
	}
	CXGeneratorEdges builder;
	if (!CXGeneratorEdgesInit(&builder, nodeCount * 8, directed, CXTrue, CXFalse)) {
		free(x);
		free(y);
		return NULL;
	}
	const double L = 1.4142135623730951;
	for (CXSize i = 0; i < nodeCount; i++) {
		CXSize start = directed ? 0 : i + 1;
		for (CXSize j = start; j < nodeCount; j++) {
			if (i == j) continue;
			double dx = x[i] - x[j];
			double dy = y[i] - y[j];
			double distance = sqrt(dx * dx + dy * dy);
			double p = beta * exp(-distance / (alpha * L));
			if (CXGeneratorUniform(&rng) < p) {
				CXGeneratorAddEdge(&builder, (CXIndex)i, (CXIndex)j);
			}
		}
	}
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	if (network) {
		CXGeneratorAddPositionAttribute(network, x, y, nodeCount);
	}
	CXGeneratorEdgesFree(&builder);
	free(x);
	free(y);
	return network;
}

CXNetworkRef CXNetworkGenerateConfigurationModel(
	CXSize nodeCount,
	const CXSize *degrees,
	CXBool directed,
	CXBool allowSelfLoops,
	CXBool allowMultiEdges,
	uint32_t seed
) {
	if (!degrees && nodeCount > 0) {
		return NULL;
	}
	CXSize stubCount = 0;
	for (CXSize i = 0; i < nodeCount; i++) {
		stubCount += degrees[i];
	}
	if (stubCount % 2 != 0) {
		return NULL;
	}
	CXIndex *stubs = (CXIndex *)malloc((size_t)stubCount * sizeof(CXIndex));
	if (stubCount && !stubs) {
		return NULL;
	}
	CXSize offset = 0;
	for (CXSize node = 0; node < nodeCount; node++) {
		for (CXSize j = 0; j < degrees[node]; j++) {
			stubs[offset++] = (CXIndex)node;
		}
	}
	CXGeneratorRandom rng = { .state = CXGeneratorSeed(seed) };
	for (CXSize i = stubCount; i > 1; i--) {
		CXSize j = CXGeneratorUniformIndex(&rng, i);
		CXIndex tmp = stubs[i - 1];
		stubs[i - 1] = stubs[j];
		stubs[j] = tmp;
	}
	CXGeneratorEdges builder;
	if (!CXGeneratorEdgesInit(&builder, stubCount / 2, directed, !allowMultiEdges, allowSelfLoops)) {
		free(stubs);
		return NULL;
	}
	for (CXSize i = 0; i + 1 < stubCount; i += 2) {
		if (!allowSelfLoops && stubs[i] == stubs[i + 1]) {
			continue;
		}
		CXGeneratorAddEdge(&builder, stubs[i], stubs[i + 1]);
	}
	free(stubs);
	CXNetworkRef network = CXGeneratorBuildNetwork(nodeCount, &builder, directed);
	CXGeneratorEdgesFree(&builder);
	return network;
}
