#include "CXNetwork.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CX_DIMENSION_MAX_PARALLEL_WORKERS 32u
#define CX_DIMENSION_FORWARD_MAX_ORDER 6u
#define CX_DIMENSION_BACKWARD_MAX_ORDER 6u
#define CX_DIMENSION_CENTRAL_MAX_ORDER 4u

static const double CXDimensionCentralDifferenceCoeffs[4][4] = {
	{0.5, 0.0, 0.0, 0.0},
	{2.0 / 3.0, -1.0 / 12.0, 0.0, 0.0},
	{3.0 / 4.0, -3.0 / 20.0, 1.0 / 60.0, 0.0},
	{4.0 / 5.0, -1.0 / 5.0, 4.0 / 105.0, -1.0 / 280.0}
};

static const double CXDimensionForwardDifferenceCoeffs[6][7] = {
	{-1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0},
	{-3.0 / 2.0, 2.0, -1.0 / 2.0, 0.0, 0.0, 0.0, 0.0},
	{-11.0 / 6.0, 3.0, -3.0 / 2.0, 1.0 / 3.0, 0.0, 0.0, 0.0},
	{-25.0 / 12.0, 4.0, -3.0, 4.0 / 3.0, -1.0 / 4.0, 0.0, 0.0},
	{-137.0 / 60.0, 5.0, -5.0, 10.0 / 3.0, -5.0 / 4.0, 1.0 / 5.0, 0.0},
	{-49.0 / 20.0, 6.0, -15.0 / 2.0, 20.0 / 3.0, -15.0 / 4.0, 6.0 / 5.0, -1.0 / 6.0}
};

static CXDimensionDifferenceMethod CXDimensionNormalizeMethod(CXDimensionDifferenceMethod method) {
	switch (method) {
		case CXDimensionForwardDifferenceMethod:
		case CXDimensionBackwardDifferenceMethod:
		case CXDimensionCentralDifferenceMethod:
		case CXDimensionLeastSquaresDifferenceMethod:
			return method;
		default:
			return CXDimensionLeastSquaresDifferenceMethod;
	}
}

static CXSize CXDimensionNormalizeOrder(CXSize order) {
	return order == 0 ? 1 : order;
}

static CXBool CXDimensionValidateOrder(CXDimensionDifferenceMethod method, CXSize order) {
	if (order < 1) {
		return CXFalse;
	}
	switch (method) {
		case CXDimensionForwardDifferenceMethod:
			return order <= CX_DIMENSION_FORWARD_MAX_ORDER;
		case CXDimensionBackwardDifferenceMethod:
			return order <= CX_DIMENSION_BACKWARD_MAX_ORDER;
		case CXDimensionCentralDifferenceMethod:
			return order <= CX_DIMENSION_CENTRAL_MAX_ORDER;
		case CXDimensionLeastSquaresDifferenceMethod:
			return CXTrue;
		default:
			return CXFalse;
	}
}

static CXSize CXDimensionExtraPadding(CXDimensionDifferenceMethod method, CXSize order) {
	if (method == CXDimensionForwardDifferenceMethod
		|| method == CXDimensionCentralDifferenceMethod
		|| method == CXDimensionLeastSquaresDifferenceMethod) {
		return order;
	}
	return 0;
}

static float CXDimensionEstimateFromCapacity(
	const uint32_t *capacity,
	CXSize capacityMaxLevel,
	CXSize radius,
	CXDimensionDifferenceMethod method,
	CXSize order
) {
	if (!capacity || radius > capacityMaxLevel || capacity[radius] == 0) {
		return 0.0f;
	}

	double derivative = 0.0;
	switch (method) {
		case CXDimensionForwardDifferenceMethod: {
			if (radius + order > capacityMaxLevel) {
				return 0.0f;
			}
			const double *coefficients = CXDimensionForwardDifferenceCoeffs[order - 1];
			for (CXSize offset = 0; offset <= order; offset++) {
				CXSize r = radius + offset;
				if (r == 0) {
					continue;
				}
				derivative += coefficients[offset] * (double)capacity[r];
			}
			break;
		}
		case CXDimensionBackwardDifferenceMethod: {
			const double *coefficients = CXDimensionForwardDifferenceCoeffs[order - 1];
			for (CXSize offset = 0; offset <= order; offset++) {
				if (offset > radius) {
					continue;
				}
				CXSize r = radius - offset;
				if (r == 0) {
					continue;
				}
				derivative += (-coefficients[offset]) * (double)capacity[r];
			}
			break;
		}
		case CXDimensionCentralDifferenceMethod: {
			if (radius + order > capacityMaxLevel) {
				return 0.0f;
			}
			const double *coefficients = CXDimensionCentralDifferenceCoeffs[order - 1];
			for (CXSize offset = 1; offset <= order; offset++) {
				if (offset <= radius) {
					CXSize rb = radius - offset;
					if (rb > 0) {
						derivative += (-coefficients[offset - 1]) * (double)capacity[rb];
					}
				}
				CXSize rf = radius + offset;
				if (rf > 0 && rf <= capacityMaxLevel) {
					derivative += coefficients[offset - 1] * (double)capacity[rf];
				}
			}
			break;
		}
		case CXDimensionLeastSquaresDifferenceMethod: {
			double sumXY = 0.0;
			double sumX = 0.0;
			double sumY = 0.0;
			double sumXX = 0.0;
			double count = 0.0;
			if (radius > order) {
				for (int64_t offset = -(int64_t)order; offset <= (int64_t)order; offset++) {
					int64_t riSigned = (int64_t)radius + offset;
					if (riSigned <= 0 || (CXSize)riSigned > capacityMaxLevel) {
						continue;
					}
					CXSize ri = (CXSize)riSigned;
					double value = (double)capacity[ri];
					if (value <= 0.0) {
						continue;
					}
					double logR = log((double)ri);
					double logV = log(value);
					sumXY += logV * logR;
					sumX += logR;
					sumY += logV;
					sumXX += logR * logR;
					count += 1.0;
				}
			}
			double denom = count * sumXX - sumX * sumX;
			if (denom == 0.0 || !isfinite(denom)) {
				return 0.0f;
			}
			double slope = (count * sumXY - sumX * sumY) / denom;
			return isfinite(slope) ? (float)slope : 0.0f;
		}
		default:
			return 0.0f;
	}

	double value = derivative * ((double)radius) / (double)capacity[radius];
	return isfinite(value) ? (float)value : 0.0f;
}

static float CXDimensionEstimateFromAverageSeries(
	const double *series,
	CXSize capacityMaxLevel,
	CXSize radius,
	CXDimensionDifferenceMethod method,
	CXSize order
) {
	if (!series || radius > capacityMaxLevel || series[radius] <= 0.0) {
		return 0.0f;
	}

	double derivative = 0.0;
	switch (method) {
		case CXDimensionForwardDifferenceMethod: {
			if (radius + order > capacityMaxLevel) {
				return 0.0f;
			}
			const double *coefficients = CXDimensionForwardDifferenceCoeffs[order - 1];
			for (CXSize offset = 0; offset <= order; offset++) {
				CXSize r = radius + offset;
				if (r == 0) {
					continue;
				}
				derivative += coefficients[offset] * series[r];
			}
			break;
		}
		case CXDimensionBackwardDifferenceMethod: {
			const double *coefficients = CXDimensionForwardDifferenceCoeffs[order - 1];
			for (CXSize offset = 0; offset <= order; offset++) {
				if (offset > radius) {
					continue;
				}
				CXSize r = radius - offset;
				if (r == 0) {
					continue;
				}
				derivative += (-coefficients[offset]) * series[r];
			}
			break;
		}
		case CXDimensionCentralDifferenceMethod: {
			if (radius + order > capacityMaxLevel) {
				return 0.0f;
			}
			const double *coefficients = CXDimensionCentralDifferenceCoeffs[order - 1];
			for (CXSize offset = 1; offset <= order; offset++) {
				if (offset <= radius) {
					CXSize rb = radius - offset;
					if (rb > 0) {
						derivative += (-coefficients[offset - 1]) * series[rb];
					}
				}
				CXSize rf = radius + offset;
				if (rf > 0 && rf <= capacityMaxLevel) {
					derivative += coefficients[offset - 1] * series[rf];
				}
			}
			break;
		}
		case CXDimensionLeastSquaresDifferenceMethod: {
			double sumXY = 0.0;
			double sumX = 0.0;
			double sumY = 0.0;
			double sumXX = 0.0;
			double count = 0.0;
			if (radius > order) {
				for (int64_t offset = -(int64_t)order; offset <= (int64_t)order; offset++) {
					int64_t riSigned = (int64_t)radius + offset;
					if (riSigned <= 0 || (CXSize)riSigned > capacityMaxLevel) {
						continue;
					}
					CXSize ri = (CXSize)riSigned;
					double value = series[ri];
					if (value <= 0.0) {
						continue;
					}
					double logR = log((double)ri);
					double logV = log(value);
					sumXY += logV * logR;
					sumX += logR;
					sumY += logV;
					sumXX += logR * logR;
					count += 1.0;
				}
			}
			double denom = count * sumXX - sumX * sumX;
			if (denom == 0.0 || !isfinite(denom)) {
				return 0.0f;
			}
			double slope = (count * sumXY - sumX * sumY) / denom;
			return isfinite(slope) ? (float)slope : 0.0f;
		}
		default:
			return 0.0f;
	}

	double value = derivative * ((double)radius) / series[radius];
	return isfinite(value) ? (float)value : 0.0f;
}

static CXBool CXDimensionComputeNodeCapacity(
	CXNetworkRef network,
	CXIndex source,
	CXSize maxLevel,
	int32_t *distances,
	CXIndex *queue,
	uint32_t *levelCounts,
	uint32_t *capacity
) {
	if (!network || !distances || !queue || !levelCounts || !capacity) {
		return CXFalse;
	}
	if (source >= network->nodeCapacity || !network->nodeActive[source]) {
		return CXFalse;
	}

	CXSize levels = maxLevel + 1;
	memset(levelCounts, 0, levels * sizeof(uint32_t));

	CXSize head = 0;
	CXSize tail = 0;
	queue[tail++] = source;
	distances[source] = 0;

	while (head < tail) {
		CXIndex node = queue[head++];
		int32_t distance = distances[node];
		if (distance < 0) {
			continue;
		}
		if ((CXSize)distance > maxLevel) {
			continue;
		}
		if (levelCounts[distance] < UINT32_MAX) {
			levelCounts[distance] += 1;
		}
		if ((CXSize)distance == maxLevel) {
			continue;
		}

		CXNeighborContainer *neighbors = &network->nodes[node].outNeighbors;
		if (neighbors->storageType == CXNeighborListType) {
			CXNeighborList *list = &neighbors->storage.list;
			for (CXSize i = 0; i < list->count; i++) {
				CXIndex neighborNode = list->nodes[i];
				if (neighborNode >= network->nodeCapacity || !network->nodeActive[neighborNode]) {
					continue;
				}
				if (distances[neighborNode] >= 0) {
					continue;
				}
				distances[neighborNode] = distance + 1;
				queue[tail++] = neighborNode;
			}
		} else {
			CXNeighborMap *map = &neighbors->storage.map;
			if (map->edgeToNode) {
				CXUIntegerDictionaryFOR(entry, map->edgeToNode) {
					CXIndex *nodePtr = (CXIndex *)entry->data;
					CXIndex neighborNode = nodePtr ? *nodePtr : CXIndexMAX;
					if (neighborNode >= network->nodeCapacity || !network->nodeActive[neighborNode]) {
						continue;
					}
					if (distances[neighborNode] >= 0) {
						continue;
					}
					distances[neighborNode] = distance + 1;
					queue[tail++] = neighborNode;
				}
			}
		}
	}

	uint64_t running = 0;
	for (CXSize r = 0; r <= maxLevel; r++) {
		running += (uint64_t)levelCounts[r];
		capacity[r] = running > UINT32_MAX ? UINT32_MAX : (uint32_t)running;
	}

	for (CXSize i = 0; i < tail; i++) {
		distances[queue[i]] = -1;
	}

	return CXTrue;
}

CXBool CXNetworkMeasureNodeDimension(
	CXNetworkRef network,
	CXIndex node,
	CXSize maxLevel,
	CXDimensionDifferenceMethod method,
	CXSize order,
	uint32_t *outCapacity,
	float *outDimension
) {
	if (!network) {
		return CXFalse;
	}
	if (node >= network->nodeCapacity || !network->nodeActive[node]) {
		return CXFalse;
	}

	method = CXDimensionNormalizeMethod(method);
	order = CXDimensionNormalizeOrder(order);
	if (!CXDimensionValidateOrder(method, order)) {
		return CXFalse;
	}

	CXSize extraPadding = CXDimensionExtraPadding(method, order);
	CXSize capacityMaxLevel = maxLevel + extraPadding;
	CXSize outputLevels = maxLevel + 1;
	CXSize capacityLevels = capacityMaxLevel + 1;
	int32_t *distances = (int32_t *)malloc(network->nodeCapacity * sizeof(int32_t));
	CXIndex *queue = (CXIndex *)malloc(network->nodeCapacity * sizeof(CXIndex));
	uint32_t *levelCounts = (uint32_t *)calloc(capacityLevels, sizeof(uint32_t));
	uint32_t *capacity = (uint32_t *)calloc(capacityLevels, sizeof(uint32_t));
	if (!distances || !queue || !levelCounts || !capacity) {
		free(distances);
		free(queue);
		free(levelCounts);
		free(capacity);
		return CXFalse;
	}

	for (CXSize i = 0; i < network->nodeCapacity; i++) {
		distances[i] = -1;
	}

	CXBool ok = CXDimensionComputeNodeCapacity(network, node, capacityMaxLevel, distances, queue, levelCounts, capacity);
	if (!ok) {
		free(distances);
		free(queue);
		free(levelCounts);
		free(capacity);
		return CXFalse;
	}

	if (outCapacity) {
		memcpy(outCapacity, capacity, outputLevels * sizeof(uint32_t));
	}
	if (outDimension) {
		outDimension[0] = 0.0f;
		for (CXSize r = 1; r <= maxLevel; r++) {
			outDimension[r] = CXDimensionEstimateFromCapacity(capacity, capacityMaxLevel, r, method, order);
		}
	}

	free(distances);
	free(queue);
	free(levelCounts);
	free(capacity);
	return CXTrue;
}

CXSize CXNetworkMeasureDimension(
	CXNetworkRef network,
	const CXIndex *nodes,
	CXSize nodeCount,
	CXSize maxLevel,
	CXDimensionDifferenceMethod method,
	CXSize order,
	float *outAverageCapacity,
	float *outGlobalDimension,
	float *outAverageNodeDimension,
	float *outNodeDimensionStddev
) {
	if (!network || network->nodeCount == 0) {
		return 0;
	}

	method = CXDimensionNormalizeMethod(method);
	order = CXDimensionNormalizeOrder(order);
	if (!CXDimensionValidateOrder(method, order)) {
		return 0;
	}

	CXSize extraPadding = CXDimensionExtraPadding(method, order);
	CXSize capacityMaxLevel = maxLevel + extraPadding;
	CXSize outputLevels = maxLevel + 1;
	CXSize capacityLevels = capacityMaxLevel + 1;

	CXIndex *selectedNodes = NULL;
	CXSize selectedCount = 0;
	if (nodes && nodeCount > 0) {
		selectedNodes = (CXIndex *)malloc(nodeCount * sizeof(CXIndex));
		if (!selectedNodes) {
			return 0;
		}
		for (CXSize i = 0; i < nodeCount; i++) {
			CXIndex node = nodes[i];
			if (node < network->nodeCapacity && network->nodeActive[node]) {
				selectedNodes[selectedCount++] = node;
			}
		}
	} else {
		selectedNodes = (CXIndex *)malloc(network->nodeCount * sizeof(CXIndex));
		if (!selectedNodes) {
			return 0;
		}
		for (CXSize node = 0; node < network->nodeCapacity; node++) {
			if (network->nodeActive[node]) {
				selectedNodes[selectedCount++] = node;
			}
		}
	}

	if (selectedCount == 0) {
		free(selectedNodes);
		return 0;
	}

	double *sumCapacity = (double *)calloc(capacityLevels, sizeof(double));
	double *sumLocalDimension = (double *)calloc(outputLevels, sizeof(double));
	double *sumSqLocalDimension = (double *)calloc(outputLevels, sizeof(double));
	double *averageCapacity = (double *)calloc(capacityLevels, sizeof(double));
	if (!sumCapacity || !sumLocalDimension || !sumSqLocalDimension || !averageCapacity) {
		free(selectedNodes);
		free(sumCapacity);
		free(sumLocalDimension);
		free(sumSqLocalDimension);
		free(averageCapacity);
		return 0;
	}

	CXSize workerCount = CXMIN(selectedCount, (CXSize)CX_DIMENSION_MAX_PARALLEL_WORKERS);
	if (workerCount == 0) {
		workerCount = 1;
	}
	CXSize chunkSize = 1 + ((selectedCount - 1) / workerCount);

	CXParallelForStart(dimensionMeasureLoop, workerIndex, workerCount) {
		CXSize start = workerIndex * chunkSize;
		CXSize end = CXMIN(selectedCount, (workerIndex + 1) * chunkSize);
		if (start >= end) {
			continue;
		}

		int32_t *distances = (int32_t *)malloc(network->nodeCapacity * sizeof(int32_t));
		CXIndex *queue = (CXIndex *)malloc(network->nodeCapacity * sizeof(CXIndex));
		uint32_t *levelCounts = (uint32_t *)calloc(capacityLevels, sizeof(uint32_t));
		uint32_t *capacity = (uint32_t *)calloc(capacityLevels, sizeof(uint32_t));
		float *localDimensions = (float *)calloc(outputLevels, sizeof(float));
		double *localCapacitySum = (double *)calloc(capacityLevels, sizeof(double));
		double *localDimensionSum = (double *)calloc(outputLevels, sizeof(double));
		double *localDimensionSqSum = (double *)calloc(outputLevels, sizeof(double));

		if (!distances || !queue || !levelCounts || !capacity || !localDimensions || !localCapacitySum || !localDimensionSum || !localDimensionSqSum) {
			free(distances);
			free(queue);
			free(levelCounts);
			free(capacity);
			free(localDimensions);
			free(localCapacitySum);
			free(localDimensionSum);
			free(localDimensionSqSum);
			continue;
		}

		for (CXSize i = 0; i < network->nodeCapacity; i++) {
			distances[i] = -1;
		}

		for (CXSize idx = start; idx < end; idx++) {
			CXIndex node = selectedNodes[idx];
			if (!CXDimensionComputeNodeCapacity(network, node, capacityMaxLevel, distances, queue, levelCounts, capacity)) {
				continue;
			}
			for (CXSize r = 0; r <= capacityMaxLevel; r++) {
				localCapacitySum[r] += (double)capacity[r];
			}

			localDimensions[0] = 0.0f;
			for (CXSize r = 1; r <= maxLevel; r++) {
				localDimensions[r] = CXDimensionEstimateFromCapacity(capacity, capacityMaxLevel, r, method, order);
			}
			for (CXSize r = 0; r <= maxLevel; r++) {
				double value = (double)localDimensions[r];
				localDimensionSum[r] += value;
				localDimensionSqSum[r] += value * value;
			}
		}

		CXParallelLoopCriticalRegionStart(dimensionMeasureLoop) {
			for (CXSize r = 0; r <= capacityMaxLevel; r++) {
				sumCapacity[r] += localCapacitySum[r];
			}
			for (CXSize r = 0; r <= maxLevel; r++) {
				sumLocalDimension[r] += localDimensionSum[r];
				sumSqLocalDimension[r] += localDimensionSqSum[r];
			}
		}
		CXParallelLoopCriticalRegionEnd(dimensionMeasureLoop);

		free(distances);
		free(queue);
		free(levelCounts);
		free(capacity);
		free(localDimensions);
		free(localCapacitySum);
		free(localDimensionSum);
		free(localDimensionSqSum);
	}
	CXParallelForEnd(dimensionMeasureLoop);

	double invCount = 1.0 / (double)selectedCount;
	for (CXSize r = 0; r <= capacityMaxLevel; r++) {
		double avgCapacity = sumCapacity[r] * invCount;
		averageCapacity[r] = avgCapacity;
	}

	for (CXSize r = 0; r <= maxLevel; r++) {
		double avgLocal = sumLocalDimension[r] * invCount;
		if (outAverageCapacity) {
			outAverageCapacity[r] = (float)averageCapacity[r];
		}
		if (outAverageNodeDimension) {
			outAverageNodeDimension[r] = (float)avgLocal;
		}
		if (outNodeDimensionStddev) {
			double variance = sumSqLocalDimension[r] * invCount - avgLocal * avgLocal;
			if (variance < 0.0) {
				variance = 0.0;
			}
			outNodeDimensionStddev[r] = (float)sqrt(variance);
		}
	}

	if (outGlobalDimension) {
		outGlobalDimension[0] = 0.0f;
		for (CXSize r = 1; r <= maxLevel; r++) {
			outGlobalDimension[r] = CXDimensionEstimateFromAverageSeries(averageCapacity, capacityMaxLevel, r, method, order);
		}
	}

	free(selectedNodes);
	free(sumCapacity);
	free(sumLocalDimension);
	free(sumSqLocalDimension);
	free(averageCapacity);
	return selectedCount;
}

// Additional node measurements ------------------------------------------------

#define CX_MEASUREMENT_MAX_PARALLEL_WORKERS 32u
#define CX_MEASUREMENT_WEIGHT_EPSILON 1e-12
#define CX_MEASUREMENT_EIGENVECTOR_SHIFT 1.0

typedef double (*CXMeasurementEdgeWeightReader)(const void *base, CXSize stride, CXIndex edge);

typedef struct {
	const void *base;
	CXSize stride;
	CXMeasurementEdgeWeightReader read;
} CXMeasurementEdgeWeights;

typedef struct {
	CXBool directed;
	CXSize nodeCount;
	CXSize nodeCapacity;
	CXIndex *compactToNode; /* nodeCount */
	CXIndex *nodeToCompact; /* nodeCapacity */
	CXIndex *outOffsets;    /* nodeCount + 1 */
	CXIndex *outNeighbors;  /* outEdgeCount */
	double *outWeights;     /* outEdgeCount */
	CXSize outEdgeCount;
	CXIndex *inOffsets;     /* nodeCount + 1 */
	CXIndex *inNeighbors;   /* inEdgeCount */
	double *inWeights;      /* inEdgeCount */
	CXSize inEdgeCount;
} CXMeasurementGraph;

typedef struct {
	CXSize size;
	CXSize capacity;
	CXIndex *nodes;
	double *keys;
} CXMeasurementMinHeap;

static double CXMeasurementWeightConstantOne(const void *base, CXSize stride, CXIndex edge) {
	(void)base;
	(void)stride;
	(void)edge;
	return 1.0;
}

static double CXMeasurementWeightFloat(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	float value = 0.0f;
	memcpy(&value, ptr, sizeof(float));
	return (double)value;
}

static double CXMeasurementWeightDouble(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	double value = 0.0;
	memcpy(&value, ptr, sizeof(double));
	return value;
}

static double CXMeasurementWeightI32(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	int32_t value = 0;
	memcpy(&value, ptr, sizeof(int32_t));
	return (double)value;
}

static double CXMeasurementWeightU32(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	uint32_t value = 0;
	memcpy(&value, ptr, sizeof(uint32_t));
	return (double)value;
}

static double CXMeasurementWeightI64(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	int64_t value = 0;
	memcpy(&value, ptr, sizeof(int64_t));
	return (double)value;
}

static double CXMeasurementWeightU64(const void *base, CXSize stride, CXIndex edge) {
	const uint8_t *ptr = (const uint8_t *)base + (size_t)edge * stride;
	uint64_t value = 0;
	memcpy(&value, ptr, sizeof(uint64_t));
	return (double)value;
}

static CXNeighborDirection CXMeasurementNormalizeDirection(CXNetworkRef network, CXNeighborDirection direction) {
	if (direction == CXNeighborDirectionOut || direction == CXNeighborDirectionIn || direction == CXNeighborDirectionBoth) {
		if (network && !network->isDirected) {
			return CXNeighborDirectionOut;
		}
		return direction;
	}
	return (network && !network->isDirected) ? CXNeighborDirectionOut : CXNeighborDirectionBoth;
}

static CXStrengthMeasure CXMeasurementNormalizeStrengthMeasure(CXStrengthMeasure measure) {
	switch (measure) {
		case CXStrengthMeasureSum:
		case CXStrengthMeasureAverage:
		case CXStrengthMeasureMaximum:
		case CXStrengthMeasureMinimum:
			return measure;
		default:
			return CXStrengthMeasureSum;
	}
}

static CXClusteringCoefficientVariant CXMeasurementNormalizeClusteringVariant(CXClusteringCoefficientVariant variant) {
	switch (variant) {
		case CXClusteringCoefficientUnweighted:
		case CXClusteringCoefficientOnnela:
		case CXClusteringCoefficientNewman:
			return variant;
		default:
			return CXClusteringCoefficientUnweighted;
	}
}

static CXMeasurementExecutionMode CXMeasurementNormalizeExecutionMode(CXMeasurementExecutionMode mode) {
	switch (mode) {
		case CXMeasurementExecutionSingleThread:
		case CXMeasurementExecutionParallel:
		case CXMeasurementExecutionAuto:
			return mode;
		default:
			return CXMeasurementExecutionAuto;
	}
}

static CXSize CXMeasurementResolveWorkerCount(CXMeasurementExecutionMode mode, CXSize taskCount) {
	if (taskCount == 0) {
		return 0;
	}
	mode = CXMeasurementNormalizeExecutionMode(mode);
	if (mode == CXMeasurementExecutionSingleThread) {
		return 1;
	}
#if CX_ENABLE_PARALLELISM
	CXSize workers = CXMIN(taskCount, (CXSize)CX_MEASUREMENT_MAX_PARALLEL_WORKERS);
	return workers == 0 ? 1 : workers;
#else
	(void)mode;
	return 1;
#endif
}

static CXBool CXMeasurementResolveEdgeWeights(CXNetworkRef network, const CXString name, CXMeasurementEdgeWeights *outWeights) {
	if (!outWeights) {
		return CXFalse;
	}
	outWeights->base = NULL;
	outWeights->stride = 0;
	outWeights->read = CXMeasurementWeightConstantOne;

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
			outWeights->read = CXMeasurementWeightFloat;
			return CXTrue;
		case CXDoubleAttributeType:
			outWeights->read = CXMeasurementWeightDouble;
			return CXTrue;
		case CXIntegerAttributeType:
			outWeights->read = CXMeasurementWeightI32;
			return CXTrue;
		case CXUnsignedIntegerAttributeType:
		case CXDataAttributeCategoryType:
			outWeights->read = CXMeasurementWeightU32;
			return CXTrue;
		case CXBigIntegerAttributeType:
			outWeights->read = CXMeasurementWeightI64;
			return CXTrue;
		case CXUnsignedBigIntegerAttributeType:
			outWeights->read = CXMeasurementWeightU64;
			return CXTrue;
		default:
			return CXFalse;
	}
}

static void CXMeasurementGraphDestroy(CXMeasurementGraph *graph) {
	if (!graph) {
		return;
	}
	free(graph->compactToNode);
	free(graph->nodeToCompact);
	free(graph->outOffsets);
	free(graph->outNeighbors);
	free(graph->outWeights);
	free(graph->inOffsets);
	free(graph->inNeighbors);
	free(graph->inWeights);
	memset(graph, 0, sizeof(*graph));
}

static CXBool CXMeasurementGraphBuild(
	CXMeasurementGraph *outGraph,
	CXNetworkRef network,
	const CXMeasurementEdgeWeights *weights
) {
	if (!outGraph || !network || !weights) {
		return CXFalse;
	}
	memset(outGraph, 0, sizeof(*outGraph));

	outGraph->directed = network->isDirected;
	outGraph->nodeCapacity = network->nodeCapacity;
	for (CXIndex node = 0; node < network->nodeCapacity; node++) {
		if (network->nodeActive[node]) {
			outGraph->nodeCount += 1;
		}
	}
	if (outGraph->nodeCount == 0) {
		return CXTrue;
	}

	outGraph->compactToNode = (CXIndex *)malloc(outGraph->nodeCount * sizeof(CXIndex));
	outGraph->nodeToCompact = (CXIndex *)malloc(outGraph->nodeCapacity * sizeof(CXIndex));
	if (!outGraph->compactToNode || !outGraph->nodeToCompact) {
		CXMeasurementGraphDestroy(outGraph);
		return CXFalse;
	}

	for (CXIndex node = 0; node < outGraph->nodeCapacity; node++) {
		outGraph->nodeToCompact[node] = CXIndexMAX;
	}
	CXIndex cursor = 0;
	for (CXIndex node = 0; node < network->nodeCapacity; node++) {
		if (!network->nodeActive[node]) {
			continue;
		}
		outGraph->compactToNode[cursor] = node;
		outGraph->nodeToCompact[node] = cursor;
		cursor += 1;
	}

	outGraph->outOffsets = (CXIndex *)calloc(outGraph->nodeCount + 1, sizeof(CXIndex));
	outGraph->inOffsets = (CXIndex *)calloc(outGraph->nodeCount + 1, sizeof(CXIndex));
	if (!outGraph->outOffsets || !outGraph->inOffsets) {
		CXMeasurementGraphDestroy(outGraph);
		return CXFalse;
	}

	CXSize outEdgeCount = 0;
	CXSize inEdgeCount = 0;
	for (CXIndex u = 0; u < outGraph->nodeCount; u++) {
		CXIndex node = outGraph->compactToNode[u];
		outEdgeCount += CXNeighborContainerCount(&network->nodes[node].outNeighbors);
		inEdgeCount += CXNeighborContainerCount(&network->nodes[node].inNeighbors);
	}
	outGraph->outEdgeCount = outEdgeCount;
	outGraph->inEdgeCount = inEdgeCount;
	outGraph->outNeighbors = outEdgeCount ? (CXIndex *)malloc(outEdgeCount * sizeof(CXIndex)) : NULL;
	outGraph->outWeights = outEdgeCount ? (double *)malloc(outEdgeCount * sizeof(double)) : NULL;
	outGraph->inNeighbors = inEdgeCount ? (CXIndex *)malloc(inEdgeCount * sizeof(CXIndex)) : NULL;
	outGraph->inWeights = inEdgeCount ? (double *)malloc(inEdgeCount * sizeof(double)) : NULL;
	if ((outEdgeCount && (!outGraph->outNeighbors || !outGraph->outWeights))
		|| (inEdgeCount && (!outGraph->inNeighbors || !outGraph->inWeights))) {
		CXMeasurementGraphDestroy(outGraph);
		return CXFalse;
	}

	CXIndex outCursor = 0;
	CXIndex inCursor = 0;
	for (CXIndex u = 0; u < outGraph->nodeCount; u++) {
		CXIndex node = outGraph->compactToNode[u];
		outGraph->outOffsets[u] = outCursor;
		CXNeighborIterator iterator;
		CXNeighborIteratorInit(&iterator, &network->nodes[node].outNeighbors);
		while (CXNeighborIteratorNext(&iterator)) {
			CXIndex neighbor = iterator.node;
			if (neighbor >= outGraph->nodeCapacity || !network->nodeActive[neighbor]) {
				continue;
			}
			CXIndex v = outGraph->nodeToCompact[neighbor];
			if (v == CXIndexMAX) {
				continue;
			}
			outGraph->outNeighbors[outCursor] = v;
			outGraph->outWeights[outCursor] = weights->read(weights->base, weights->stride, iterator.edge);
			outCursor += 1;
		}
		outGraph->inOffsets[u] = inCursor;
		CXNeighborIteratorInit(&iterator, &network->nodes[node].inNeighbors);
		while (CXNeighborIteratorNext(&iterator)) {
			CXIndex neighbor = iterator.node;
			if (neighbor >= outGraph->nodeCapacity || !network->nodeActive[neighbor]) {
				continue;
			}
			CXIndex v = outGraph->nodeToCompact[neighbor];
			if (v == CXIndexMAX) {
				continue;
			}
			outGraph->inNeighbors[inCursor] = v;
			outGraph->inWeights[inCursor] = weights->read(weights->base, weights->stride, iterator.edge);
			inCursor += 1;
		}
	}
	outGraph->outOffsets[outGraph->nodeCount] = outCursor;
	outGraph->inOffsets[outGraph->nodeCount] = inCursor;
	outGraph->outEdgeCount = outCursor;
	outGraph->inEdgeCount = inCursor;
	return CXTrue;
}

static CXBool CXMeasurementMinHeapInit(CXMeasurementMinHeap *heap, CXSize capacity) {
	if (!heap) {
		return CXFalse;
	}
	heap->size = 0;
	heap->capacity = capacity > 0 ? capacity : 1;
	heap->nodes = (CXIndex *)malloc(heap->capacity * sizeof(CXIndex));
	heap->keys = (double *)malloc(heap->capacity * sizeof(double));
	if (!heap->nodes || !heap->keys) {
		free(heap->nodes);
		free(heap->keys);
		memset(heap, 0, sizeof(*heap));
		return CXFalse;
	}
	return CXTrue;
}

static void CXMeasurementMinHeapDestroy(CXMeasurementMinHeap *heap) {
	if (!heap) {
		return;
	}
	free(heap->nodes);
	free(heap->keys);
	memset(heap, 0, sizeof(*heap));
}

static CXBool CXMeasurementMinHeapPush(CXMeasurementMinHeap *heap, CXIndex node, double key) {
	if (!heap) {
		return CXFalse;
	}
	if (heap->size >= heap->capacity) {
		CXSize nextCapacity = CXCapacityGrow(heap->capacity);
		CXIndex *nextNodes = (CXIndex *)malloc(nextCapacity * sizeof(CXIndex));
		double *nextKeys = (double *)malloc(nextCapacity * sizeof(double));
		if (!nextNodes || !nextKeys) {
			free(nextNodes);
			free(nextKeys);
			return CXFalse;
		}
		memcpy(nextNodes, heap->nodes, heap->size * sizeof(CXIndex));
		memcpy(nextKeys, heap->keys, heap->size * sizeof(double));
		free(heap->nodes);
		free(heap->keys);
		heap->nodes = nextNodes;
		heap->keys = nextKeys;
		heap->capacity = nextCapacity;
	}

	CXSize index = heap->size++;
	while (index > 0) {
		CXSize parent = (index - 1) / 2;
		if (heap->keys[parent] <= key) {
			break;
		}
		heap->nodes[index] = heap->nodes[parent];
		heap->keys[index] = heap->keys[parent];
		index = parent;
	}
	heap->nodes[index] = node;
	heap->keys[index] = key;
	return CXTrue;
}

static CXBool CXMeasurementMinHeapPop(CXMeasurementMinHeap *heap, CXIndex *outNode, double *outKey) {
	if (!heap || heap->size == 0) {
		return CXFalse;
	}
	CXIndex node = heap->nodes[0];
	double key = heap->keys[0];
	heap->size -= 1;
	if (heap->size > 0) {
		CXIndex tailNode = heap->nodes[heap->size];
		double tailKey = heap->keys[heap->size];
		CXSize index = 0;
		for (;;) {
			CXSize left = index * 2 + 1;
			CXSize right = left + 1;
			if (left >= heap->size) {
				break;
			}
			CXSize child = left;
			if (right < heap->size && heap->keys[right] < heap->keys[left]) {
				child = right;
			}
			if (heap->keys[child] >= tailKey) {
				break;
			}
			heap->nodes[index] = heap->nodes[child];
			heap->keys[index] = heap->keys[child];
			index = child;
		}
		heap->nodes[index] = tailNode;
		heap->keys[index] = tailKey;
	}
	if (outNode) {
		*outNode = node;
	}
	if (outKey) {
		*outKey = key;
	}
	return CXTrue;
}

static void CXMeasurementStrengthAccumulate(
	CXNetworkRef network,
	CXNeighborContainer *container,
	const CXMeasurementEdgeWeights *weights,
	double *inOutSum,
	double *inOutMin,
	double *inOutMax,
	CXSize *inOutCount
) {
	if (!network || !container || !weights || !inOutSum || !inOutMin || !inOutMax || !inOutCount) {
		return;
	}
	CXNeighborIterator iterator;
	CXNeighborIteratorInit(&iterator, container);
	while (CXNeighborIteratorNext(&iterator)) {
		CXIndex neighbor = iterator.node;
		if (neighbor >= network->nodeCapacity || !network->nodeActive[neighbor]) {
			continue;
		}
		double weight = weights->read(weights->base, weights->stride, iterator.edge);
		*inOutSum += weight;
		if (*inOutCount == 0 || weight < *inOutMin) {
			*inOutMin = weight;
		}
		if (*inOutCount == 0 || weight > *inOutMax) {
			*inOutMax = weight;
		}
		*inOutCount += 1;
	}
}

static float CXMeasurementStrengthReduce(double sum, double minValue, double maxValue, CXSize count, CXStrengthMeasure measure) {
	if (count == 0) {
		return 0.0f;
	}
	switch (measure) {
		case CXStrengthMeasureAverage:
			return (float)(sum / (double)count);
		case CXStrengthMeasureMaximum:
			return isfinite(maxValue) ? (float)maxValue : 0.0f;
		case CXStrengthMeasureMinimum:
			return isfinite(minValue) ? (float)minValue : 0.0f;
		case CXStrengthMeasureSum:
		default:
			return isfinite(sum) ? (float)sum : 0.0f;
	}
}

static double CXMeasurementDirectedEdgeWeight(
	CXNetworkRef network,
	CXIndex from,
	CXIndex to,
	const CXMeasurementEdgeWeights *weights,
	CXBool *outFound
) {
	if (outFound) {
		*outFound = CXFalse;
	}
	if (!network || !weights || from >= network->nodeCapacity || to >= network->nodeCapacity) {
		return 0.0;
	}
	if (!network->nodeActive[from] || !network->nodeActive[to]) {
		return 0.0;
	}

	double total = 0.0;
	CXBool found = CXFalse;
	CXNeighborIterator iterator;
	CXNeighborIteratorInit(&iterator, &network->nodes[from].outNeighbors);
	while (CXNeighborIteratorNext(&iterator)) {
		if (iterator.node != to) {
			continue;
		}
		total += weights->read(weights->base, weights->stride, iterator.edge);
		found = CXTrue;
	}
	if (outFound) {
		*outFound = found;
	}
	return total;
}

static double CXMeasurementUndirectedEdgeWeight(
	CXNetworkRef network,
	CXIndex a,
	CXIndex b,
	const CXMeasurementEdgeWeights *weights,
	CXBool *outFound
) {
	CXBool foundAB = CXFalse;
	CXBool foundBA = CXFalse;
	double ab = CXMeasurementDirectedEdgeWeight(network, a, b, weights, &foundAB);
	double ba = CXMeasurementDirectedEdgeWeight(network, b, a, weights, &foundBA);

	if (outFound) {
		*outFound = (foundAB || foundBA) ? CXTrue : CXFalse;
	}
	if (foundAB && foundBA) {
		return 0.5 * (ab + ba);
	}
	if (foundAB) {
		return ab;
	}
	if (foundBA) {
		return ba;
	}
	return 0.0;
}

static CXSize CXMeasurementCollectNeighbors(
	CXNetworkRef network,
	CXIndex node,
	CXNeighborDirection direction,
	uint32_t stamp,
	uint32_t *seen,
	CXIndex *neighbors
) {
	if (!network || !seen || !neighbors || node >= network->nodeCapacity || !network->nodeActive[node]) {
		return 0;
	}
	CXSize count = 0;
	CXNeighborIterator iterator;

	if (direction == CXNeighborDirectionOut || direction == CXNeighborDirectionBoth) {
		CXNeighborIteratorInit(&iterator, &network->nodes[node].outNeighbors);
		while (CXNeighborIteratorNext(&iterator)) {
			CXIndex neighbor = iterator.node;
			if (neighbor == node || neighbor >= network->nodeCapacity || !network->nodeActive[neighbor]) {
				continue;
			}
			if (seen[neighbor] == stamp) {
				continue;
			}
			seen[neighbor] = stamp;
			neighbors[count++] = neighbor;
		}
	}

	if (direction == CXNeighborDirectionIn || (direction == CXNeighborDirectionBoth && network->isDirected)) {
		CXNeighborIteratorInit(&iterator, &network->nodes[node].inNeighbors);
		while (CXNeighborIteratorNext(&iterator)) {
			CXIndex neighbor = iterator.node;
			if (neighbor == node || neighbor >= network->nodeCapacity || !network->nodeActive[neighbor]) {
				continue;
			}
			if (seen[neighbor] == stamp) {
				continue;
			}
			seen[neighbor] = stamp;
			neighbors[count++] = neighbor;
		}
	}

	return count;
}

typedef struct CXCorenessSession {
	CXNetworkRef network;
	CXNeighborDirection direction;
	CXMeasurementExecutionMode executionMode;
	CXMeasurementGraph graph;
	CXMeasurementMinHeap heap;
	uint32_t *nodeCoreness;
	uint32_t *degrees;
	uint8_t *removed;
	CXSize activeNodes;
	CXSize peeledNodes;
	CXSize workDone;
	CXSize workTotal;
	uint32_t currentCore;
	uint32_t maxCore;
	CXCorenessPhase phase;
} CXCorenessSession;

static CXBool CXCorenessSessionBuildInitialDegrees(CXCorenessSession *session) {
	if (!session) {
		return CXFalse;
	}
	const CXSize nodeCount = session->graph.nodeCount;
	if (nodeCount == 0) {
		return CXTrue;
	}

	const CXSize workerCount = CXMeasurementResolveWorkerCount(session->executionMode, nodeCount);
	if (workerCount <= 1) {
		for (CXIndex u = 0; u < nodeCount; u++) {
			uint32_t degree = 0;
			CXIndex outDegree = session->graph.outOffsets[u + 1] - session->graph.outOffsets[u];
			CXIndex inDegree = session->graph.inOffsets[u + 1] - session->graph.inOffsets[u];
			if (!session->graph.directed) {
				degree = (uint32_t)outDegree;
			} else if (session->direction == CXNeighborDirectionOut) {
				degree = (uint32_t)outDegree;
			} else if (session->direction == CXNeighborDirectionIn) {
				degree = (uint32_t)inDegree;
			} else {
				degree = (uint32_t)(outDegree + inDegree);
			}
			session->degrees[u] = degree;
		}
		return CXTrue;
	}

	const CXSize chunkSize = 1 + ((nodeCount - 1) / workerCount);
	CXParallelForStart(corenessDegreeInitLoop, workerIndex, workerCount) {
		const CXSize start = workerIndex * chunkSize;
		const CXSize end = CXMIN(nodeCount, (workerIndex + 1) * chunkSize);
		for (CXSize u = start; u < end; u++) {
			uint32_t degree = 0;
			CXIndex outDegree = session->graph.outOffsets[u + 1] - session->graph.outOffsets[u];
			CXIndex inDegree = session->graph.inOffsets[u + 1] - session->graph.inOffsets[u];
			if (!session->graph.directed) {
				degree = (uint32_t)outDegree;
			} else if (session->direction == CXNeighborDirectionOut) {
				degree = (uint32_t)outDegree;
			} else if (session->direction == CXNeighborDirectionIn) {
				degree = (uint32_t)inDegree;
			} else {
				degree = (uint32_t)(outDegree + inDegree);
			}
			session->degrees[u] = degree;
		}
	}
	CXParallelForEnd(corenessDegreeInitLoop);
	return CXTrue;
}

static CXBool CXCorenessSessionInitialize(CXCorenessSession *session) {
	if (!session) {
		return CXFalse;
	}
	if (!CXCorenessSessionBuildInitialDegrees(session)) {
		return CXFalse;
	}
	session->heap.size = 0;
	for (CXIndex u = 0; u < session->graph.nodeCount; u++) {
		if (!CXMeasurementMinHeapPush(&session->heap, u, (double)session->degrees[u])) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool CXCorenessSessionDecrementNeighbors(
	CXCorenessSession *session,
	const CXIndex *neighbors,
	CXIndex start,
	CXIndex end,
	uint32_t removedDegree
) {
	if (!session || !neighbors) {
		return CXFalse;
	}
	for (CXIndex idx = start; idx < end; idx++) {
		CXIndex v = neighbors[idx];
		if (v >= session->graph.nodeCount || session->removed[v]) {
			continue;
		}
		if (session->degrees[v] <= removedDegree) {
			continue;
		}
		session->degrees[v] -= 1;
		if (!CXMeasurementMinHeapPush(&session->heap, v, (double)session->degrees[v])) {
			return CXFalse;
		}
	}
	return CXTrue;
}

CXCorenessSessionRef CXCorenessSessionCreate(
	CXNetworkRef network,
	CXNeighborDirection direction,
	CXMeasurementExecutionMode executionMode
) {
	if (!network) {
		return NULL;
	}
	CXCorenessSession *session = (CXCorenessSession *)calloc(1, sizeof(CXCorenessSession));
	if (!session) {
		return NULL;
	}
	session->network = network;
	session->direction = CXMeasurementNormalizeDirection(network, direction);
	session->executionMode = CXMeasurementNormalizeExecutionMode(executionMode);

	CXMeasurementEdgeWeights weights = {0};
	weights.read = CXMeasurementWeightConstantOne;
	if (!CXMeasurementGraphBuild(&session->graph, network, &weights)) {
		free(session);
		return NULL;
	}
	session->activeNodes = session->graph.nodeCount;
	session->workTotal = session->activeNodes;
	if (network->nodeCapacity > 0) {
		session->nodeCoreness = (uint32_t *)calloc(network->nodeCapacity, sizeof(uint32_t));
	}
	if (network->nodeCapacity > 0 && !session->nodeCoreness) {
		CXMeasurementGraphDestroy(&session->graph);
		free(session);
		return NULL;
	}
	if (session->activeNodes > 0) {
		session->degrees = (uint32_t *)calloc(session->activeNodes, sizeof(uint32_t));
		session->removed = (uint8_t *)calloc(session->activeNodes, sizeof(uint8_t));
		if (!session->degrees || !session->removed || !CXMeasurementMinHeapInit(&session->heap, session->activeNodes)) {
			CXMeasurementMinHeapDestroy(&session->heap);
			CXMeasurementGraphDestroy(&session->graph);
			free(session->nodeCoreness);
			free(session->degrees);
			free(session->removed);
			free(session);
			return NULL;
		}
		session->phase = CXCorenessPhaseInitialize;
	} else {
		session->phase = CXCorenessPhaseDone;
	}
	return session;
}

void CXCorenessSessionDestroy(CXCorenessSessionRef sessionRef) {
	CXCorenessSession *session = (CXCorenessSession *)sessionRef;
	if (!session) {
		return;
	}
	free(session->nodeCoreness);
	free(session->degrees);
	free(session->removed);
	CXMeasurementMinHeapDestroy(&session->heap);
	CXMeasurementGraphDestroy(&session->graph);
	free(session);
}

CXCorenessPhase CXCorenessSessionStep(
	CXCorenessSessionRef sessionRef,
	CXSize budget
) {
	CXCorenessSession *session = (CXCorenessSession *)sessionRef;
	if (!session || !session->network) {
		return CXCorenessPhaseInvalid;
	}
	if (session->phase == CXCorenessPhaseDone
		|| session->phase == CXCorenessPhaseFailed
		|| session->phase == CXCorenessPhaseInvalid) {
		return session->phase;
	}
	if (budget == 0) {
		budget = 1;
	}

	while (budget > 0) {
		if (session->phase == CXCorenessPhaseInitialize) {
			if (!CXCorenessSessionInitialize(session)) {
				session->phase = CXCorenessPhaseFailed;
				break;
			}
			session->phase = CXCorenessPhasePeel;
			continue;
		}
		if (session->phase != CXCorenessPhasePeel) {
			break;
		}
		if (session->peeledNodes >= session->activeNodes) {
			session->phase = CXCorenessPhaseDone;
			break;
		}

		CXIndex compactNode = CXIndexMAX;
		double key = 0.0;
		if (!CXMeasurementMinHeapPop(&session->heap, &compactNode, &key)) {
			session->phase = (session->peeledNodes >= session->activeNodes)
				? CXCorenessPhaseDone
				: CXCorenessPhaseFailed;
			break;
		}
		if (compactNode >= session->graph.nodeCount || session->removed[compactNode]) {
			continue;
		}
		const uint32_t currentDegree = session->degrees[compactNode];
		if ((uint32_t)key != currentDegree) {
			continue;
		}

		session->removed[compactNode] = 1;
		session->peeledNodes += 1;
		session->workDone += 1;
		session->currentCore = currentDegree;
		if (currentDegree > session->maxCore) {
			session->maxCore = currentDegree;
		}
		CXIndex node = session->graph.compactToNode[compactNode];
		session->nodeCoreness[node] = currentDegree;

		CXBool ok = CXTrue;
		if (!session->graph.directed) {
			ok = CXCorenessSessionDecrementNeighbors(
				session,
				session->graph.outNeighbors,
				session->graph.outOffsets[compactNode],
				session->graph.outOffsets[compactNode + 1],
				currentDegree
			);
		} else if (session->direction == CXNeighborDirectionOut) {
			ok = CXCorenessSessionDecrementNeighbors(
				session,
				session->graph.inNeighbors,
				session->graph.inOffsets[compactNode],
				session->graph.inOffsets[compactNode + 1],
				currentDegree
			);
		} else if (session->direction == CXNeighborDirectionIn) {
			ok = CXCorenessSessionDecrementNeighbors(
				session,
				session->graph.outNeighbors,
				session->graph.outOffsets[compactNode],
				session->graph.outOffsets[compactNode + 1],
				currentDegree
			);
		} else {
			ok = CXCorenessSessionDecrementNeighbors(
				session,
				session->graph.outNeighbors,
				session->graph.outOffsets[compactNode],
				session->graph.outOffsets[compactNode + 1],
				currentDegree
			);
			if (ok) {
				ok = CXCorenessSessionDecrementNeighbors(
					session,
					session->graph.inNeighbors,
					session->graph.inOffsets[compactNode],
					session->graph.inOffsets[compactNode + 1],
					currentDegree
				);
			}
		}
		if (!ok) {
			session->phase = CXCorenessPhaseFailed;
			break;
		}

		budget -= 1;
	}
	return session->phase;
}

void CXCorenessSessionGetProgress(
	CXCorenessSessionRef sessionRef,
	double *outProgressCurrent,
	double *outProgressTotal,
	CXCorenessPhase *outPhase,
	CXSize *outPeeledNodes,
	CXSize *outActiveNodes,
	uint32_t *outCurrentCore,
	uint32_t *outMaxCore
) {
	CXCorenessSession *session = (CXCorenessSession *)sessionRef;
	if (!session) {
		if (outProgressCurrent) *outProgressCurrent = 0.0;
		if (outProgressTotal) *outProgressTotal = 0.0;
		if (outPhase) *outPhase = CXCorenessPhaseInvalid;
		if (outPeeledNodes) *outPeeledNodes = 0;
		if (outActiveNodes) *outActiveNodes = 0;
		if (outCurrentCore) *outCurrentCore = 0;
		if (outMaxCore) *outMaxCore = 0;
		return;
	}
	if (outProgressCurrent) *outProgressCurrent = (double)session->workDone;
	if (outProgressTotal) *outProgressTotal = (double)session->workTotal;
	if (outPhase) *outPhase = session->phase;
	if (outPeeledNodes) *outPeeledNodes = session->peeledNodes;
	if (outActiveNodes) *outActiveNodes = session->activeNodes;
	if (outCurrentCore) *outCurrentCore = session->currentCore;
	if (outMaxCore) *outMaxCore = session->maxCore;
}

CXBool CXCorenessSessionFinalize(
	CXCorenessSessionRef sessionRef,
	uint32_t *outNodeCoreness,
	CXSize outNodeCorenessCount,
	uint32_t *outMaxCore
) {
	CXCorenessSession *session = (CXCorenessSession *)sessionRef;
	if (!session || !session->network || !outNodeCoreness) {
		return CXFalse;
	}
	if (session->phase != CXCorenessPhaseDone) {
		return CXFalse;
	}
	if (outNodeCorenessCount < session->network->nodeCapacity) {
		return CXFalse;
	}
	if (session->network->nodeCapacity > 0) {
		memcpy(outNodeCoreness, session->nodeCoreness, session->network->nodeCapacity * sizeof(uint32_t));
	}
	if (outMaxCore) {
		*outMaxCore = session->maxCore;
	}
	return CXTrue;
}

CXBool CXNetworkMeasureCoreness(
	CXNetworkRef network,
	CXNeighborDirection direction,
	CXMeasurementExecutionMode executionMode,
	uint32_t *outNodeCoreness,
	uint32_t *outMaxCore
) {
	if (!network || !outNodeCoreness) {
		return CXFalse;
	}
	memset(outNodeCoreness, 0, network->nodeCapacity * sizeof(uint32_t));
	CXCorenessSessionRef session = CXCorenessSessionCreate(network, direction, executionMode);
	if (!session) {
		return CXFalse;
	}

	CXCorenessPhase phase = CXCorenessPhaseInvalid;
	do {
		phase = CXCorenessSessionStep(session, 4096);
	} while (phase == CXCorenessPhaseInitialize || phase == CXCorenessPhasePeel);

	CXBool ok = (phase == CXCorenessPhaseDone)
		&& CXCorenessSessionFinalize(
			session,
			outNodeCoreness,
			network->nodeCapacity,
			outMaxCore
		);
	CXCorenessSessionDestroy(session);
	if (!ok) {
		if (outMaxCore) {
			*outMaxCore = 0;
		}
		return CXFalse;
	}
	return CXTrue;
}

typedef struct CXConnectedComponentsSession {
	CXNetworkRef network;
	CXConnectedComponentsMode mode;
	CXMeasurementGraph graph;
	uint32_t *nodeComponent;
	uint8_t *forwardState;
	CXIndex *dfsNodeStack;
	CXIndex *dfsEdgeCursor;
	CXIndex *finishOrder;
	CXIndex *queue;
	CXSize activeNodes;
	CXSize visitedNodes;
	CXSize workDone;
	CXSize workTotal;
	CXIndex scanCursor;
	CXSize queueHead;
	CXSize queueTail;
	CXSize dfsDepth;
	CXSize finishCount;
	CXSize reverseCursor;
	uint32_t componentCount;
	uint32_t currentComponentId;
	uint32_t currentComponentSize;
	uint32_t largestComponentSize;
	CXConnectedComponentsPhase phase;
} CXConnectedComponentsSession;

static CXConnectedComponentsMode CXConnectedComponentsNormalizeMode(
	CXNetworkRef network,
	CXConnectedComponentsMode mode
) {
	if (mode != CXConnectedComponentsStrong) {
		return CXConnectedComponentsWeak;
	}
	if (!network || !network->isDirected) {
		return CXConnectedComponentsWeak;
	}
	return CXConnectedComponentsStrong;
}

static void CXConnectedComponentsSessionResetWeakTraversal(CXConnectedComponentsSession *session) {
	if (!session) {
		return;
	}
	session->queueHead = 0;
	session->queueTail = 0;
	session->currentComponentSize = 0;
	session->currentComponentId = 0;
}

static CXBool CXConnectedComponentsAssignWeakNode(
	CXConnectedComponentsSession *session,
	CXIndex compactNode
) {
	if (!session || compactNode >= session->graph.nodeCount) {
		return CXFalse;
	}
	CXIndex node = session->graph.compactToNode[compactNode];
	if (node >= session->network->nodeCapacity || session->nodeComponent[node] != 0) {
		return CXFalse;
	}
	session->nodeComponent[node] = session->currentComponentId;
	session->currentComponentSize += 1;
	session->visitedNodes += 1;
	session->workDone += 1;
	return CXTrue;
}

static void CXConnectedComponentsSessionStartWeakComponent(CXConnectedComponentsSession *session, CXIndex compactNode) {
	if (!session || compactNode >= session->graph.nodeCount) {
		return;
	}
	session->currentComponentId = session->componentCount + 1;
	session->currentComponentSize = 0;
	session->queueHead = 0;
	session->queueTail = 0;
	if (CXConnectedComponentsAssignWeakNode(session, compactNode)) {
		session->queue[session->queueTail++] = compactNode;
	}
	session->phase = CXConnectedComponentsPhaseTraverse;
}

static void CXConnectedComponentsSessionVisitWeakNeighbor(
	CXConnectedComponentsSession *session,
	CXIndex compactNeighbor
) {
	if (!session || compactNeighbor >= session->graph.nodeCount) {
		return;
	}
	if (session->queueTail < session->graph.nodeCount
		&& CXConnectedComponentsAssignWeakNode(session, compactNeighbor)) {
		session->queue[session->queueTail++] = compactNeighbor;
	}
}

static void CXConnectedComponentsSessionTraverseWeakNode(
	CXConnectedComponentsSession *session,
	CXIndex compactNode
) {
	if (!session || compactNode >= session->graph.nodeCount) {
		return;
	}
	for (CXIndex idx = session->graph.outOffsets[compactNode]; idx < session->graph.outOffsets[compactNode + 1]; idx++) {
		CXConnectedComponentsSessionVisitWeakNeighbor(session, session->graph.outNeighbors[idx]);
	}
	if (session->network->isDirected) {
		for (CXIndex idx = session->graph.inOffsets[compactNode]; idx < session->graph.inOffsets[compactNode + 1]; idx++) {
			CXConnectedComponentsSessionVisitWeakNeighbor(session, session->graph.inNeighbors[idx]);
		}
	}
}

static void CXConnectedComponentsSessionFinishWeakComponent(CXConnectedComponentsSession *session) {
	if (!session) {
		return;
	}
	if (session->currentComponentSize > session->largestComponentSize) {
		session->largestComponentSize = session->currentComponentSize;
	}
	session->componentCount += 1;
	CXConnectedComponentsSessionResetWeakTraversal(session);
	session->phase = CXConnectedComponentsPhaseScan;
}

static CXBool CXConnectedComponentsStrongPush(CXConnectedComponentsSession *session, CXIndex node) {
	if (!session || node >= session->graph.nodeCount || !session->forwardState) {
		return CXFalse;
	}
	if (session->dfsDepth >= session->graph.nodeCount) {
		return CXFalse;
	}
	session->forwardState[node] = 1;
	session->dfsNodeStack[session->dfsDepth] = node;
	session->dfsEdgeCursor[session->dfsDepth] = 0;
	session->dfsDepth += 1;
	session->workDone += 1;
	return CXTrue;
}

static CXBool CXConnectedComponentsStrongAssignNode(
	CXConnectedComponentsSession *session,
	CXIndex compactNode
) {
	if (!session || compactNode >= session->graph.nodeCount) {
		return CXFalse;
	}
	CXIndex node = session->graph.compactToNode[compactNode];
	if (node >= session->network->nodeCapacity || session->nodeComponent[node] != 0) {
		return CXFalse;
	}
	session->nodeComponent[node] = session->currentComponentId;
	session->currentComponentSize += 1;
	session->visitedNodes += 1;
	session->workDone += 1;
	return CXTrue;
}

static CXConnectedComponentsPhase CXConnectedComponentsSessionStepStrong(
	CXConnectedComponentsSession *session,
	CXSize budget
) {
	while (budget > 0) {
		if (session->phase == CXConnectedComponentsPhaseForward) {
			if (session->dfsDepth == 0) {
				while (session->scanCursor < session->graph.nodeCount
					&& session->forwardState[session->scanCursor] != 0) {
					session->scanCursor += 1;
				}
				if (session->scanCursor >= session->graph.nodeCount) {
					session->phase = CXConnectedComponentsPhaseReverse;
					session->dfsDepth = 0;
					session->reverseCursor = session->finishCount;
					continue;
				}
				CXIndex root = session->scanCursor++;
				if (!CXConnectedComponentsStrongPush(session, root)) {
					session->phase = CXConnectedComponentsPhaseFailed;
					return session->phase;
				}
				budget -= 1;
				continue;
			}

			CXSize top = session->dfsDepth - 1;
			CXIndex node = session->dfsNodeStack[top];
			CXIndex edgeCursor = session->dfsEdgeCursor[top];
			CXIndex start = session->graph.outOffsets[node];
			CXIndex end = session->graph.outOffsets[node + 1];
			CXBool pushed = CXFalse;
			while (start + edgeCursor < end) {
				CXIndex neighbor = session->graph.outNeighbors[start + edgeCursor];
				edgeCursor += 1;
				session->dfsEdgeCursor[top] = edgeCursor;
				if (session->forwardState[neighbor] != 0) {
					continue;
				}
				if (!CXConnectedComponentsStrongPush(session, neighbor)) {
					session->phase = CXConnectedComponentsPhaseFailed;
					return session->phase;
				}
				pushed = CXTrue;
				budget -= 1;
				break;
			}
			if (pushed) {
				continue;
			}
			session->forwardState[node] = 2;
			session->finishOrder[session->finishCount++] = node;
			session->dfsDepth -= 1;
			budget -= 1;
			continue;
		}

		if (session->phase == CXConnectedComponentsPhaseReverse) {
			if (session->dfsDepth == 0) {
				while (session->reverseCursor > 0) {
					CXIndex rootCompact = session->finishOrder[session->reverseCursor - 1];
					CXIndex rootNode = session->graph.compactToNode[rootCompact];
					if (session->nodeComponent[rootNode] == 0) {
						session->currentComponentId = session->componentCount + 1;
						session->currentComponentSize = 0;
						if (!CXConnectedComponentsStrongAssignNode(session, rootCompact)) {
							session->phase = CXConnectedComponentsPhaseFailed;
							return session->phase;
						}
						session->dfsNodeStack[0] = rootCompact;
						session->dfsEdgeCursor[0] = 0;
						session->dfsDepth = 1;
						session->reverseCursor -= 1;
						budget -= 1;
						break;
					}
					session->reverseCursor -= 1;
				}
				if (session->dfsDepth == 0) {
					session->phase = CXConnectedComponentsPhaseDone;
					break;
				}
				continue;
			}

			CXSize top = session->dfsDepth - 1;
			CXIndex node = session->dfsNodeStack[top];
			CXIndex edgeCursor = session->dfsEdgeCursor[top];
			CXIndex start = session->graph.inOffsets[node];
			CXIndex end = session->graph.inOffsets[node + 1];
			CXBool pushed = CXFalse;
			while (start + edgeCursor < end) {
				CXIndex neighbor = session->graph.inNeighbors[start + edgeCursor];
				edgeCursor += 1;
				session->dfsEdgeCursor[top] = edgeCursor;
				CXIndex neighborNode = session->graph.compactToNode[neighbor];
				if (session->nodeComponent[neighborNode] != 0) {
					continue;
				}
				if (!CXConnectedComponentsStrongAssignNode(session, neighbor)) {
					session->phase = CXConnectedComponentsPhaseFailed;
					return session->phase;
				}
				if (session->dfsDepth >= session->graph.nodeCount) {
					session->phase = CXConnectedComponentsPhaseFailed;
					return session->phase;
				}
				session->dfsNodeStack[session->dfsDepth] = neighbor;
				session->dfsEdgeCursor[session->dfsDepth] = 0;
				session->dfsDepth += 1;
				pushed = CXTrue;
				budget -= 1;
				break;
			}
			if (pushed) {
				continue;
			}
			session->dfsDepth -= 1;
			budget -= 1;
			if (session->dfsDepth == 0) {
				if (session->currentComponentSize > session->largestComponentSize) {
					session->largestComponentSize = session->currentComponentSize;
				}
				session->componentCount += 1;
				session->currentComponentSize = 0;
				session->currentComponentId = 0;
			}
			continue;
		}

		break;
	}
	return session->phase;
}

CXConnectedComponentsSessionRef CXConnectedComponentsSessionCreate(
	CXNetworkRef network,
	CXConnectedComponentsMode mode
) {
	if (!network) {
		return NULL;
	}
	CXConnectedComponentsSession *session = (CXConnectedComponentsSession *)calloc(1, sizeof(CXConnectedComponentsSession));
	if (!session) {
		return NULL;
	}
	session->network = network;
	session->mode = CXConnectedComponentsNormalizeMode(network, mode);

	CXMeasurementEdgeWeights weights = {0};
	weights.read = CXMeasurementWeightConstantOne;
	if (!CXMeasurementGraphBuild(&session->graph, network, &weights)) {
		free(session);
		return NULL;
	}
	session->activeNodes = session->graph.nodeCount;
	if (network->nodeCapacity > 0) {
		session->nodeComponent = (uint32_t *)calloc(network->nodeCapacity, sizeof(uint32_t));
	}
	if (network->nodeCapacity > 0 && !session->nodeComponent) {
		CXMeasurementGraphDestroy(&session->graph);
		free(session);
		return NULL;
	}

	if (session->activeNodes > 0) {
		session->queue = (CXIndex *)malloc(session->activeNodes * sizeof(CXIndex));
		if (!session->queue) {
			CXMeasurementGraphDestroy(&session->graph);
			free(session->nodeComponent);
			free(session);
			return NULL;
		}
	}

	if (session->mode == CXConnectedComponentsStrong && session->activeNodes > 0) {
		session->forwardState = (uint8_t *)calloc(session->activeNodes, sizeof(uint8_t));
		session->dfsNodeStack = (CXIndex *)malloc(session->activeNodes * sizeof(CXIndex));
		session->dfsEdgeCursor = (CXIndex *)malloc(session->activeNodes * sizeof(CXIndex));
		session->finishOrder = (CXIndex *)malloc(session->activeNodes * sizeof(CXIndex));
		if (!session->forwardState || !session->dfsNodeStack || !session->dfsEdgeCursor || !session->finishOrder) {
			CXMeasurementGraphDestroy(&session->graph);
			free(session->nodeComponent);
			free(session->queue);
			free(session->forwardState);
			free(session->dfsNodeStack);
			free(session->dfsEdgeCursor);
			free(session->finishOrder);
			free(session);
			return NULL;
		}
		session->workTotal = session->activeNodes * 2;
	} else {
		session->workTotal = session->activeNodes;
	}

	if (session->activeNodes == 0) {
		session->phase = CXConnectedComponentsPhaseDone;
	} else {
		session->phase = (session->mode == CXConnectedComponentsStrong)
			? CXConnectedComponentsPhaseForward
			: CXConnectedComponentsPhaseScan;
	}
	return session;
}

void CXConnectedComponentsSessionDestroy(CXConnectedComponentsSessionRef sessionRef) {
	CXConnectedComponentsSession *session = (CXConnectedComponentsSession *)sessionRef;
	if (!session) {
		return;
	}
	free(session->nodeComponent);
	free(session->forwardState);
	free(session->dfsNodeStack);
	free(session->dfsEdgeCursor);
	free(session->finishOrder);
	free(session->queue);
	CXMeasurementGraphDestroy(&session->graph);
	free(session);
}

CXConnectedComponentsPhase CXConnectedComponentsSessionStep(
	CXConnectedComponentsSessionRef sessionRef,
	CXSize budget
) {
	CXConnectedComponentsSession *session = (CXConnectedComponentsSession *)sessionRef;
	if (!session || !session->network) {
		return CXConnectedComponentsPhaseInvalid;
	}
	if (session->phase == CXConnectedComponentsPhaseDone
		|| session->phase == CXConnectedComponentsPhaseFailed
		|| session->phase == CXConnectedComponentsPhaseInvalid) {
		return session->phase;
	}
	if (budget == 0) {
		budget = 1;
	}
	if (session->mode == CXConnectedComponentsStrong) {
		return CXConnectedComponentsSessionStepStrong(session, budget);
	}

	while (budget > 0) {
		if (session->phase == CXConnectedComponentsPhaseScan) {
			while (session->scanCursor < session->graph.nodeCount) {
				CXIndex compactNode = session->scanCursor++;
				CXIndex node = session->graph.compactToNode[compactNode];
				if (session->nodeComponent[node] != 0) {
					continue;
				}
				CXConnectedComponentsSessionStartWeakComponent(session, compactNode);
				break;
			}
			if (session->phase != CXConnectedComponentsPhaseTraverse) {
				if (session->scanCursor >= session->graph.nodeCount) {
					session->phase = CXConnectedComponentsPhaseDone;
					break;
				}
				continue;
			}
		}

		if (session->phase == CXConnectedComponentsPhaseTraverse) {
			if (session->queueHead >= session->queueTail) {
				CXConnectedComponentsSessionFinishWeakComponent(session);
				continue;
			}
			CXIndex compactNode = session->queue[session->queueHead++];
			CXConnectedComponentsSessionTraverseWeakNode(session, compactNode);
			budget -= 1;
			continue;
		}
		break;
	}

	return session->phase;
}

void CXConnectedComponentsSessionGetProgress(
	CXConnectedComponentsSessionRef sessionRef,
	double *outProgressCurrent,
	double *outProgressTotal,
	CXConnectedComponentsPhase *outPhase,
	CXSize *outVisitedNodes,
	CXSize *outActiveNodes,
	uint32_t *outComponentCount,
	uint32_t *outLargestComponentSize
) {
	CXConnectedComponentsSession *session = (CXConnectedComponentsSession *)sessionRef;
	if (!session) {
		if (outProgressCurrent) *outProgressCurrent = 0.0;
		if (outProgressTotal) *outProgressTotal = 0.0;
		if (outPhase) *outPhase = CXConnectedComponentsPhaseInvalid;
		if (outVisitedNodes) *outVisitedNodes = 0;
		if (outActiveNodes) *outActiveNodes = 0;
		if (outComponentCount) *outComponentCount = 0;
		if (outLargestComponentSize) *outLargestComponentSize = 0;
		return;
	}
	if (outProgressCurrent) *outProgressCurrent = (double)session->workDone;
	if (outProgressTotal) *outProgressTotal = (double)session->workTotal;
	if (outPhase) *outPhase = session->phase;
	if (outVisitedNodes) *outVisitedNodes = session->visitedNodes;
	if (outActiveNodes) *outActiveNodes = session->activeNodes;
	if (outComponentCount) *outComponentCount = session->componentCount;
	if (outLargestComponentSize) {
		uint32_t largest = session->largestComponentSize;
		if ((session->phase == CXConnectedComponentsPhaseTraverse
			|| session->phase == CXConnectedComponentsPhaseReverse)
			&& session->currentComponentSize > largest) {
			largest = session->currentComponentSize;
		}
		*outLargestComponentSize = largest;
	}
}

CXBool CXConnectedComponentsSessionFinalize(
	CXConnectedComponentsSessionRef sessionRef,
	uint32_t *outNodeComponent,
	CXSize outNodeComponentCount,
	uint32_t *outComponentCount,
	uint32_t *outLargestComponentSize
) {
	CXConnectedComponentsSession *session = (CXConnectedComponentsSession *)sessionRef;
	if (!session || !session->network || !outNodeComponent) {
		return CXFalse;
	}
	if (session->phase != CXConnectedComponentsPhaseDone) {
		return CXFalse;
	}
	if (outNodeComponentCount < session->network->nodeCapacity) {
		return CXFalse;
	}
	if (session->network->nodeCapacity > 0) {
		memcpy(outNodeComponent, session->nodeComponent, session->network->nodeCapacity * sizeof(uint32_t));
	}
	if (outComponentCount) {
		*outComponentCount = session->componentCount;
	}
	if (outLargestComponentSize) {
		*outLargestComponentSize = session->largestComponentSize;
	}
	return CXTrue;
}

CXSize CXNetworkMeasureConnectedComponents(
	CXNetworkRef network,
	CXConnectedComponentsMode mode,
	uint32_t *outNodeComponent,
	uint32_t *outLargestComponentSize
) {
	if (!network || !outNodeComponent) {
		return 0;
	}
	memset(outNodeComponent, 0, network->nodeCapacity * sizeof(uint32_t));

	CXConnectedComponentsSessionRef session = CXConnectedComponentsSessionCreate(network, mode);
	if (!session) {
		return 0;
	}

	CXConnectedComponentsPhase phase = CXConnectedComponentsPhaseInvalid;
	do {
		phase = CXConnectedComponentsSessionStep(session, 4096);
	} while (phase == CXConnectedComponentsPhaseScan
		|| phase == CXConnectedComponentsPhaseTraverse
		|| phase == CXConnectedComponentsPhaseForward
		|| phase == CXConnectedComponentsPhaseReverse);

	uint32_t componentCount = 0;
	uint32_t largestComponentSize = 0;
	CXBool ok = (phase == CXConnectedComponentsPhaseDone)
		&& CXConnectedComponentsSessionFinalize(
			session,
			outNodeComponent,
			network->nodeCapacity,
			&componentCount,
			&largestComponentSize
		);
	CXConnectedComponentsSessionDestroy(session);
	if (!ok) {
		return 0;
	}
	if (outLargestComponentSize) {
		*outLargestComponentSize = largestComponentSize;
	}
	return (CXSize)componentCount;
}

static void CXMeasurementBetweennessSourceUnweighted(
	const CXMeasurementGraph *graph,
	CXIndex source,
	double *centrality,
	int32_t *dist,
	double *sigma,
	double *delta,
	CXIndex *queue,
	CXIndex *stack
) {
	const CXSize n = graph->nodeCount;
	for (CXSize i = 0; i < n; i++) {
		dist[i] = -1;
		sigma[i] = 0.0;
		delta[i] = 0.0;
	}

	CXSize qHead = 0;
	CXSize qTail = 0;
	CXSize stackCount = 0;
	dist[source] = 0;
	sigma[source] = 1.0;
	queue[qTail++] = source;

	while (qHead < qTail) {
		CXIndex v = queue[qHead++];
		stack[stackCount++] = v;
		for (CXIndex idx = graph->outOffsets[v]; idx < graph->outOffsets[v + 1]; idx++) {
			CXIndex w = graph->outNeighbors[idx];
			if (dist[w] < 0) {
				dist[w] = dist[v] + 1;
				queue[qTail++] = w;
			}
			if (dist[w] == dist[v] + 1) {
				sigma[w] += sigma[v];
			}
		}
	}

	for (CXSize pos = stackCount; pos > 0; pos--) {
		CXIndex w = stack[pos - 1];
		if (sigma[w] <= 0.0) {
			continue;
		}
		for (CXIndex idx = graph->inOffsets[w]; idx < graph->inOffsets[w + 1]; idx++) {
			CXIndex v = graph->inNeighbors[idx];
			if (dist[v] == dist[w] - 1 && sigma[v] > 0.0) {
				delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w]);
			}
		}
		if (w != source) {
			centrality[w] += delta[w];
		}
	}
}

static void CXMeasurementBetweennessSourceWeighted(
	const CXMeasurementGraph *graph,
	CXIndex source,
	double *centrality,
	double *dist,
	double *sigma,
	double *delta,
	CXBool *settled,
	CXIndex *stack,
	CXMeasurementMinHeap *heap
) {
	const CXSize n = graph->nodeCount;
	for (CXSize i = 0; i < n; i++) {
		dist[i] = DBL_MAX;
		sigma[i] = 0.0;
		delta[i] = 0.0;
		settled[i] = CXFalse;
	}
	heap->size = 0;

	dist[source] = 0.0;
	sigma[source] = 1.0;
	if (!CXMeasurementMinHeapPush(heap, source, 0.0)) {
		return;
	}

	CXSize stackCount = 0;
	CXIndex v = 0;
	double d = 0.0;
	while (CXMeasurementMinHeapPop(heap, &v, &d)) {
		if (d > dist[v] + CX_MEASUREMENT_WEIGHT_EPSILON) {
			continue;
		}
		if (settled[v]) {
			continue;
		}
		settled[v] = CXTrue;
		stack[stackCount++] = v;

		for (CXIndex idx = graph->outOffsets[v]; idx < graph->outOffsets[v + 1]; idx++) {
			CXIndex w = graph->outNeighbors[idx];
			double weight = graph->outWeights[idx];
			if (!isfinite(weight) || weight <= 0.0) {
				weight = CX_MEASUREMENT_WEIGHT_EPSILON;
			}
			double candidate = d + weight;
			if (candidate + CX_MEASUREMENT_WEIGHT_EPSILON < dist[w]) {
				dist[w] = candidate;
				sigma[w] = sigma[v];
				CXMeasurementMinHeapPush(heap, w, candidate);
			} else if (fabs(candidate - dist[w]) <= CX_MEASUREMENT_WEIGHT_EPSILON) {
				sigma[w] += sigma[v];
			}
		}
	}

	for (CXSize pos = stackCount; pos > 0; pos--) {
		CXIndex w = stack[pos - 1];
		if (sigma[w] <= 0.0) {
			continue;
		}
		for (CXIndex idx = graph->inOffsets[w]; idx < graph->inOffsets[w + 1]; idx++) {
			CXIndex pred = graph->inNeighbors[idx];
			double weight = graph->inWeights[idx];
			if (!isfinite(weight) || weight <= 0.0) {
				weight = CX_MEASUREMENT_WEIGHT_EPSILON;
			}
			if (!isfinite(dist[pred]) || !isfinite(dist[w])) {
				continue;
			}
			if (fabs((dist[pred] + weight) - dist[w]) <= CX_MEASUREMENT_WEIGHT_EPSILON && sigma[pred] > 0.0) {
				delta[pred] += (sigma[pred] / sigma[w]) * (1.0 + delta[w]);
			}
		}
		if (w != source) {
			centrality[w] += delta[w];
		}
	}
}

CXBool CXNetworkMeasureDegree(
	CXNetworkRef network,
	CXNeighborDirection direction,
	float *outNodeDegree
) {
	if (!network || !outNodeDegree) {
		return CXFalse;
	}
	direction = CXMeasurementNormalizeDirection(network, direction);
	memset(outNodeDegree, 0, network->nodeCapacity * sizeof(float));

	for (CXIndex node = 0; node < network->nodeCapacity; node++) {
		if (!network->nodeActive[node]) {
			continue;
		}
		CXSize outDegree = CXNeighborContainerCount(&network->nodes[node].outNeighbors);
		CXSize inDegree = CXNeighborContainerCount(&network->nodes[node].inNeighbors);
		double degree = 0.0;
		if (!network->isDirected) {
			degree = (double)outDegree;
		} else if (direction == CXNeighborDirectionOut) {
			degree = (double)outDegree;
		} else if (direction == CXNeighborDirectionIn) {
			degree = (double)inDegree;
		} else {
			degree = (double)(outDegree + inDegree);
		}
		outNodeDegree[node] = (float)degree;
	}

	return CXTrue;
}

CXBool CXNetworkMeasureStrength(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	CXNeighborDirection direction,
	CXStrengthMeasure measure,
	float *outNodeStrength
) {
	if (!network || !outNodeStrength) {
		return CXFalse;
	}
	direction = CXMeasurementNormalizeDirection(network, direction);
	measure = CXMeasurementNormalizeStrengthMeasure(measure);

	CXMeasurementEdgeWeights weights;
	if (!CXMeasurementResolveEdgeWeights(network, edgeWeightAttribute, &weights)) {
		return CXFalse;
	}

	memset(outNodeStrength, 0, network->nodeCapacity * sizeof(float));
	for (CXIndex node = 0; node < network->nodeCapacity; node++) {
		if (!network->nodeActive[node]) {
			continue;
		}
		double sum = 0.0;
		double minValue = DBL_MAX;
		double maxValue = -DBL_MAX;
		CXSize count = 0;

		if (!network->isDirected || direction == CXNeighborDirectionOut || direction == CXNeighborDirectionBoth) {
			CXMeasurementStrengthAccumulate(
				network,
				&network->nodes[node].outNeighbors,
				&weights,
				&sum,
				&minValue,
				&maxValue,
				&count
			);
		}
		if (network->isDirected && (direction == CXNeighborDirectionIn || direction == CXNeighborDirectionBoth)) {
			CXMeasurementStrengthAccumulate(
				network,
				&network->nodes[node].inNeighbors,
				&weights,
				&sum,
				&minValue,
				&maxValue,
				&count
			);
		}
		outNodeStrength[node] = CXMeasurementStrengthReduce(sum, minValue, maxValue, count, measure);
	}
	return CXTrue;
}

CXBool CXNetworkMeasureLocalClusteringCoefficient(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	CXNeighborDirection direction,
	CXClusteringCoefficientVariant variant,
	float *outNodeCoefficient
) {
	if (!network || !outNodeCoefficient) {
		return CXFalse;
	}
	direction = CXMeasurementNormalizeDirection(network, direction);
	variant = CXMeasurementNormalizeClusteringVariant(variant);

	CXMeasurementEdgeWeights weights;
	if (!CXMeasurementResolveEdgeWeights(network, edgeWeightAttribute, &weights)) {
		return CXFalse;
	}

	double maxWeight = 1.0;
	if (variant == CXClusteringCoefficientOnnela) {
		maxWeight = 0.0;
		for (CXIndex edge = 0; edge < network->edgeCapacity; edge++) {
			if (!network->edgeActive[edge]) {
				continue;
			}
			double w = fabs(weights.read(weights.base, weights.stride, edge));
			if (isfinite(w) && w > maxWeight) {
				maxWeight = w;
			}
		}
		if (!(maxWeight > 0.0)) {
			maxWeight = 1.0;
		}
	}

	memset(outNodeCoefficient, 0, network->nodeCapacity * sizeof(float));
	if (network->nodeCapacity == 0) {
		return CXTrue;
	}

	uint32_t *seen = (uint32_t *)calloc(network->nodeCapacity, sizeof(uint32_t));
	CXIndex *neighbors = (CXIndex *)malloc(network->nodeCapacity * sizeof(CXIndex));
	double *neighborWeights = (double *)malloc(network->nodeCapacity * sizeof(double));
	if (!seen || !neighbors || !neighborWeights) {
		free(seen);
		free(neighbors);
		free(neighborWeights);
		return CXFalse;
	}

	uint32_t stamp = 1;
	for (CXIndex node = 0; node < network->nodeCapacity; node++) {
		if (!network->nodeActive[node]) {
			continue;
		}
		if (++stamp == 0) {
			memset(seen, 0, network->nodeCapacity * sizeof(uint32_t));
			stamp = 1;
		}

		CXSize k = CXMeasurementCollectNeighbors(network, node, direction, stamp, seen, neighbors);
		if (k < 2) {
			outNodeCoefficient[node] = 0.0f;
			continue;
		}

		double nodeStrength = 0.0;
		for (CXSize i = 0; i < k; i++) {
			CXBool found = CXFalse;
			double wij = CXMeasurementUndirectedEdgeWeight(network, node, neighbors[i], &weights, &found);
			if (!found) {
				wij = 0.0;
			}
			neighborWeights[i] = wij;
			nodeStrength += wij;
		}

		double triangleCount = 0.0;
		double onnelaSum = 0.0;
		double newmanNumerator = 0.0;
		for (CXSize a = 0; a < k; a++) {
			for (CXSize b = a + 1; b < k; b++) {
				CXBool found = CXFalse;
				double wjk = CXMeasurementUndirectedEdgeWeight(network, neighbors[a], neighbors[b], &weights, &found);
				if (!found) {
					continue;
				}
				triangleCount += 1.0;
				if (variant == CXClusteringCoefficientOnnela) {
					double wa = fabs(neighborWeights[a]) / maxWeight;
					double wb = fabs(neighborWeights[b]) / maxWeight;
					double wc = fabs(wjk) / maxWeight;
					double term = cbrt(CXMAX(0.0, wa * wb * wc));
					onnelaSum += term;
				} else if (variant == CXClusteringCoefficientNewman) {
					newmanNumerator += 0.5 * (neighborWeights[a] + neighborWeights[b]);
				}
			}
		}

		double denomPairs = (double)k * (double)(k - 1);
		double coefficient = 0.0;
		if (variant == CXClusteringCoefficientUnweighted) {
			coefficient = denomPairs > 0.0 ? (2.0 * triangleCount) / denomPairs : 0.0;
		} else if (variant == CXClusteringCoefficientOnnela) {
			coefficient = denomPairs > 0.0 ? (2.0 * onnelaSum) / denomPairs : 0.0;
		} else {
			double denom = nodeStrength * (double)(k - 1);
			coefficient = denom > 0.0 ? (2.0 * newmanNumerator) / denom : 0.0;
		}
		outNodeCoefficient[node] = (float)(isfinite(coefficient) ? coefficient : 0.0);
	}

	free(seen);
	free(neighbors);
	free(neighborWeights);
	return CXTrue;
}

CXBool CXNetworkMeasureEigenvectorCentrality(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	CXNeighborDirection direction,
	CXMeasurementExecutionMode executionMode,
	CXSize maxIterations,
	double tolerance,
	const float *initialNodeCentrality,
	float *outNodeCentrality,
	double *outEigenvalue,
	double *outDelta,
	CXSize *outIterations,
	CXBool *outConverged
) {
	if (!network || !outNodeCentrality) {
		return CXFalse;
	}
	direction = CXMeasurementNormalizeDirection(network, direction);
	executionMode = CXMeasurementNormalizeExecutionMode(executionMode);
	if (maxIterations == 0) {
		maxIterations = 100;
	}
	if (!(tolerance > 0.0) || !isfinite(tolerance)) {
		tolerance = 1e-6;
	}

	memset(outNodeCentrality, 0, network->nodeCapacity * sizeof(float));

	CXMeasurementEdgeWeights weights;
	if (!CXMeasurementResolveEdgeWeights(network, edgeWeightAttribute, &weights)) {
		return CXFalse;
	}

	CXMeasurementGraph graph;
	if (!CXMeasurementGraphBuild(&graph, network, &weights)) {
		return CXFalse;
	}
	if (graph.nodeCount == 0) {
		CXMeasurementGraphDestroy(&graph);
		if (outEigenvalue) *outEigenvalue = 0.0;
		if (outDelta) *outDelta = 0.0;
		if (outIterations) *outIterations = 0;
		if (outConverged) *outConverged = CXTrue;
		return CXTrue;
	}

	double *x = (double *)calloc(graph.nodeCount, sizeof(double));
	double *y = (double *)calloc(graph.nodeCount, sizeof(double));
	if (!x || !y) {
		free(x);
		free(y);
		CXMeasurementGraphDestroy(&graph);
		return CXFalse;
	}

	double initNormSq = 0.0;
	if (initialNodeCentrality) {
		for (CXIndex u = 0; u < graph.nodeCount; u++) {
			CXIndex node = graph.compactToNode[u];
			double value = (double)initialNodeCentrality[node];
			if (!isfinite(value)) {
				value = 0.0;
			}
			x[u] = value;
			initNormSq += value * value;
		}
	}
	if (!(initNormSq > 0.0) || !isfinite(initNormSq)) {
		double uniform = 1.0 / sqrt((double)graph.nodeCount);
		for (CXIndex u = 0; u < graph.nodeCount; u++) {
			x[u] = uniform;
		}
	} else {
		double invNorm = 1.0 / sqrt(initNormSq);
		for (CXIndex u = 0; u < graph.nodeCount; u++) {
			x[u] *= invNorm;
		}
	}

	CXSize workerCount = CXMeasurementResolveWorkerCount(executionMode, graph.nodeCount);
	if (workerCount == 0) {
		workerCount = 1;
	}
	CXSize chunkSize = 1 + ((graph.nodeCount - 1) / workerCount);
	double *localNorm = (double *)calloc(workerCount, sizeof(double));
	double *localLambda = (double *)calloc(workerCount, sizeof(double));
	double *localDelta = (double *)calloc(workerCount, sizeof(double));
	if (!localNorm || !localLambda || !localDelta) {
		free(localNorm);
		free(localLambda);
		free(localDelta);
		free(x);
		free(y);
		CXMeasurementGraphDestroy(&graph);
		return CXFalse;
	}

	double eigenvalue = 0.0;
	double delta = 0.0;
	CXBool converged = CXFalse;
	CXSize iterations = 0;

	for (CXSize iter = 0; iter < maxIterations; iter++) {
		double normSq = 0.0;
		double lambdaNumerator = 0.0;
		if (workerCount == 1) {
			for (CXSize u = 0; u < graph.nodeCount; u++) {
				double sum = 0.0;
				if (direction == CXNeighborDirectionOut || direction == CXNeighborDirectionBoth || !graph.directed) {
					for (CXIndex idx = graph.outOffsets[u]; idx < graph.outOffsets[u + 1]; idx++) {
						CXIndex v = graph.outNeighbors[idx];
						sum += graph.outWeights[idx] * x[v];
					}
				}
				if (graph.directed && (direction == CXNeighborDirectionIn || direction == CXNeighborDirectionBoth)) {
					for (CXIndex idx = graph.inOffsets[u]; idx < graph.inOffsets[u + 1]; idx++) {
						CXIndex v = graph.inNeighbors[idx];
						sum += graph.inWeights[idx] * x[v];
					}
				}
				double adjusted = sum + CX_MEASUREMENT_EIGENVECTOR_SHIFT * x[u];
				y[u] = adjusted;
				normSq += adjusted * adjusted;
				lambdaNumerator += x[u] * sum;
			}
		} else {
			memset(localNorm, 0, workerCount * sizeof(double));
			memset(localLambda, 0, workerCount * sizeof(double));

			CXParallelForStart(eigenvectorMultiplyLoop, workerIndex, workerCount) {
				CXSize start = workerIndex * chunkSize;
				CXSize end = CXMIN(graph.nodeCount, (workerIndex + 1) * chunkSize);
				double normPart = 0.0;
				double lambdaPart = 0.0;

				for (CXSize u = start; u < end; u++) {
					double sum = 0.0;
					if (direction == CXNeighborDirectionOut || direction == CXNeighborDirectionBoth || !graph.directed) {
						for (CXIndex idx = graph.outOffsets[u]; idx < graph.outOffsets[u + 1]; idx++) {
							CXIndex v = graph.outNeighbors[idx];
							sum += graph.outWeights[idx] * x[v];
						}
					}
					if (graph.directed && (direction == CXNeighborDirectionIn || direction == CXNeighborDirectionBoth)) {
						for (CXIndex idx = graph.inOffsets[u]; idx < graph.inOffsets[u + 1]; idx++) {
							CXIndex v = graph.inNeighbors[idx];
							sum += graph.inWeights[idx] * x[v];
						}
					}
					double adjusted = sum + CX_MEASUREMENT_EIGENVECTOR_SHIFT * x[u];
					y[u] = adjusted;
					normPart += adjusted * adjusted;
					lambdaPart += x[u] * sum;
				}

				localNorm[workerIndex] = normPart;
				localLambda[workerIndex] = lambdaPart;
			}
			CXParallelForEnd(eigenvectorMultiplyLoop);

			for (CXSize w = 0; w < workerCount; w++) {
				normSq += localNorm[w];
				lambdaNumerator += localLambda[w];
			}
		}
		eigenvalue = lambdaNumerator;
		if (!(normSq > 0.0) || !isfinite(normSq)) {
			delta = 0.0;
			iterations = iter + 1;
			converged = CXFalse;
			break;
		}
		double invNorm = 1.0 / sqrt(normSq);

		if (workerCount == 1) {
			delta = 0.0;
			for (CXSize u = 0; u < graph.nodeCount; u++) {
				double normalized = y[u] * invNorm;
				double diff = fabs(normalized - x[u]);
				if (diff > delta) {
					delta = diff;
				}
				y[u] = normalized;
			}
		} else {
			memset(localDelta, 0, workerCount * sizeof(double));
			CXParallelForStart(eigenvectorNormalizeLoop, workerIndexNorm, workerCount) {
				CXSize start = workerIndexNorm * chunkSize;
				CXSize end = CXMIN(graph.nodeCount, (workerIndexNorm + 1) * chunkSize);
				double deltaPart = 0.0;
				for (CXSize u = start; u < end; u++) {
					double normalized = y[u] * invNorm;
					double diff = fabs(normalized - x[u]);
					if (diff > deltaPart) {
						deltaPart = diff;
					}
					y[u] = normalized;
				}
				localDelta[workerIndexNorm] = deltaPart;
			}
			CXParallelForEnd(eigenvectorNormalizeLoop);

			delta = 0.0;
			for (CXSize w = 0; w < workerCount; w++) {
				if (localDelta[w] > delta) {
					delta = localDelta[w];
				}
			}
		}
		double *tmp = x;
		x = y;
		y = tmp;
		iterations = iter + 1;
		if (delta <= tolerance) {
			converged = CXTrue;
			break;
		}
	}

	for (CXIndex u = 0; u < graph.nodeCount; u++) {
		CXIndex node = graph.compactToNode[u];
		outNodeCentrality[node] = (float)(isfinite(x[u]) ? x[u] : 0.0);
	}

	if (outEigenvalue) {
		*outEigenvalue = isfinite(eigenvalue) ? eigenvalue : 0.0;
	}
	if (outDelta) {
		*outDelta = isfinite(delta) ? delta : 0.0;
	}
	if (outIterations) {
		*outIterations = iterations;
	}
	if (outConverged) {
		*outConverged = converged;
	}

	free(localNorm);
	free(localLambda);
	free(localDelta);
	free(x);
	free(y);
	CXMeasurementGraphDestroy(&graph);
	return CXTrue;
}

CXSize CXNetworkMeasureBetweennessCentrality(
	CXNetworkRef network,
	const CXString edgeWeightAttribute,
	CXMeasurementExecutionMode executionMode,
	const CXIndex *sourceNodes,
	CXSize sourceCount,
	CXBool normalize,
	CXBool accumulate,
	float *inOutNodeBetweenness
) {
	if (!network || !inOutNodeBetweenness) {
		return 0;
	}

	executionMode = CXMeasurementNormalizeExecutionMode(executionMode);
	if (!accumulate) {
		memset(inOutNodeBetweenness, 0, network->nodeCapacity * sizeof(float));
	}

	CXMeasurementEdgeWeights weights;
	if (!CXMeasurementResolveEdgeWeights(network, edgeWeightAttribute, &weights)) {
		return 0;
	}
	CXBool weighted = (edgeWeightAttribute && edgeWeightAttribute[0]) ? CXTrue : CXFalse;

	CXMeasurementGraph graph;
	if (!CXMeasurementGraphBuild(&graph, network, &weights)) {
		return 0;
	}
	if (graph.nodeCount == 0) {
		CXMeasurementGraphDestroy(&graph);
		return 0;
	}

	CXIndex *sources = NULL;
	CXSize selectedCount = 0;
	if (sourceNodes && sourceCount > 0) {
		sources = (CXIndex *)malloc(sourceCount * sizeof(CXIndex));
		CXBool *used = (CXBool *)calloc(graph.nodeCount, sizeof(CXBool));
		if (!sources || !used) {
			free(sources);
			free(used);
			CXMeasurementGraphDestroy(&graph);
			return 0;
		}
		for (CXSize i = 0; i < sourceCount; i++) {
			CXIndex node = sourceNodes[i];
			if (node >= graph.nodeCapacity || !network->nodeActive[node]) {
				continue;
			}
			CXIndex compact = graph.nodeToCompact[node];
			if (compact == CXIndexMAX || used[compact]) {
				continue;
			}
			used[compact] = CXTrue;
			sources[selectedCount++] = compact;
		}
		free(used);
	} else {
		sources = (CXIndex *)malloc(graph.nodeCount * sizeof(CXIndex));
		if (!sources) {
			CXMeasurementGraphDestroy(&graph);
			return 0;
		}
		for (CXIndex u = 0; u < graph.nodeCount; u++) {
			sources[selectedCount++] = u;
		}
	}

	if (selectedCount == 0) {
		free(sources);
		CXMeasurementGraphDestroy(&graph);
		return 0;
	}

	double *centrality = (double *)calloc(graph.nodeCount, sizeof(double));
	double *contrib = (double *)calloc(graph.nodeCount, sizeof(double));
	if (!centrality || !contrib) {
		free(sources);
		free(centrality);
		free(contrib);
		CXMeasurementGraphDestroy(&graph);
		return 0;
	}
	if (accumulate) {
		for (CXIndex u = 0; u < graph.nodeCount; u++) {
			CXIndex node = graph.compactToNode[u];
			centrality[u] = (double)inOutNodeBetweenness[node];
		}
	}

	CXSize workerCount = CXMeasurementResolveWorkerCount(executionMode, selectedCount);
	if (workerCount == 0) {
		workerCount = 1;
	}
	CXSize chunkSize = 1 + ((selectedCount - 1) / workerCount);

	if (workerCount == 1) {
		int32_t *dist = (int32_t *)malloc(graph.nodeCount * sizeof(int32_t));
		double *distWeighted = (double *)malloc(graph.nodeCount * sizeof(double));
		double *sigma = (double *)malloc(graph.nodeCount * sizeof(double));
		double *delta = (double *)malloc(graph.nodeCount * sizeof(double));
		CXBool *settled = (CXBool *)malloc(graph.nodeCount * sizeof(CXBool));
		CXIndex *queue = (CXIndex *)malloc(graph.nodeCount * sizeof(CXIndex));
		CXIndex *stack = (CXIndex *)malloc(graph.nodeCount * sizeof(CXIndex));
		CXMeasurementMinHeap heap;
		CXBool heapOk = weighted ? CXMeasurementMinHeapInit(&heap, graph.nodeCount + 1) : CXTrue;
		if (!dist || !distWeighted || !sigma || !delta || !settled || !queue || !stack || !heapOk) {
			free(dist);
			free(distWeighted);
			free(sigma);
			free(delta);
			free(settled);
			free(queue);
			free(stack);
			if (weighted && heapOk) {
				CXMeasurementMinHeapDestroy(&heap);
			}
			free(centrality);
			free(contrib);
			free(sources);
			CXMeasurementGraphDestroy(&graph);
			return 0;
		}

		for (CXSize i = 0; i < selectedCount; i++) {
			CXIndex source = sources[i];
			if (weighted) {
				CXMeasurementBetweennessSourceWeighted(
					&graph,
					source,
					contrib,
					distWeighted,
					sigma,
					delta,
					settled,
					stack,
					&heap
				);
			} else {
				CXMeasurementBetweennessSourceUnweighted(
					&graph,
					source,
					contrib,
					dist,
					sigma,
					delta,
					queue,
					stack
				);
			}
		}

		free(dist);
		free(distWeighted);
		free(sigma);
		free(delta);
		free(settled);
		free(queue);
		free(stack);
		if (weighted) {
			CXMeasurementMinHeapDestroy(&heap);
		}
	} else {
		CXParallelForStart(betweennessParallelLoop, workerIndex, workerCount) {
			CXSize start = workerIndex * chunkSize;
			CXSize end = CXMIN(selectedCount, (workerIndex + 1) * chunkSize);
			if (start >= end) {
				continue;
			}

			double *local = (double *)calloc(graph.nodeCount, sizeof(double));
			int32_t *dist = (int32_t *)malloc(graph.nodeCount * sizeof(int32_t));
			double *distWeighted = (double *)malloc(graph.nodeCount * sizeof(double));
			double *sigma = (double *)malloc(graph.nodeCount * sizeof(double));
			double *delta = (double *)malloc(graph.nodeCount * sizeof(double));
			CXBool *settled = (CXBool *)malloc(graph.nodeCount * sizeof(CXBool));
			CXIndex *queue = (CXIndex *)malloc(graph.nodeCount * sizeof(CXIndex));
			CXIndex *stack = (CXIndex *)malloc(graph.nodeCount * sizeof(CXIndex));
			CXMeasurementMinHeap heap;
			CXBool heapOk = weighted ? CXMeasurementMinHeapInit(&heap, graph.nodeCount + 1) : CXTrue;
			if (!local || !dist || !distWeighted || !sigma || !delta || !settled || !queue || !stack || !heapOk) {
				free(local);
				free(dist);
				free(distWeighted);
				free(sigma);
				free(delta);
				free(settled);
				free(queue);
				free(stack);
				if (weighted && heapOk) {
					CXMeasurementMinHeapDestroy(&heap);
				}
				continue;
			}

			for (CXSize i = start; i < end; i++) {
				CXIndex source = sources[i];
				if (weighted) {
					CXMeasurementBetweennessSourceWeighted(
						&graph,
						source,
						local,
						distWeighted,
						sigma,
						delta,
						settled,
						stack,
						&heap
					);
				} else {
					CXMeasurementBetweennessSourceUnweighted(
						&graph,
						source,
						local,
						dist,
						sigma,
						delta,
						queue,
						stack
					);
				}
			}

			CXParallelLoopCriticalRegionStart(betweennessParallelLoop) {
				for (CXSize u = 0; u < graph.nodeCount; u++) {
					contrib[u] += local[u];
				}
			}
			CXParallelLoopCriticalRegionEnd(betweennessParallelLoop);

			free(local);
			free(dist);
			free(distWeighted);
			free(sigma);
			free(delta);
			free(settled);
			free(queue);
			free(stack);
			if (weighted) {
				CXMeasurementMinHeapDestroy(&heap);
			}
		}
		CXParallelForEnd(betweennessParallelLoop);
	}

	if (!graph.directed) {
		for (CXIndex u = 0; u < graph.nodeCount; u++) {
			contrib[u] *= 0.5;
		}
	}
	if (normalize && graph.nodeCount > 2) {
		double n = (double)graph.nodeCount;
		double denom = graph.directed ? ((n - 1.0) * (n - 2.0)) : (((n - 1.0) * (n - 2.0)) / 2.0);
		if (denom > 0.0) {
			double scale = 1.0 / denom;
			for (CXIndex u = 0; u < graph.nodeCount; u++) {
				contrib[u] *= scale;
			}
		}
	}

	for (CXIndex u = 0; u < graph.nodeCount; u++) {
		centrality[u] += contrib[u];
	}

	for (CXIndex u = 0; u < graph.nodeCount; u++) {
		CXIndex node = graph.compactToNode[u];
		inOutNodeBetweenness[node] = (float)(isfinite(centrality[u]) ? centrality[u] : 0.0);
	}

	free(centrality);
	free(contrib);
	free(sources);
	CXMeasurementGraphDestroy(&graph);
	return selectedCount;
}
