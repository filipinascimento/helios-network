//
//  CXNetwork.h
//  
//
//  Created by Filipi Nascimento Silva on 26/09/12.
//
//

#ifndef CXNetwork_CXNetwork_h
#define CXNetwork_CXNetwork_h

#include "CXCommons.h"
#include "CXBasicArrays.h"
#include "CXDictionary.h"

// typedef enum{
// 	CXStringAttributeType = 0, //"s"
// 	CXBooleanAttributeType = 1, //"b"
// 	CXFloatAttributeType = 2, //"f"
// 	CXIntegerAttributeType = 3, //"i"
// 	CXUnsignedIntegerAttributeType = 4, //"u"
// 	CXDoubleAttributeType = 5, //"d"
// 	CXDataAttributeType = 6, //"p" encodes a pointer to a data structure
// 	CXUnknownAttributeType = 255,
// } CXAttributeType;

// // create an attributeInfo struct with the following fields:
// // CXAttributeType type;
// // CXSize dimension;

// typedef struct{
// 	CXAttributeType type;
// 	CXSize dimension; // only for bool,float, integer, unsigned integer and double
// 	void** data;
// } CXAttributeInfo;




typedef enum{
	CXStringPropertyType = 0,
	CXNumberPropertyType = 1,
	CXVector2DPropertyType = 2,
	CXVector3DPropertyType = 3,
	CXUnknownPropertyType = 255,
} CXPropertyType;

typedef struct{
	CXSize* vertexNumOfEdges;
	CXSize* vertexCapacityOfEdges;
	CXIndex** vertexEdgesLists;
	CXIndex** vertexEdgesIndices;
	
	CXIndex* vertexNumOfInEdges;
	CXIndex* vertexCapacityOfInEdges;
	CXIndex** vertexInEdgesLists;
	CXIndex** vertexInEdgesIndices;
	
	CXIndex* edgeFromList;
	CXIndex* edgeToList;
	
	CXFloat* edgesWeights;
	CXFloat* verticesWeights;
	CXBool* verticesEnabled;
	
	CXSize vertexCapacity;
	CXSize edgesCapacity;
	
	CXSize edgesCount;
	CXSize verticesCount;
	
	CXBool editable;
	CXBool directed;
	CXBool edgeWeighted;
	CXBool vertexWeighted;
	
	CXString* vertexNames;
    
	CXString* propertiesNames;
	void** propertiesData;
	CXPropertyType* propertiesTypes;
	CXSize propertiesCount;

	// CXStringDictionaryRef networkAttributes;

	
} CXNetwork;

typedef CXNetwork* CXNetworkRef;

typedef struct{
	CXIndex vertex;
	CXIndex level;
	CXFloat weight;
	CXIndex branchIndex;
} CXNetworkAgent;

typedef struct{
	CXNetworkAgent* data;
	CXSize count;
	CXSize _capacity;
	CXBitArray visitedNodes;
} CXAgentPath;


typedef struct{
	CXUInteger from;
	CXUInteger to;
} CXEdge;



CXBool EMSCRIPTEN_KEEPALIVE CXNetworkAddNewEdges(CXNetworkRef network, CXIndex* fromIndices, CXIndex* toIndices, CXFloat* weights, CXSize count);
void EMSCRIPTEN_KEEPALIVE CXNetworkDestroy(CXNetworkRef network);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromXNETFile(FILE* networkFile);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetwork(CXSize verticesCount, CXBool edgeWeighted, CXBool directed);
void EMSCRIPTEN_KEEPALIVE CXNetworkWriteToFile(CXNetworkRef theNetwork, FILE* networkFile);
void EMSCRIPTEN_KEEPALIVE CXNetworkWriteToGMLFile(CXNetworkRef theNetwork, FILE* networkFile);
void EMSCRIPTEN_KEEPALIVE CXNetworkWriteToEdgesFile(CXNetworkRef theNetwork, FILE* networkFile);
void EMSCRIPTEN_KEEPALIVE CXNetworkWriteToPajekFile(CXNetworkRef theNetwork, FILE* networkFile);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkWithNetwork(CXNetworkRef originalNetwork, CXBool edgeWeighted, CXBool directed);

// CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewRegularNetwork(CXSize* gridSize, CXSize dimension, CXFloat connectionRadius, CXBool toroidal);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewRegular2DNetwork(CXSize rows, CXSize columns, CXBool toroidal);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewRandomNetwork(CXSize verticesCount, CXFloat degree);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewFastRandomNetwork(CXSize verticesCount, CXFloat degree);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewWaxmanNetwork(CXSize verticesCount,CXFloat alpha, CXFloat beta, CXSize dimension);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewRandomGeographicNetwork(CXSize verticesCount, CXFloat maximumDistance, CXSize dimension);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewRandomProbabilisticGeographicNetwork(CXSize verticesCount,CXFloat connectionProbability, CXFloat maximumDistance, CXSize dimension);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkHomogeneusModel(CXSize degree, CXSize verticesCount);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromRandomRemovingEdgeList(CXIndex* fromList,CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat rewireProbability);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromRandomRemoving(const CXNetworkRef originalNetwork, CXFloat removingProbability);


CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromRectangleRemovingEdgeList(CXIndex* fromList, CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat* positions, CXFloat minRectangleSize, CXFloat maxRectangleSize,CXSize rectangleCount,CXFloat removeProbability);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromRectangleRemoving(const CXNetworkRef originalNetwork,CXFloat minRectangleSize, CXFloat maxRectangleSize,CXSize rectangleCount,CXFloat removeProbability);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromRandomRewiring(const CXNetworkRef originalNetwork, CXFloat rewiringProbability);
CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromRandomRewiringEdgeList(CXIndex* fromList,CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat rewireProbability);
// CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewWattsStrogatzNetwork(CXSize approximateNumberOfVertices, CXSize dimension, CXFloat connectionRadius, CXBool toroidal, CXFloat rewiringProbability);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewBarabasiAlbertNetwork(CXSize initialSize, CXSize degreeGrowth, CXSize iterations);

CXNetworkRef* EMSCRIPTEN_KEEPALIVE CXNewBarabasiAlbertNetworkOverTime(CXSize initialSize, CXSize degreeGrowth, CXSize* iterationsArray, CXSize iterationsCount);

void EMSCRIPTEN_KEEPALIVE CXNetworkAppendProperty(CXNetworkRef theNetwork, CXString name, CXPropertyType type, void* data);

void* EMSCRIPTEN_KEEPALIVE CXNetworkPropertyWithName(const CXNetworkRef network, CXString name, CXPropertyType* outType);

void EMSCRIPTEN_KEEPALIVE CXNetworkGetDegree(const CXNetworkRef network, CXIntegerArray* degrees);

CXBool EMSCRIPTEN_KEEPALIVE CXNetworkCouldBeIsomorphic(const CXNetworkRef aNetwork,const CXNetworkRef bNetwork);

CXFloat EMSCRIPTEN_KEEPALIVE CXNetworkClusteringCoefficient(const CXNetworkRef aNetwork, CXIndex nodeIndex);

//private

CXNetworkRef CX_NewAllocationNetwork(CXSize verticesCount);


//Inline
CX_INLINE void CXNetworkGrowEdgesCount(CXNetworkRef network,CXSize growSize){
	network->edgesCount+=growSize;
	if(CXUnlikely(network->edgesCapacity < network->edgesCount)){
		network->edgesCapacity = 2 * network->edgesCount;
		network->edgeFromList = realloc(network->edgeFromList, sizeof(CXIndex)*network->edgesCapacity);
		network->edgeToList = realloc(network->edgeToList, sizeof(CXIndex)*network->edgesCapacity);
		if(network->edgeWeighted)
			network->edgesWeights = realloc(network->edgesWeights, sizeof(CXFloat)*network->edgesCapacity);
	}
}
CX_INLINE void CXNetworkGrowEdgesCapacity(CXNetworkRef network,CXSize capacityIncrease){
	CXSize newCapacity = network->edgesCount+capacityIncrease;
	if(CXUnlikely(network->edgesCapacity < newCapacity)){
		network->edgesCapacity = 2 * newCapacity;
		network->edgeFromList = realloc(network->edgeFromList, sizeof(CXIndex)*network->edgesCapacity);
		network->edgeToList = realloc(network->edgeToList, sizeof(CXIndex)*network->edgesCapacity);
		if(network->edgeWeighted)
			network->edgesWeights = realloc(network->edgesWeights, sizeof(CXFloat)*network->edgesCapacity);
	}
}
CX_INLINE void CXNetworkGrowVertexSetEdgeForVertex(CXNetworkRef network,CXIndex edgeIndex,CXIndex vertexIndex,CXIndex toVertexIndex){
	network->vertexNumOfEdges[vertexIndex]++;
	if(CXUnlikely(network->vertexCapacityOfEdges[vertexIndex] < network->vertexNumOfEdges[vertexIndex])){
		network->vertexCapacityOfEdges[vertexIndex]=CXCapacityGrow(network->vertexNumOfEdges[vertexIndex]);
		network->vertexEdgesLists[vertexIndex] = (CXIndex*) realloc(network->vertexEdgesLists[vertexIndex], sizeof(CXIndex)*network->vertexCapacityOfEdges[vertexIndex]);
		network->vertexEdgesIndices[vertexIndex] = (CXIndex*) realloc(network->vertexEdgesIndices[vertexIndex], sizeof(CXIndex)*network->vertexCapacityOfEdges[vertexIndex]);
	}
	network->vertexEdgesLists[vertexIndex][network->vertexNumOfEdges[vertexIndex]-1]=toVertexIndex;
	network->vertexEdgesIndices[vertexIndex][network->vertexNumOfEdges[vertexIndex]-1]=edgeIndex;
}

CX_INLINE void CXNetworkGrowVertexSetInEdgeForVertex(CXNetworkRef network,CXIndex edgeIndex,CXIndex vertexIndex,CXIndex toVertexIndex){
	network->vertexNumOfInEdges[vertexIndex]++;
	if(CXUnlikely(network->vertexCapacityOfInEdges[vertexIndex] < network->vertexNumOfInEdges[vertexIndex])){
		network->vertexCapacityOfInEdges[vertexIndex]=CXCapacityGrow(network->vertexNumOfInEdges[vertexIndex]);
		network->vertexInEdgesLists[vertexIndex] = (CXIndex*) realloc(network->vertexInEdgesLists[vertexIndex], sizeof(CXIndex)*network->vertexCapacityOfInEdges[vertexIndex]);
		network->vertexInEdgesIndices[vertexIndex] = (CXIndex*) realloc(network->vertexInEdgesIndices[vertexIndex], sizeof(CXIndex)*network->vertexCapacityOfInEdges[vertexIndex]);
	}
	network->vertexInEdgesLists[vertexIndex][network->vertexNumOfInEdges[vertexIndex]-1]=toVertexIndex;
	network->vertexInEdgesIndices[vertexIndex][network->vertexNumOfInEdges[vertexIndex]-1]=edgeIndex;
}



CX_INLINE CXSize CXNetworkCommonNeighborhood(const CXNetworkRef network,CXIndex vertex1,CXIndex vertex2){
	CXIndex v1n;
	CXSize commonNeighCount = 0;
	CXIndex* neigh1 = network->vertexEdgesLists[vertex1];
	CXIndex* neigh2 = network->vertexEdgesLists[vertex2];
	CXSize neigh1Count = network->vertexNumOfEdges[vertex1];
	CXSize neigh2Count = network->vertexNumOfEdges[vertex2];
	for (v1n=0; v1n<neigh1Count; v1n++) {
		CXIndex v2n;
		CXIndex neigh1Vertex = neigh1[v1n];
		for (v2n=0; v2n<neigh2Count; v2n++) {
			if(neigh1Vertex==neigh2[v2n]){
				commonNeighCount++;
			}
		}
	}
	return commonNeighCount;
}


CX_INLINE CXBool CXNetworkAreAdjacent(const CXNetworkRef network,CXIndex vertex1, CXIndex vertex2){
	CXIndex v1n;
	CXIndex* neigh1 = network->vertexEdgesLists[vertex1];
	CXSize neigh1Count = network->vertexNumOfEdges[vertex1];
	for (v1n=0; v1n<neigh1Count; v1n++) {
		CXIndex neigh1Vertex = neigh1[v1n];
		if(CXUnlikely(neigh1Vertex==vertex2)){
			return CXTrue;
		}
	}
	return CXFalse;
}

CXBool EMSCRIPTEN_KEEPALIVE CXNetworkAddNewEdge(CXNetworkRef network, CXIndex fromIndex, CXIndex toIndex, CXFloat weight);
CXBool EMSCRIPTEN_KEEPALIVE CXNetworkAddNewEdgeAndIntegrateWeight(CXNetworkRef network, CXIndex fromIndex, CXIndex toIndex, CXFloat weight);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkRemovingChains(const CXNetworkRef originalNetwork);

CXSize EMSCRIPTEN_KEEPALIVE CXNetworkNumberOfConnectedComponents(const CXNetworkRef theNetwork, CXGenericArray* connectedComponents);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromLargestComponent(const CXNetworkRef theNetwork);

CXSize EMSCRIPTEN_KEEPALIVE CXNetworkLargestComponentSize(const CXNetworkRef theNetwork, CXSize* connectedComponentsCount);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewSubNetworkFromNetwork(const CXNetworkRef theNetwork, const CXUIntegerArray verticesIndices);

CXNetworkRef EMSCRIPTEN_KEEPALIVE CXNewNetworkFromAdjacencyMatrix(const CXBitArray adjacencyMatrix,CXSize verticesCount, CXBool directed);
CXBitArray EMSCRIPTEN_KEEPALIVE CXNewAdjacencyMatrixFromNetwork(const CXNetworkRef theNetwork);




// Access to the network
CXSize EMSCRIPTEN_KEEPALIVE CXNetworkVerticesCount(const CXNetworkRef theNetwork);
CXSize EMSCRIPTEN_KEEPALIVE CXNetworkEdgesCount(const CXNetworkRef theNetwork);

CXSize EMSCRIPTEN_KEEPALIVE CXNetworkVertexDegree(const CXNetworkRef theNetwork, CXIndex vertexIndex);
CXSize EMSCRIPTEN_KEEPALIVE CXNetworkVertexInDegree(const CXNetworkRef theNetwork, CXIndex vertexIndex);

CXSize EMSCRIPTEN_KEEPALIVE CXNetworkVertexNumberOfEdges(const CXNetworkRef theNetwork, CXIndex vertexIndex);
CXSize EMSCRIPTEN_KEEPALIVE CXNetworkVertexNumberOfInEdges(const CXNetworkRef theNetwork, CXIndex vertexIndex);

CXIndex EMSCRIPTEN_KEEPALIVE CXNetworkVertexEdgeAtIndex(const CXNetworkRef theNetwork, CXIndex vertexIndex, CXIndex vertexEdgeIndex);
CXIndex EMSCRIPTEN_KEEPALIVE CXNetworkVertexInEdgeAtIndex(const CXNetworkRef theNetwork, CXIndex vertexIndex, CXIndex vertexedgeIndex);



#endif

