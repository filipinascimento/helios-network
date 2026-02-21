#include "CXNetwork.h"

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
