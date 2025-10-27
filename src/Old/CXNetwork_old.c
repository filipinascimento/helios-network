//
//  CXNetwork.c
//
//
//  Created by Filipi Nascimento Silva on 9/22/12.
//
//
#include "CXNetwork.h"
#include "uthash.h"


CXBool CXNetworkAddNewEdges(CXNetworkRef network, CXIndex* fromIndices, CXIndex* toIndices, CXFloat* weights, CXSize count){
	CXIndex initialEdge = network->edgesCount;
	CXSize verticesCount = network->verticesCount;
	CXIndex i;

	CXNetworkGrowEdgesCount(network,count);

	for(i=0; i<count; i++){
		CXIndex toIndex   = toIndices[i];
		CXIndex fromIndex = fromIndices[i];
		if (toIndex >= verticesCount||fromIndex >= verticesCount) {
			return CXFalse;
		}
		network->edgeFromList[i+initialEdge]=fromIndex;
		network->edgeToList[i+initialEdge]=toIndex;

		CXNetworkGrowVertexSetEdgeForVertex(network,i+initialEdge,fromIndex,toIndex);

		if(network->edgeWeighted&&weights==NULL){
			network->edgesWeights[i+initialEdge]=1.0f;
		}else if(network->edgeWeighted){
			network->edgesWeights[i+initialEdge]=weights[i];
		}
		if(!network->directed){
			CXNetworkGrowVertexSetEdgeForVertex(network,i+initialEdge,toIndex,fromIndex);
		}else{
			CXNetworkGrowVertexSetInEdgeForVertex(network,i+initialEdge,toIndex,fromIndex);
		}
	}
	return CXTrue;
}



void CXNetworkPrint(const CXNetworkRef network){
	printf("Vertices:" "%" CXSizeScan "\n", network->verticesCount);
	printf("Edges: " "%" CXSizeScan "\n", network->edgesCount);
	CXIndex fromVertex;
	for (fromVertex=0; fromVertex<=network->verticesCount; fromVertex++) {
		CXIndex i;
		CXSize toVerticesCount = network->vertexNumOfEdges[fromVertex];
		CXIndex* toVertices = network->vertexEdgesLists[fromVertex];
		printf("%"CXIndexScan"\t:",fromVertex);
		for (i=0; i<toVerticesCount; i++) {
			CXIndex toVertex = toVertices[i];
			printf("\t" "%"CXIndexScan,toVertex);
		}
		printf("\n");
	}
}

CXNetworkRef CX_NewAllocationNetwork(CXSize verticesCount){
	CXNetworkRef newNetwork = malloc(sizeof(CXNetwork));
	newNetwork->vertexNumOfEdges = calloc(verticesCount, sizeof(CXSize));
	newNetwork->vertexCapacityOfEdges = calloc(verticesCount, sizeof(CXSize));
	newNetwork->vertexEdgesLists = calloc(verticesCount, sizeof(CXSize*));
	newNetwork->vertexEdgesIndices = calloc(verticesCount, sizeof(CXSize*));

	newNetwork->vertexNumOfInEdges = calloc(verticesCount, sizeof(CXSize));
	newNetwork->vertexCapacityOfInEdges = calloc(verticesCount, sizeof(CXSize));
	newNetwork->vertexInEdgesLists = calloc(verticesCount, sizeof(CXSize*));
	newNetwork->vertexInEdgesIndices = calloc(verticesCount, sizeof(CXSize*));
	newNetwork->verticesWeights = calloc(verticesCount, sizeof(CXFloat));
	newNetwork->verticesEnabled = calloc(verticesCount, sizeof(CXBool));
	CXIndex i;
	for(i=0;i<verticesCount;i++){
		newNetwork->verticesWeights[i] = 1.0f;
		newNetwork->verticesEnabled[i] = CXTrue;
	}

	newNetwork->edgeFromList = NULL;
	newNetwork->edgeToList = NULL;

	newNetwork->edgesWeights = NULL;

	newNetwork->vertexCapacity = verticesCount;
	newNetwork->edgesCapacity = 0;

	newNetwork->edgesCount = 0;
	newNetwork->verticesCount = verticesCount;

	newNetwork->editable = CXFalse;
	newNetwork->directed = CXFalse;
	newNetwork->edgeWeighted = CXFalse;
	newNetwork->vertexWeighted = CXFalse;

	newNetwork->vertexNames = NULL;
	newNetwork->propertiesData = NULL;
	newNetwork->propertiesTypes = NULL;
	newNetwork->propertiesCount = 0;
	newNetwork->propertiesNames = NULL;
	
	return newNetwork;
}



void CX_NetworkDestroyProperties(CXNetworkRef theNetwork){
	CXIndex i;
	for (i=0; i<theNetwork->propertiesCount; i++) {
		CXPropertyType type = theNetwork->propertiesTypes[i];
		
		if(type==CXStringPropertyType){
			CXIndex j;
			CXString* values = theNetwork->propertiesData[i];
			for (j=0; j<theNetwork->verticesCount; j++) {
				free(values[j]);
			}
		}
		free(theNetwork->propertiesData[i]);
		free(theNetwork->propertiesNames[i]);
	}
	free(theNetwork->propertiesData);
	free(theNetwork->propertiesNames);
	free(theNetwork->propertiesTypes);
	theNetwork->propertiesCount = 0;
	theNetwork->propertiesData= NULL;
	theNetwork->propertiesNames= NULL;
	theNetwork->propertiesTypes= NULL;
}

void CX_NetworkDestroyNames(CXNetworkRef theNetwork){
	CXIndex i;
	if(theNetwork->vertexNames){
		for (i=0; i<theNetwork->verticesCount; i++) {
			free(theNetwork->vertexNames[i]);
		}
		free(theNetwork->vertexNames);
		theNetwork->vertexNames = NULL;
	}
}

void CXNetworkAppendProperty(CXNetworkRef theNetwork, CXString name, CXPropertyType type, void* data){
	CXIndex newIndex = 0;
	CXBool isNameProperty = CXFalse;
	
	if(strcmp(name, "name")==0){
		if(type==CXStringPropertyType){
			isNameProperty = CXTrue;
		}else{
			return;
			//ERROR DO NOTHING
		}
	}
	
	CXIndex currentPropertyIndex;
	void* currentData = NULL;
	CXPropertyType currentType = CXUnknownPropertyType;
	for(currentPropertyIndex=0;currentPropertyIndex<theNetwork->propertiesCount;currentPropertyIndex++){
		CXPropertyType theType = theNetwork->propertiesTypes[currentPropertyIndex];
		if(strcmp(theNetwork->propertiesNames[currentPropertyIndex], name)==0){
			currentType = theType;
			currentData = theNetwork->propertiesData[currentPropertyIndex];
		}
	}
	CXBool replacingProperty = CXFalse;
	if(isNameProperty){
	}else if(currentPropertyIndex!=theNetwork->propertiesCount && currentType==type){
		newIndex = currentPropertyIndex;
		replacingProperty = CXTrue;
	}else if(currentPropertyIndex!=theNetwork->propertiesCount && currentType!=type){
		newIndex = currentPropertyIndex;
		free(theNetwork->propertiesData[newIndex]);
		replacingProperty = CXTrue;
	}else{
		theNetwork->propertiesCount++;
		newIndex = theNetwork->propertiesCount-1;
		theNetwork->propertiesData  = realloc(theNetwork->propertiesData, sizeof(void*)*theNetwork->propertiesCount);
		theNetwork->propertiesNames = realloc(theNetwork->propertiesNames, sizeof(CXString)*theNetwork->propertiesCount);
		theNetwork->propertiesTypes = realloc(theNetwork->propertiesTypes, sizeof(CXPropertyType)*theNetwork->propertiesCount);
	
		theNetwork->propertiesTypes[newIndex] =type;
		theNetwork->propertiesNames[newIndex] = calloc(strlen(name)+1, sizeof(CXChar));
		strncpy(theNetwork->propertiesNames[newIndex], name, strlen(name));
	}
	
	
	switch (type) {
		case CXStringPropertyType:{
			CXString* values = calloc(theNetwork->verticesCount, sizeof(CXString));
			CXString* stringValues = data;
			CXIndex i;
			for(i=0; i<theNetwork->verticesCount; i++) {
				values[i] = calloc(strlen(stringValues[i])+1, sizeof(CXChar));
				strncpy(values[i], stringValues[i], strlen(stringValues[i]));
			}
			if(isNameProperty) {
				CX_NetworkDestroyNames(theNetwork);
				theNetwork->vertexNames = values;
			}else{
				if(replacingProperty){
					CXIndex j;
					for (j=0; j<theNetwork->verticesCount; j++) {
						free(((CXString*)theNetwork->propertiesData[newIndex])[j]);
					}
					free(theNetwork->propertiesData[newIndex]);
				}
				theNetwork->propertiesData[newIndex]=values;
			}
			break;
		}
		case CXNumberPropertyType:{
			if(replacingProperty || currentType!=type){
				theNetwork->propertiesData[newIndex] = calloc(theNetwork->verticesCount, sizeof(CXFloat));
			}
			memcpy(theNetwork->propertiesData[newIndex], data, sizeof(CXFloat)*theNetwork->verticesCount);
			break;
		}
		case CXVector2DPropertyType:{
			if(replacingProperty || currentType!=type){
				theNetwork->propertiesData[newIndex] = calloc(theNetwork->verticesCount*2, sizeof(CXFloat));
			}
			memcpy(theNetwork->propertiesData[newIndex], data, sizeof(CXFloat)*theNetwork->verticesCount*2);
			break;
		}
		case CXVector3DPropertyType:{
			if(replacingProperty || currentType!=type){
				theNetwork->propertiesData[newIndex] = calloc(theNetwork->verticesCount*3, sizeof(CXFloat));
			}
			memcpy(theNetwork->propertiesData[newIndex], data, sizeof(CXFloat)*theNetwork->verticesCount*3);
			break;
		}
		default:
			break;
	}
}

void* CXNetworkPropertyWithName(const CXNetworkRef network, CXString name, CXPropertyType* outType){
	CXIndex propertyIndex;
	void* data = NULL;
	for(propertyIndex=0;propertyIndex<network->propertiesCount;propertyIndex++){
		CXPropertyType type = network->propertiesTypes[propertyIndex];
		
		if(strcmp(network->propertiesNames[propertyIndex], name)==0){
			if(outType){
				*outType = type;
			}
			data = network->propertiesData[propertyIndex];
		}
	}
	return data;
}



/*
 if((token = strsep(&lineSegment, " ")) != NULL){
 }
 */

CXNetworkRef CXNewNetwork(CXSize verticesCount, CXBool edgeWeighted, CXBool directed){
	CXNetwork * theNetwork = NULL;
	theNetwork = CX_NewAllocationNetwork(verticesCount);
	theNetwork->vertexWeighted = CXFalse;
	theNetwork->edgeWeighted = edgeWeighted;
	theNetwork->directed = directed;
	return theNetwork;
}

CXNetworkRef CXNewNetworkWithNetwork(CXNetworkRef originalNetwork, CXBool edgeWeighted, CXBool directed){
	CXNetwork * theNetwork = NULL;
	theNetwork = CX_NewAllocationNetwork(originalNetwork->verticesCount);
	theNetwork->vertexWeighted = CXFalse;
	theNetwork->edgeWeighted = edgeWeighted;
	theNetwork->directed = directed;
	CXIndex i;
	for(i=0;i<originalNetwork->edgesCount;i++){
		CXIndex from,to;
		from = originalNetwork->edgeFromList[i];
		to = originalNetwork->edgeToList[i];
		CXFloat weight = 1.0f;
		if(originalNetwork->edgeWeighted){
			weight = originalNetwork->edgesWeights[i];
		}
		CXNetworkAddNewEdge(theNetwork, from, to, weight);
	}
	CXIndex propertyIndex;
	for(propertyIndex=0;propertyIndex<originalNetwork->propertiesCount;propertyIndex++){
		CXPropertyType type = originalNetwork->propertiesTypes[propertyIndex];
		void* data = originalNetwork->propertiesData[propertyIndex];
		CXString name = originalNetwork->propertiesNames[propertyIndex];
		CXNetworkAppendProperty(theNetwork, name, type, data);
	}
	
	return theNetwork;
}

void CXNetworkWriteToFile(CXNetworkRef theNetwork, FILE* networkFile){
	fprintf(networkFile,"#vertices ""%"CXSizeScan" nonweighted\n",theNetwork->verticesCount);
	if(theNetwork->vertexNames){
		CXIndex i;
		for(i=0;i<theNetwork->verticesCount;i++){
			fprintf(networkFile,"\"%s\"\n",theNetwork->vertexNames[i]);
		}
	}
	fprintf(networkFile,"#edges %s %s\n",theNetwork->edgeWeighted?"weighted":"nonweighted",theNetwork->directed?"directed":"undirected");
	CXIndex edgeIndex;
	
	CXIndex* edgesFrom = theNetwork->edgeFromList;
	CXIndex* edgesTo = theNetwork->edgeToList;
	CXFloat* edgesWeights = theNetwork->edgesWeights;
	for (edgeIndex=0; edgeIndex<theNetwork->edgesCount; edgeIndex++) {
		CXIndex fromVertex = edgesFrom[edgeIndex];
		CXIndex toVertex = edgesTo[edgeIndex];
		if(theNetwork->edgeWeighted){
			CXFloat weight = edgesWeights[edgeIndex];
			fprintf(networkFile,"%"CXIndexScan" ""%"CXIndexScan" ""%"CXFloatScan"\n",fromVertex,toVertex,weight);
		}else{
			fprintf(networkFile,"%"CXIndexScan" ""%"CXIndexScan"\n",fromVertex,toVertex);
		}
	}
	CXIndex propertyIndex;
	for(propertyIndex=0;propertyIndex<theNetwork->propertiesCount;propertyIndex++){
		CXPropertyType type = theNetwork->propertiesTypes[propertyIndex];
		void* data = theNetwork->propertiesData[propertyIndex];
		CXString name = theNetwork->propertiesNames[propertyIndex];
		switch (type) {
			case CXNumberPropertyType:{
				CXFloat* floatData = data;
				fprintf(networkFile,"#v \"%s\" n\n",name);
				CXIndex i;
				for(i=0;i<theNetwork->verticesCount;i++){
					fprintf(networkFile,"%"CXFloatScan"\n",floatData[i]);
				}
				break;
			}
			case CXStringPropertyType:{
				CXString* stringData = data;
				fprintf(networkFile,"#v \"%s\" s\n",name);
				CXIndex i;
				for(i=0;i<theNetwork->verticesCount;i++){
					fprintf(networkFile,"\"%s\"\n",stringData[i]);
				}
				break;
			}
			case CXVector2DPropertyType:{
				CXFloat* floatData = data;
				fprintf(networkFile,"#v \"%s\" v2\n",name);
				CXIndex i;
				for(i=0;i<theNetwork->verticesCount;i++){
					fprintf(networkFile,"%"CXFloatScan" %"CXFloatScan"\n",floatData[i*2],floatData[i*2+1]);
				}
				break;
			}
			case CXVector3DPropertyType:{
				CXFloat* floatData = data;
				fprintf(networkFile,"#v \"%s\" v3\n",name);
				CXIndex i;
				for(i=0;i<theNetwork->verticesCount;i++){
					fprintf(networkFile,"%"CXFloatScan" %"CXFloatScan" %"CXFloatScan"\n",floatData[i*3],floatData[i*3+1],floatData[i*3+2]);
				}
				break;
			}
			default:{
			}
		}
	}
}



void CXNetworkWriteToPajekFile(CXNetworkRef theNetwork, FILE* networkFile){
	fprintf(networkFile,"*vertices ""%"CXSizeScan"\n",theNetwork->verticesCount);
	if(theNetwork->vertexNames){
		CXIndex i;
		for(i=0;i<theNetwork->verticesCount;i++){
			fprintf(networkFile,"%"CXIndexScan" \"%s\"\n",i+1,theNetwork->vertexNames[i]);
		}
	}
	if(theNetwork->directed){
		fprintf(networkFile,"*arcs\n");
	}else{
		fprintf(networkFile,"*edges\n");
	}
	CXIndex edgeIndex;
	
	CXIndex* edgesFrom = theNetwork->edgeFromList;
	CXIndex* edgesTo = theNetwork->edgeToList;
	CXFloat* edgesWeights = theNetwork->edgesWeights;
	for (edgeIndex=0; edgeIndex<theNetwork->edgesCount; edgeIndex++) {
		CXIndex fromVertex = edgesFrom[edgeIndex];
		CXIndex toVertex = edgesTo[edgeIndex];
		if(theNetwork->edgeWeighted){
			CXFloat weight = edgesWeights[edgeIndex];
			fprintf(networkFile,"%"CXIndexScan" ""%"CXIndexScan" ""%"CXFloatScan"\n",fromVertex+1,toVertex+1,weight);
		}else{
			fprintf(networkFile,"%"CXIndexScan" ""%"CXIndexScan"\n",fromVertex+1,toVertex+1);
		}
	}
}


void CXNetworkWriteToEdgesFile(CXNetworkRef theNetwork, FILE* networkFile){
	CXIndex edgeIndex;
	
	CXIndex* edgesFrom = theNetwork->edgeFromList;
	CXIndex* edgesTo = theNetwork->edgeToList;
	CXFloat* edgesWeights = theNetwork->edgesWeights;
	for (edgeIndex=0; edgeIndex<theNetwork->edgesCount; edgeIndex++) {
		CXIndex fromVertex = edgesFrom[edgeIndex];
		CXIndex toVertex = edgesTo[edgeIndex];
		if(theNetwork->edgeWeighted){
			CXFloat weight = edgesWeights[edgeIndex];
			fprintf(networkFile,"%"CXIndexScan" ""%"CXIndexScan" ""%"CXFloatScan"\n",fromVertex,toVertex,weight);
		}else{
			fprintf(networkFile,"%"CXIndexScan" ""%"CXIndexScan"\n",fromVertex,toVertex);
		}
	}
}



CXNetworkRef CXNewNetworkFromXNETFile(FILE* networkFile){
	CXSize verticesCount = 0;

	CXBool isReadingVertices = CXFalse;
	CXBool isReadingEdges = CXFalse;
	CXBool isReadingProperty = CXFalse;

	CXNetwork * theNetwork = NULL;
	CXSize* fromIndices = NULL;
	CXSize* toIndices = NULL;
	CXFloat* edgesWeights = NULL;
	CXSize edgesCount = 0;
	CXSize edgesCapacity = 0;
	CXIndex currentVertex = 0;
	CXString propertyName = NULL;
	CXPropertyType propertyType = CXUnknownPropertyType;
	CXIndex propertyVertexIndex = 0;
	void* propertyData = NULL;
	CXString parsingError = NULL;

	CXIndex currentLine = 0;

	CXString lineBuffer;
	while((lineBuffer = CXNewStringReadingLine(networkFile))){
		CXString lineSegment = lineBuffer;
		currentLine++;
		if(lineSegment&&CXStringScanCharacters(&lineSegment, '#')){
			//printf("Reading Line: %s\n",lineSegment);
			if(CXStringScan(&lineSegment, "vertices")){
				CXStringScanCharacters(&lineSegment, ' ');
				if(CXStringScanIndex(&lineSegment, &verticesCount)){
					CXStringScanCharacters(&lineSegment, ' ');
					//printf("VerticesCount: %ld\n", verticesCount);
					theNetwork = CX_NewAllocationNetwork(verticesCount);
				}
				if(CXStringScan(&lineSegment, "weighted")){
					theNetwork->vertexWeighted = CXTrue;
				}
				isReadingVertices=CXTrue;
				isReadingEdges=CXFalse;
				isReadingProperty = CXFalse;
				currentVertex = 0;
			}else if(CXStringScan(&lineSegment, "edges")){
				CXStringScanCharacters(&lineSegment, ' ');
				if(CXStringScan(&lineSegment, "weighted")){
					theNetwork->edgeWeighted = CXTrue;
				}
				CXStringScanCharacters(&lineSegment, ' ');
				if(CXStringScan(&lineSegment, "directed")){
					theNetwork->directed = CXTrue;
				}
				CXStringScanCharacters(&lineSegment, ' ');
				if(CXStringScan(&lineSegment, "weighted")){
					theNetwork->edgeWeighted = CXTrue;
				}
				isReadingVertices=CXFalse;
				isReadingEdges=CXTrue;
				isReadingProperty = CXFalse;
			}else if(CXStringScan(&lineSegment,"v")){
				CXStringScanCharacters(&lineSegment, ' ');
				CXStringScanCharacters(&lineSegment, '\"');
				free(propertyName);
				propertyName = CXNewStringScanningUpToCharacter(&lineSegment, '\"');
				propertyVertexIndex=0;
				CXStringScanCharacters(&lineSegment, '\"');
				CXStringScanCharacters(&lineSegment, ' ');
				if(CXStringScan(&lineSegment, "n")){
					propertyType=CXNumberPropertyType;
					isReadingProperty = CXTrue;
					propertyData = calloc(verticesCount,sizeof(CXFloat));
				}else if(CXStringScan(&lineSegment, "v2")){
					propertyType=CXVector2DPropertyType;
					isReadingProperty = CXTrue;
					propertyData = calloc(verticesCount*2,sizeof(CXFloat));
				}else if(CXStringScan(&lineSegment, "v3")){
					propertyType=CXVector3DPropertyType;
					isReadingProperty = CXTrue;
					propertyData = calloc(verticesCount*3,sizeof(CXFloat));
				}else if(CXStringScan(&lineSegment, "s")){
					propertyType=CXStringPropertyType;
					isReadingProperty = CXTrue;
					propertyData = calloc(verticesCount,sizeof(CXString));
				}else{
					isReadingProperty = CXFalse;
					//printf("Unnalocationg %s\n",propertyName);
					free(propertyName);
					parsingError = "Error reading header for property.";
					break;
				}
				isReadingVertices=CXFalse;
				isReadingEdges=CXFalse;
			}else{
				isReadingVertices=CXFalse;
				isReadingEdges=CXFalse;
				isReadingProperty = CXFalse;
			}
		}else{
			if(isReadingVertices){
				if(currentVertex<verticesCount){
					if(!theNetwork->vertexNames){
						theNetwork->vertexNames = calloc(verticesCount, sizeof(CXString));
					}
					CXStringTrim(lineSegment, "\"\n \t");
					CXSize lineLength = strlen(lineSegment);
					theNetwork->vertexNames[currentVertex] = calloc(lineLength+1, sizeof(CXChar));
					strncpy(theNetwork->vertexNames[currentVertex], lineSegment, lineLength);
					currentVertex++;
				}else{
					isReadingVertices=CXFalse;
				}
			}else if(isReadingEdges){
				unsigned long _longFromIndex = 0;
				unsigned long _longToIndex = 0;
				CXFloat _doubleWeight = 1.0;
				if(sscanf(lineSegment, "%ld %ld " "%"CXFloatScan,&_longFromIndex,&_longToIndex,&_doubleWeight)>=2){
					edgesCount++;
					if(CXUnlikely(edgesCapacity < edgesCount)){
						edgesCapacity = CXCapacityGrow(edgesCount);
						fromIndices = realloc(fromIndices, sizeof(CXIndex)*edgesCapacity);
						toIndices = realloc(toIndices, sizeof(CXIndex)*edgesCapacity);
						if(theNetwork->edgeWeighted){
							edgesWeights = realloc(edgesWeights, sizeof(CXFloat)*edgesCapacity);
						}
					}
					fromIndices[edgesCount-1]=_longFromIndex;
					toIndices[edgesCount-1]=_longToIndex;
					if(theNetwork->edgeWeighted){
						edgesWeights[edgesCount-1] = _doubleWeight;
					}
				}
			}else if(isReadingProperty){
				CXStringTrim(lineSegment, "\"\n \t");
				switch (propertyType) {
					case CXNumberPropertyType:{
						CXFloat currentValue = 0.0f;
						if(sscanf(lineSegment, "%"CXFloatScan,&currentValue)>0&&propertyVertexIndex<verticesCount){
							CXFloat* currentData = propertyData;
							currentData[propertyVertexIndex] = currentValue;
							propertyVertexIndex++;
							if(propertyVertexIndex==verticesCount){
								CXNetworkAppendProperty(theNetwork, propertyName, propertyType, currentData);
							}
						}
						break;
					}
					case CXStringPropertyType:{
						CXStringScanCharacters(&lineSegment, '\"');
						CXString currentString = CXNewStringScanningUpToCharacter(&lineSegment, '\"');
						
						CXString* currentData = propertyData;
						currentData[propertyVertexIndex] = currentString;
						propertyVertexIndex++;
						if(propertyVertexIndex==verticesCount){
							CXNetworkAppendProperty(theNetwork, propertyName, propertyType, currentData);
						}
						break;
					}
					case CXVector2DPropertyType:{
						CXFloat currentValue1 = 0.0f;
						CXFloat currentValue2 = 0.0f;
						if(sscanf(lineSegment, "%"CXFloatScan" %"CXFloatScan,&currentValue1,&currentValue2)>0&&propertyVertexIndex<verticesCount){
							CXFloat* currentData = propertyData;
							currentData[propertyVertexIndex*2] = currentValue1;
							currentData[propertyVertexIndex*2+1] = currentValue2;
							propertyVertexIndex++;
							if(propertyVertexIndex==verticesCount){
								CXNetworkAppendProperty(theNetwork, propertyName, propertyType, currentData);
							}
						}
						break;
					}
					case CXVector3DPropertyType:{
						CXFloat currentValue1 = 0.0f;
						CXFloat currentValue2 = 0.0f;
						CXFloat currentValue3 = 0.0f;
						if(sscanf(lineSegment, "%"CXFloatScan" %"CXFloatScan" %"CXFloatScan,&currentValue1,&currentValue2,&currentValue3)>0&&propertyVertexIndex<verticesCount){
							CXFloat* currentData = propertyData;
							currentData[propertyVertexIndex*3] = currentValue1;
							currentData[propertyVertexIndex*3+1] = currentValue2;
							currentData[propertyVertexIndex*3+2] = currentValue3;
							propertyVertexIndex++;
							if(propertyVertexIndex==verticesCount){
								CXNetworkAppendProperty(theNetwork, propertyName, propertyType, currentData);
							}
						}
						break;
					}
					default:
						break;
				}
			}

		}
		free(lineBuffer);
	}
	
	if(parsingError==NULL){
		if(theNetwork && theNetwork->verticesCount>0){
			CXNetworkAddNewEdges(theNetwork, fromIndices,toIndices,edgesWeights, edgesCount);
		}
	}else{
		fprintf(stderr, "Parsing error occurred[at line %"CXIndexScan"]: %s\n", currentLine, parsingError);
		CXNetworkDestroy(theNetwork);
		theNetwork = NULL;
	}
	free(fromIndices);
	free(toIndices);
	free(edgesWeights);

	return theNetwork;
}

void CXNetworkDestroy(CXNetworkRef network){
	CXIndex i;
	CX_NetworkDestroyProperties(network);
	CX_NetworkDestroyNames(network);
	
	
	
	
	for(i=0;i<network->verticesCount;i++){
		free(network->vertexEdgesLists[i]);
		free(network->vertexEdgesIndices[i]);

		//if(network->directed){
			free(network->vertexInEdgesLists[i]);
			free(network->vertexInEdgesIndices[i]);
			//}
	}

	free(network->vertexCapacityOfEdges);
	free(network->vertexNumOfEdges);
	free(network->vertexEdgesLists);
	free(network->vertexEdgesIndices);

	//if(network->directed){
		free(network->vertexNumOfInEdges);
		free(network->vertexInEdgesLists);
		free(network->vertexInEdgesIndices);
		free(network->vertexCapacityOfInEdges);
	//}

	free(network->edgeFromList);
	free(network->edgeToList);

	//if(network->edgeWeighted)
	free(network->edgesWeights);

	free(network->verticesWeights);
	free(network->verticesEnabled);
	
	free(network);
}

void CXNetworkGetDegree(const CXNetworkRef network, CXIntegerArray* degrees){
	CXSize verticesCount = network->verticesCount;
	CXIntegerArrayReallocToCapacity(verticesCount, degrees);
	CXIntegerArraySetCount(verticesCount, degrees);
	
	CXIndex vertexIndex;
	for (vertexIndex=0; vertexIndex<verticesCount; vertexIndex++) {
		degrees->data[vertexIndex] = network->vertexNumOfEdges[vertexIndex];
	}
}


CXNetworkRef CXNewRegular2DNetwork(CXSize rows, CXSize columns, CXBool toroidal){
	CXSize verticesCount = rows*columns;
	CXSize maximumEdgesCount = verticesCount*2;
	CXSize edgesCount = 0;
	CXIndex* fromList = calloc(maximumEdgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(maximumEdgesCount, sizeof(CXIndex));
	CXFloat* positions = calloc(verticesCount*3, sizeof(CXFloat));
	CXIndex i,j;
	for (i=0; i<rows; i++) {
		for (j=0; j<columns; j++) {
			positions[(i*columns+j)*3+0]=(i-rows*0.5f)*200.0f/CXMAX(rows, columns);
			positions[(i*columns+j)*3+1]=(j-columns*0.5f)*200.0f/CXMAX(rows, columns);
			positions[(i*columns+j)*3+2]=0.0f;
			if(toroidal){
				fromList[edgesCount] = i*columns+j;
				toList[edgesCount] =(i)*columns+((j+1)%columns);
				edgesCount++;
				
				fromList[edgesCount] = i*columns+j;
				toList[edgesCount] =((i+1)%rows)*columns+(j);
				edgesCount++;
			}else{
				if(j+1<columns){
					fromList[edgesCount] = i*columns+j;
					toList[edgesCount] =(i)*columns+(j+1);
					edgesCount++;
				}
				if(i+1<rows){
					fromList[edgesCount] = i*columns+j;
					toList[edgesCount] =(i+1)*columns+(j);
					edgesCount++;
				}
			}
		}
	}
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	CXNetworkAppendProperty(theNetwork, "Position", CXVector3DPropertyType, positions);
	free(positions);
	free(fromList);
	free(toList);
	return theNetwork;
}


CXNetworkRef CXNewRandomNetwork(CXSize verticesCount, CXFloat degree){
	CXSize averageEdgesCount = floorf(verticesCount*degree)+1;
	CXSize edgesCapacity = averageEdgesCount;
	CXIndex* fromList = calloc(edgesCapacity, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCapacity, sizeof(CXIndex));
	CXSize edgesCount = 0;
	
	double probability = degree/(double)verticesCount;
	
	CXIndex toIndex,fromIndex;
	for (fromIndex=0; fromIndex<verticesCount; fromIndex++) {
		for (toIndex=fromIndex+1; toIndex<verticesCount; toIndex++) {
			if (CXRandomFloat()<probability){
				if(edgesCapacity<edgesCount+1){
					edgesCapacity = CXCapacityGrow(edgesCount+1);
					fromList = realloc(fromList, sizeof(CXIndex)*edgesCapacity);
					toList = realloc(toList, sizeof(CXIndex)*edgesCapacity);
				}
				fromList[edgesCount] = fromIndex;
				toList[edgesCount] = toIndex;
				edgesCount++;
			}
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	free(fromList);
	free(toList);
	return theNetwork;
}

CXNetworkRef CXNewFastRandomNetwork(CXSize verticesCount, CXFloat degree){
	CXSize edgesCount = roundf(verticesCount*degree*0.5);
	CXIndex* fromList = calloc(edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCount, sizeof(CXIndex));
	
	CXIndex edgeIndex;
	for (edgeIndex=0; edgeIndex<edgesCount; edgeIndex++) {
		fromList[edgeIndex] = CXRandomInRange(0, verticesCount);
		toList[edgeIndex] = CXRandomInRange(0, verticesCount);
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	free(fromList);
	free(toList);
	return theNetwork;
}



CXNetworkRef CXNewWaxmanNetwork(CXSize verticesCount,CXFloat alpha, CXFloat beta, CXSize dimension){
	CXSize edgesCapacity = verticesCount*3;
	CXIndex* fromList = calloc(edgesCapacity, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCapacity, sizeof(CXIndex));
	CXSize edgesCount = 0;
	CXFloat* positions = calloc(verticesCount*dimension, sizeof(CXFloat));
	CXFloat* positions3D = calloc(verticesCount*3, sizeof(CXFloat));
	
	CXIndex vertexIndex;
	for (vertexIndex=0; vertexIndex<verticesCount; vertexIndex++) {
		CXIndex dimensionIndex;
		for (dimensionIndex=0; dimensionIndex<dimension; dimensionIndex++) {
			positions[dimension*vertexIndex+dimensionIndex] = CXRandomFloat();
			if(dimensionIndex<3){
				positions3D[3*vertexIndex+dimensionIndex] = (positions[dimension*vertexIndex+dimensionIndex] - 0.5)*200;
			}
		}
	}
	
	CXIndex toIndex,fromIndex;
	for (fromIndex=0; fromIndex<verticesCount; fromIndex++) {
		printf("Oe %"CXUIntegerScan"      \r",fromIndex);
		fflush(stdout);
		for (toIndex=fromIndex+1; toIndex<verticesCount; toIndex++) {
			double distanceSquared = 0.0;
			CXIndex dimensionIndex;
			for (dimensionIndex=0; dimensionIndex<dimension; dimensionIndex++) {
				double positionFrom = positions[dimension*fromIndex+dimensionIndex];
				double positionTo = positions[dimension*toIndex+dimensionIndex];
				distanceSquared += (positionFrom-positionTo)*(positionFrom-positionTo);
			}
			double probability = alpha*exp(-sqrt(distanceSquared)/(beta*sqrt(dimension)));
			if (CXRandomFloat()<probability){
				if(edgesCapacity<edgesCount+1){
					edgesCapacity = CXCapacityGrow(edgesCount+1);
					fromList = realloc(fromList, sizeof(CXIndex)*edgesCapacity);
					toList = realloc(toList, sizeof(CXIndex)*edgesCapacity);
				}
				fromList[edgesCount] = fromIndex;
				toList[edgesCount] = toIndex;
				edgesCount++;
			}
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	CXNetworkAppendProperty(theNetwork, "Position", CXVector3DPropertyType, positions3D);
	free(fromList);
	free(toList);
	free(positions);
	free(positions3D);
	return theNetwork;
}



CXNetworkRef CXNewRandomGeographicNetwork(CXSize verticesCount, CXFloat maximumDistance, CXSize dimension){
	CXSize edgesCapacity = verticesCount*3;
	CXIndex* fromList = calloc(edgesCapacity, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCapacity, sizeof(CXIndex));
	CXSize edgesCount = 0;
	CXFloat* positions = calloc(verticesCount*dimension, sizeof(CXFloat));
	CXFloat* positions3D = calloc(verticesCount*3, sizeof(CXFloat));
	
	CXIndex vertexIndex;
	for (vertexIndex=0; vertexIndex<verticesCount; vertexIndex++) {
		CXIndex dimensionIndex;
		for (dimensionIndex=0; dimensionIndex<dimension; dimensionIndex++) {
			positions[dimension*vertexIndex+dimensionIndex] = CXRandomFloat();
			if(dimensionIndex<3){
				positions3D[3*vertexIndex+dimensionIndex] = (positions[dimension*vertexIndex+dimensionIndex] - 0.5)*200;
			}
		}
	}
	
	CXIndex toIndex,fromIndex;
	for (fromIndex=0; fromIndex<verticesCount; fromIndex++) {
		for (toIndex=fromIndex+1; toIndex<verticesCount; toIndex++) {
			double distanceSquared = 0.0;
			CXIndex dimensionIndex;
			for (dimensionIndex=0; dimensionIndex<dimension; dimensionIndex++) {
				double positionFrom = positions[dimension*fromIndex+dimensionIndex];
				double positionTo = positions[dimension*toIndex+dimensionIndex];
				distanceSquared += (positionFrom-positionTo)*(positionFrom-positionTo);
			}
			if (sqrt(distanceSquared)<maximumDistance){
				if(edgesCapacity<edgesCount+1){
					edgesCapacity = CXCapacityGrow(edgesCount+1);
					fromList = realloc(fromList, sizeof(CXIndex)*edgesCapacity);
					toList = realloc(toList, sizeof(CXIndex)*edgesCapacity);
				}
				fromList[edgesCount] = fromIndex;
				toList[edgesCount] = toIndex;
				edgesCount++;
			}
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	CXNetworkAppendProperty(theNetwork, "Position", CXVector3DPropertyType, positions3D);
	free(fromList);
	free(toList);
	free(positions);
	free(positions3D);
	return theNetwork;
}



CXNetworkRef CXNewRandomProbabilisticGeographicNetwork(CXSize verticesCount,CXFloat connectionProbability, CXFloat maximumDistance, CXSize dimension){
	CXSize edgesCapacity = verticesCount*3;
	CXIndex* fromList = calloc(edgesCapacity, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCapacity, sizeof(CXIndex));
	CXSize edgesCount = 0;
	CXFloat* positions = calloc(verticesCount*dimension, sizeof(CXFloat));
	CXFloat* positions3D = calloc(verticesCount*3, sizeof(CXFloat));
	
	CXIndex vertexIndex;
	for (vertexIndex=0; vertexIndex<verticesCount; vertexIndex++) {
		CXIndex dimensionIndex;
		for (dimensionIndex=0; dimensionIndex<dimension; dimensionIndex++) {
			positions[dimension*vertexIndex+dimensionIndex] = CXRandomFloat();
			if(dimensionIndex<3){
				positions3D[3*vertexIndex+dimensionIndex] = (positions[dimension*vertexIndex+dimensionIndex] - 0.5)*200;
			}
		}
	}
	
	CXIndex toIndex,fromIndex;
	for (fromIndex=0; fromIndex<verticesCount; fromIndex++) {
		for (toIndex=fromIndex+1; toIndex<verticesCount; toIndex++) {
			double distanceSquared = 0.0;
			CXIndex dimensionIndex;
			for (dimensionIndex=0; dimensionIndex<dimension; dimensionIndex++) {
				double positionFrom = positions[dimension*fromIndex+dimensionIndex];
				double positionTo = positions[dimension*toIndex+dimensionIndex];
				distanceSquared += (positionFrom-positionTo)*(positionFrom-positionTo);
			}
			if (sqrt(distanceSquared)<maximumDistance && CXRandomFloat()<=connectionProbability){
				if(edgesCapacity<edgesCount+1){
					edgesCapacity = CXCapacityGrow(edgesCount+1);
					fromList = realloc(fromList, sizeof(CXIndex)*edgesCapacity);
					toList = realloc(toList, sizeof(CXIndex)*edgesCapacity);
				}
				fromList[edgesCount] = fromIndex;
				toList[edgesCount] = toIndex;
				edgesCount++;
			}
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	CXNetworkAppendProperty(theNetwork, "Position", CXVector3DPropertyType, positions3D);
	free(fromList);
	free(toList);
	free(positions);
	free(positions3D);
	return theNetwork;
}



CXNetworkRef CXNewNetworkFromRandomRewiringEdgeList(CXIndex* fromList,CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat rewireProbability){
	CXIndex edgeIndex = 0;
	struct __cv_edge{
		CXIndex from;
		CXIndex to;
	};
	struct __cv_edge_element{
		struct __cv_edge edge;
		UT_hash_handle hh;
	};
	
	struct __cv_edge_element* edgesHash = NULL;
	
	//	HASH_ADD_KEYPTR(hh, edgesHash, edgesHash->edge, sizeof(struct __cv_edge), edgesHash);
	
	for (edgeIndex=0; edgeIndex<edgesCount; edgeIndex++) {
		struct __cv_edge_element* edgeElement = calloc(1, sizeof(struct __cv_edge_element));
		if(!directed){
			edgeElement->edge.from = CXMIN(fromList[edgeIndex],toList[edgeIndex]);
			edgeElement->edge.to = CXMAX(fromList[edgeIndex],toList[edgeIndex]);
		}else{
			edgeElement->edge.from = fromList[edgeIndex];
			edgeElement->edge.to = toList[edgeIndex];
		}
		HASH_ADD_KEYPTR(hh, edgesHash, (&(edgeElement->edge)), sizeof(struct __cv_edge), edgeElement);
	}
	
	for (edgeIndex=0; edgeIndex<edgesCount; edgeIndex++) {
		if(CXRandomFloat()<rewireProbability){
			CXBool edgeExists;
			do{
				struct __cv_edge_element* edgeElement = NULL;
				struct __cv_edge edgesKey;
				edgesKey.from = CXRandomInRange(0, verticesCount);//fromList[edgeIndex];
				edgesKey.to = CXRandomInRange(0, verticesCount);
				fromList[edgeIndex] = edgesKey.from;
				toList[edgeIndex] = edgesKey.to;
				HASH_FIND(hh, edgesHash, &edgesKey, sizeof(struct __cv_edge), edgeElement);
				if(edgeElement || edgesKey.from==edgesKey.to){
					edgeExists=CXTrue;
				}else{
					edgeExists=CXFalse;
				}
			}while(edgeExists);
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, directed);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	
	struct __cv_edge_element* edgeElement, *tempElement;
	
	HASH_ITER(hh, edgesHash, edgeElement, tempElement) {
		HASH_DEL(edgesHash, edgeElement);
		free(edgeElement);
	}
	
	return theNetwork;
}



CXNetworkRef CXNewNetworkFromRandomRemovingEdgeList(CXIndex* fromList,CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat rewireProbability){
	CXIndex edgeIndex = 0;
	CXSize newEdgesCount = 0;
	
	for (edgeIndex=0; edgeIndex<edgesCount; edgeIndex++) {
		if(CXRandomFloat()>=rewireProbability){ //not remove
			fromList[newEdgesCount] = fromList[edgeIndex];
			toList[newEdgesCount] = toList[edgeIndex];
			newEdgesCount++;
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, directed);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, newEdgesCount);
	
	return theNetwork;
}

CXNetworkRef CXNewNetworkFromRandomRemoving(const CXNetworkRef originalNetwork, CXFloat removingProbability){
	CXIndex* fromList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	
	memcpy(fromList, originalNetwork->edgeFromList, sizeof(CXSize)*originalNetwork->edgesCount);
	memcpy(toList, originalNetwork->edgeToList, sizeof(CXSize)*originalNetwork->edgesCount);
	
	CXNetworkRef theNetwork = CXNewNetworkFromRandomRemovingEdgeList(fromList, toList, originalNetwork->edgesCount, originalNetwork->verticesCount, originalNetwork->directed,removingProbability);
	
	free(fromList);
	free(toList);
	return theNetwork;
}




CXNetworkRef CXNewNetworkFromRectangleRemovingEdgeList(CXIndex* fromList,CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat* positions,CXFloat minRectangleSize, CXFloat maxRectangleSize,CXSize rectangleCount,CXFloat removeProbability){
	CXIndex edgeIndex = 0;
	CXSize newEdgesCount = 0;
	
	CXIndex i;
	CXFloat positionXMax = CXFloatMIN;
	CXFloat positionXMin = CXFloatMAX;
	CXFloat positionYMax = CXFloatMIN;
	CXFloat positionYMin = CXFloatMAX;
	
	for (i=0; i<verticesCount; i++) {
		CXFloat x = positions[3*i];
		CXFloat y = positions[3*i+1];
		positionXMax = CXMAX(positionXMax, x);
		positionXMin = CXMIN(positionXMin, x);
		positionYMax = CXMAX(positionYMax, y);
		positionYMin = CXMIN(positionYMin, y);
	}
	CXFloat* rectangles = calloc(rectangleCount*4, sizeof(CXFloat));
	
	for (i=0; i<rectangleCount; i++) {
		CXFloat cx = positionXMin + CXRandomFloat()*(positionXMax-positionXMin);
		CXFloat cy = positionYMin + CXRandomFloat()*(positionYMax-positionYMin);
		
		CXFloat sizex = (minRectangleSize + (maxRectangleSize-minRectangleSize)*CXRandomFloat())*(positionXMax-positionXMin);
		CXFloat sizey = (minRectangleSize + (maxRectangleSize-minRectangleSize)*CXRandomFloat())*(positionYMax-positionYMin);
		
		rectangles[i*4 + 0] = cx;
		rectangles[i*4 + 1] = cy;
		rectangles[i*4 + 2] = sizex;
		rectangles[i*4 + 3] = sizey;
		printf("xy = (%f, %f)    rect = [%f, %f, %f, %f]\n",0.1,1.2,cx-sizex*0.5f,cy-sizey*0.5f,cx+sizex*0.5f,cy+sizey*0.5f);
	}
	
	double* probabilityVertices = calloc(verticesCount,sizeof(double));
	CXIndex j;
	for (j=0; j<verticesCount; j++) {
		probabilityVertices[j] = 1.0;
		CXFloat x = positions[3*j];
		CXFloat y = positions[3*j+1];
		for (i=0; i<rectangleCount; i++) {
			CXFloat cx = rectangles[i*4 + 0];
			CXFloat cy = rectangles[i*4 + 1];
			CXFloat sizex = rectangles[i*4 + 2];
			CXFloat sizey = rectangles[i*4 + 3];
			//printf("xy = (%f, %f)    rect = [%f, %f, %f, %f]\n",x,y,cx-sizex*0.5f,cy-sizey*0.5f,cx+sizex*0.5f,cy+sizey*0.5f);
			
			if(cx-sizex*0.5f <= x && x <= cx+sizex*0.5f && cy-sizey*0.5f <= y && y <= cy+sizey*0.5f){
				probabilityVertices[j] *= (1.0-removeProbability);
			}
		}
	}
	
	for (edgeIndex=0; edgeIndex<edgesCount; edgeIndex++) {
		double prob = (probabilityVertices[fromList[edgeIndex]] * probabilityVertices[toList[edgeIndex]]);
		if(CXRandomFloat() < prob){ //not remove
			fromList[newEdgesCount] = fromList[edgeIndex];
			toList[newEdgesCount] = toList[edgeIndex];
			newEdgesCount++;
		}
	}
	
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, directed);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, newEdgesCount);
	
	free(rectangles);
	free(probabilityVertices);
	return theNetwork;
}

CXNetworkRef CXNewNetworkFromRectangleRemoving(const CXNetworkRef originalNetwork,CXFloat minRectangleSize, CXFloat maxRectangleSize, CXSize rectangleCount,CXFloat removeProbability){
	CXNetworkRef theNetwork;
	CXFloat* positions = NULL;
	CXPropertyType positionType = CXUnknownPropertyType;
	positions = CXNetworkPropertyWithName(originalNetwork, "Position", &positionType);
	if(positions && positionType==CXVector3DPropertyType){
		CXIndex* fromList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
		CXIndex* toList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
		memcpy(fromList, originalNetwork->edgeFromList, sizeof(CXSize)*originalNetwork->edgesCount);
		memcpy(toList, originalNetwork->edgeToList, sizeof(CXSize)*originalNetwork->edgesCount);
		theNetwork = CXNewNetworkFromRectangleRemovingEdgeList(fromList, toList, originalNetwork->edgesCount, originalNetwork->verticesCount, originalNetwork->directed, positions,minRectangleSize,maxRectangleSize,rectangleCount, removeProbability);
		CXNetworkAppendProperty(theNetwork, "Position", CXVector3DPropertyType, positions);
		free(fromList);
		free(toList);
	}else{
		theNetwork = NULL;
	}
	
	return theNetwork;
}


CXNetworkRef CXNewNetworkFromRandomRewiring(const CXNetworkRef originalNetwork, CXFloat rewiringProbability){
	CXIndex* fromList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	
	memcpy(fromList, originalNetwork->edgeFromList, sizeof(CXSize)*originalNetwork->edgesCount);
	memcpy(toList, originalNetwork->edgeToList, sizeof(CXSize)*originalNetwork->edgesCount);
	
	CXNetworkRef theNetwork = CXNewNetworkFromRandomRewiringEdgeList(fromList, toList, originalNetwork->edgesCount, originalNetwork->verticesCount, originalNetwork->directed,rewiringProbability);
	
	free(fromList);
	free(toList);
	return theNetwork;
}


CXNetworkRef CXNewNetworkFromModularRandomRewiring(const CXNetworkRef originalNetwork, CXIntegerArray modules, CXFloat rewiringProbability){
	CXIndex* fromList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	
	memcpy(fromList, originalNetwork->edgeFromList, sizeof(CXSize)*originalNetwork->edgesCount);
	memcpy(toList, originalNetwork->edgeToList, sizeof(CXSize)*originalNetwork->edgesCount);
	
	CXNetworkRef theNetwork = CXNewNetworkFromRandomRewiringEdgeList(fromList, toList, originalNetwork->edgesCount, originalNetwork->verticesCount, originalNetwork->directed,rewiringProbability);
	
	free(fromList);
	free(toList);
	return theNetwork;
}


// CXNetworkRef CXNewWattsStrogatzNetwork(CXSize approximateNumberOfVertices, CXSize dimension, CXFloat connectionRadius, CXBool toroidal, CXFloat rewiringProbability){
// 	CXSize* gridSize = calloc(dimension, sizeof(CXSize));
// 	CXSize sizePerDimension = CXMAX(1,ceil(pow(approximateNumberOfVertices, 1.0/dimension)));
// 	CXIndex gridIndex;
// 	for (gridIndex=0; gridIndex<dimension; gridIndex++) {
// 		gridSize[gridIndex] = sizePerDimension;
// 	}
// 	CXNetworkRef gridNetwork = CXNewRegularNetwork(gridSize, dimension, connectionRadius, toroidal);
	
// 	CXNetworkRef wsNetwork = CXNewNetworkFromRandomRewiring(gridNetwork, rewiringProbability);
// 	free(gridSize);
// 	CXNetworkDestroy(gridNetwork);
// 	return wsNetwork;
// }


CXNetworkRef CXNewBarabasiAlbertNetwork(CXSize initialSize, CXSize degreeGrowth, CXSize iterations){
	CXSize edgesCount = iterations*degreeGrowth;
	CXIndex* fromList = calloc(edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCount, sizeof(CXIndex));
	CXSize verticesCount = iterations+initialSize;
	CXIndex currentLink=0;
	CXIndex currentVertex;
	
	CXSize* distribGenerator = calloc(initialSize+(degreeGrowth*2)*iterations, sizeof(CXSize));
	CXIndex distribPointer=0;
	CXIndex i;
	for(i=0;i<initialSize;i++){
		distribGenerator[distribPointer]=i;
		distribPointer++;
	}
	
	for(currentVertex=initialSize;currentVertex < iterations+initialSize;currentVertex++){
		CXIndex m;
		for(m=0;m<degreeGrowth;m++){
			CXIndex connectTo = 0;
			CXBool linkExist = CXFalse;
			do{
				linkExist = CXFalse;
				connectTo = distribGenerator[CXRandomInRange(0, distribPointer-1)];
				CXIndex curLink;
				for(curLink=0;curLink<currentLink;curLink++){
					if((fromList[curLink]==currentVertex&&toList[curLink]==connectTo) ||
					   (fromList[curLink]==connectTo&&toList[curLink]==currentVertex)    ){
						linkExist=CXTrue;
					}
				}
			}while(linkExist);
			fromList[currentLink] = currentVertex;
			toList[currentLink] = connectTo;
			currentLink++;
			distribGenerator[distribPointer]=connectTo;
			distribPointer++;
		}
		for(m=0;m<degreeGrowth;m++){
			distribGenerator[distribPointer]=currentVertex;
			distribPointer++;
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, CXFalse);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	free(fromList);
	free(toList);
	free(distribGenerator);
	return theNetwork;
}



CXNetworkRef* CXNewBarabasiAlbertNetworkOverTime(CXSize initialSize, CXSize degreeGrowth, CXSize* iterationsArray, CXSize iterationsCount){
	CXSize edgesCount = iterationsArray[iterationsCount-1]*degreeGrowth;
	CXIndex* fromList = calloc(edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCount, sizeof(CXIndex));
	CXSize verticesCount = iterationsArray[iterationsCount-1]+initialSize;
	CXIndex currentLink=0;
	CXIndex currentVertex;
	CXNetworkRef* networks = calloc(iterationsCount, sizeof(CXNetworkRef));
	CXSize* distribGenerator = calloc(initialSize+(degreeGrowth*2)*iterationsArray[iterationsCount-1], sizeof(CXSize));
	CXIndex distribPointer=0;
	CXIndex i;
	for(i=0;i<initialSize;i++){
		distribGenerator[distribPointer]=i;
		distribPointer++;
	}
	currentVertex=initialSize;
	for (CXIndex iterationIndex = 0; iterationIndex < iterationsCount; iterationIndex++) {
		CXSize iterations = iterationsArray[iterationIndex];
		for(;currentVertex < iterations+initialSize;currentVertex++){
			CXIndex m;
			for(m=0;m<degreeGrowth;m++){
				CXIndex connectTo = 0;
				CXBool linkExist = CXFalse;
				do{
					linkExist = CXFalse;
					connectTo = distribGenerator[CXRandomInRange(0, distribPointer-1)];
					CXIndex curLink;
					for(curLink=0;curLink<currentLink;curLink++){
						if((fromList[curLink]==currentVertex&&toList[curLink]==connectTo) ||
							 (fromList[curLink]==connectTo&&toList[curLink]==currentVertex)    ){
							linkExist=CXTrue;
						}
					}
				}while(linkExist);
				fromList[currentLink] = currentVertex;
				toList[currentLink] = connectTo;
				currentLink++;
				distribGenerator[distribPointer]=connectTo;
				distribPointer++;
			}
			for(m=0;m<degreeGrowth;m++){
				distribGenerator[distribPointer]=currentVertex;
				distribPointer++;
			}
		}

		CXSize currentVerticesCount = iterations+initialSize;
		CXSize currentEdgesCount = iterations*degreeGrowth;

		CXNetworkRef theNetwork = CXNewNetwork(currentVerticesCount, CXFalse, CXFalse);
		CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, currentEdgesCount);
		networks[iterationIndex] = theNetwork;
	}
	free(fromList);
	free(toList);
	free(distribGenerator);
	return networks;
}


CXBool CXNetworkCouldBeIsomorphic(const CXNetworkRef aNetwork,const CXNetworkRef bNetwork){
	if(aNetwork->directed||bNetwork->directed){ //DIRECTED NOT SUPPORTED
		return CXFalse;
	}
	if(aNetwork->vertexWeighted||bNetwork->vertexWeighted){ //DIRECTED NOT SUPPORTED
		return CXFalse;
	}
	if(aNetwork->verticesCount!=bNetwork->verticesCount){
		return CXFalse;
	}
	
	if(aNetwork->edgesCount!=bNetwork->edgesCount){
		return CXFalse;
	}
	
	CXIntegerArray aDegrees;
	CXIntegerArray bDegrees;
	
	CXIntegerArrayInitWithCapacity(1, &aDegrees);
	CXIntegerArrayInitWithCapacity(1, &bDegrees);
	
	CXNetworkGetDegree(aNetwork, &aDegrees);
	CXNetworkGetDegree(bNetwork, &bDegrees);
	
	CXIntegerArrayQuickSort3(&aDegrees);
	CXIntegerArrayQuickSort3(&bDegrees);
	
	CXIndex i;
	CXBool degreeOk = CXTrue;;
	for (i=0; i<aNetwork->verticesCount; i++) {
		if(aDegrees.data[i]!=bDegrees.data[i]){
			degreeOk = CXFalse;
		}
	}
	
	
	if(!degreeOk){
		CXIntegerArrayDestroy(&aDegrees);
		CXIntegerArrayDestroy(&bDegrees);
		return CXFalse;
	}
	
	
	//Neighbors
	CXIntegerArray aEdgeDegrees;
	CXIntegerArray bEdgeDegrees;
	
	CXNetworkGetDegree(aNetwork, &aDegrees);
	CXNetworkGetDegree(bNetwork, &bDegrees);
	
	CXIntegerArrayInitWithCapacity(1, &aEdgeDegrees);
	CXIntegerArrayInitWithCapacity(1, &bEdgeDegrees);
	
	CXBool edgesDegreeOk = CXTrue;
	for (i=0; i<aNetwork->edgesCount; i++) {
		CXInteger aEdgeDegree = aDegrees.data[aNetwork->edgeFromList[i]]+aDegrees.data[aNetwork->edgeToList[i]];
		CXInteger bEdgeDegree = bDegrees.data[bNetwork->edgeFromList[i]]+bDegrees.data[bNetwork->edgeToList[i]];
		
		CXIntegerArrayAdd(aEdgeDegree, &aEdgeDegrees);
		CXIntegerArrayAdd(bEdgeDegree, &bEdgeDegrees);
	}
	
	CXIntegerArrayQuickSort3(&aEdgeDegrees);
	CXIntegerArrayQuickSort3(&bEdgeDegrees);
	
	for (i=0; i<aNetwork->edgesCount; i++) {
		if(aEdgeDegrees.data[i]!=bEdgeDegrees.data[i]){
			edgesDegreeOk = CXFalse;
		}
	}
	
	CXIntegerArrayDestroy(&aDegrees);
	CXIntegerArrayDestroy(&bDegrees);
	CXIntegerArrayDestroy(&aEdgeDegrees);
	CXIntegerArrayDestroy(&bEdgeDegrees);
	
	if(!edgesDegreeOk){
		return CXFalse;
	}
	
	
	
	return CXTrue;
}





CXBool CXNetworkAddNewEdge(CXNetworkRef network, CXIndex fromIndex, CXIndex toIndex, CXFloat weight){
	CXIndex initialEdge = network->edgesCount;
	CXSize verticesCount = network->verticesCount;
	if (toIndex >= verticesCount||fromIndex >= verticesCount) {
		return CXFalse;
	}
	CXNetworkGrowEdgesCapacity(network,1);
	network->edgeFromList[initialEdge]=fromIndex;
	network->edgeToList[initialEdge]=toIndex;
	
	CXNetworkGrowVertexSetEdgeForVertex(network,initialEdge,fromIndex,toIndex);
	
	if(network->edgeWeighted&&weight>=0){
		network->edgesWeights[initialEdge]=weight;
	}else if(network->edgeWeighted){
		network->edgesWeights[initialEdge]=1.0f;
	}
	if(!network->directed){
		//printf("Index: %lu toIndex:%lu fromIndex:%lu\n",i+initialEdge,toIndex,fromIndex);
		CXNetworkGrowVertexSetEdgeForVertex(network,initialEdge,toIndex,fromIndex);
		//printf("OK\n");
	}else{
		CXNetworkGrowVertexSetInEdgeForVertex(network,initialEdge,toIndex,fromIndex);
	}
	network->edgesCount++;
	return CXTrue;
}

CXBool CXNetworkAddNewEdgeAndIntegrateWeight(CXNetworkRef network, CXIndex fromIndex, CXIndex toIndex, CXFloat weight){
	CXIndex initialEdge = network->edgesCount;
	CXSize verticesCount = network->verticesCount;
	if (toIndex >= verticesCount||fromIndex >= verticesCount) {
		return CXFalse;
	}
	
	CXBool edgeFound = CXFalse;
	CXIndex i;
	CXSize toVerticesCount = network->vertexNumOfEdges[fromIndex];
	CXIndex* toVertices = network->vertexEdgesLists[fromIndex];
	for (i=0; i<toVerticesCount; i++) {
		if(toVertices[i]==toIndex){
			edgeFound = CXTrue;
			break;
		}
	}
	
	if(edgeFound){
		if(network->edgeWeighted&&weight>0){
			network->edgesWeights[network->vertexEdgesIndices[fromIndex][i]]+=weight;
		}
	}else{
		CXNetworkGrowEdgesCapacity(network,1);
		network->edgeFromList[initialEdge]=fromIndex;
		network->edgeToList[initialEdge]=toIndex;
		
		CXNetworkGrowVertexSetEdgeForVertex(network,initialEdge,fromIndex,toIndex);
		
		if(network->edgeWeighted&&weight>=0){
			network->edgesWeights[initialEdge]=weight;
		}else if(network->edgeWeighted){
			network->edgesWeights[initialEdge]=1.0f;
		}
		if(!network->directed){
			//printf("Index: %lu toIndex:%lu fromIndex:%lu\n",i+initialEdge,toIndex,fromIndex);
			//FIXME: Directed Networks
			CXNetworkGrowVertexSetEdgeForVertex(network,initialEdge,toIndex,fromIndex);
			//printf("OK\n");
		}else{
			CXNetworkGrowVertexSetInEdgeForVertex(network,initialEdge,toIndex,fromIndex);
		}
		network->edgesCount++;
	}
	return CXTrue;
}


void CXNetworkWriteToGMLFile(CXNetworkRef theNetwork, FILE* networkFile){
	fprintf(networkFile, "graph [\n");
	if(theNetwork->directed){
		fprintf(networkFile, "  directed 1\n");
	}
	
	CXIndex i;
	for(i=0;i<theNetwork->verticesCount;i++){
		fprintf(networkFile,"  node [\n");
		
		fprintf(networkFile,"    id %"CXIndexScan"\n",i);
		if(theNetwork->vertexNames){
			fprintf(networkFile,"    label \"%s\"\n",theNetwork->vertexNames[i]);
		}
		
		CXIndex propertyIndex;
		for(propertyIndex=0;propertyIndex<theNetwork->propertiesCount;propertyIndex++){
			CXPropertyType type = theNetwork->propertiesTypes[propertyIndex];
			void* data = theNetwork->propertiesData[propertyIndex];
			CXString name = theNetwork->propertiesNames[propertyIndex];
			switch (type) {
				case CXNumberPropertyType:{
					CXFloat* floatData = data;
					fprintf(networkFile,"    ");
					CXBool nextUpper = CXFalse;
					while (*name) {
						if(isalnum(*name)){
							fputc(nextUpper?toupper(*name):*name, networkFile);
							nextUpper = CXFalse;
						}else if(isspace(*name)){
							nextUpper=CXTrue;
						}
						name++;
					}
					fprintf(networkFile," ");
					fprintf(networkFile,"%"CXFloatScan"\n",floatData[i]);
					break;
				}
				case CXStringPropertyType:{
					CXString* stringData = data;
					fprintf(networkFile,"    ");
					CXBool nextUpper = CXFalse;
					while (*name) {
						if(isalnum(*name)){
							fputc(nextUpper?toupper(*name):*name, networkFile);
							nextUpper = CXFalse;
						}else if(isspace(*name)){
							nextUpper=CXTrue;
						}
						name++;
					}
					fprintf(networkFile," ");
					fprintf(networkFile,"\"%s\"\n",stringData[i]);
					break;
				}
				case CXVector2DPropertyType:{
					CXFloat* floatData = data;
					fprintf(networkFile,"    ");
					CXBool nextUpper = CXFalse;
					while (*name) {
						if(isalnum(*name)){
							fputc(nextUpper?toupper(*name):*name, networkFile);
							nextUpper = CXFalse;
						}else if(isspace(*name)){
							nextUpper=CXTrue;
						}
						name++;
					}
					fprintf(networkFile," ");
					fprintf(networkFile,"[ x %"CXFloatScan" y %"CXFloatScan" ]\n",floatData[i*2],floatData[i*2+1]);
					break;
				}
				case CXVector3DPropertyType:{
					CXFloat* floatData = data;
					if(strcmp(name, "Position")==0 || strcmp(name, "position")==0 ){
						fprintf(networkFile,"    graphics");
					}else{
						fprintf(networkFile,"    ");
						CXBool nextUpper = CXFalse;
						while (*name) {
							if(isalnum(*name)){
								fputc(nextUpper?toupper(*name):*name, networkFile);
								nextUpper = CXFalse;
							}else if(isspace(*name)){
								nextUpper=CXTrue;
							}
							name++;
						}
					}
					fprintf(networkFile," ");
					fprintf(networkFile,"[ x %"CXFloatScan" y %"CXFloatScan" z %"CXFloatScan" ]\n",floatData[i*3],floatData[i*3+1],floatData[i*3+2]);
					break;
				}
				default:{
				}
			}
		}
		
		fprintf(networkFile,"  ]\n");
	}
	CXIndex edgeIndex;
	
	CXIndex* edgesFrom = theNetwork->edgeFromList;
	CXIndex* edgesTo = theNetwork->edgeToList;
	CXFloat* edgesWeights = theNetwork->edgesWeights;
	for (edgeIndex=0; edgeIndex<theNetwork->edgesCount; edgeIndex++) {
		fprintf(networkFile,"  edge [\n");
		CXIndex fromVertex = edgesFrom[edgeIndex];
		CXIndex toVertex = edgesTo[edgeIndex];
		if(theNetwork->edgeWeighted){
			CXFloat weight = edgesWeights[edgeIndex];
			fprintf(networkFile,"    source %"CXIndexScan"\n    target %"CXIndexScan" \n    weight %"CXFloatScan"\n",fromVertex,toVertex,weight);
		}else{
			fprintf(networkFile,"    source %"CXIndexScan"\n    target %"CXIndexScan"\n",fromVertex,toVertex);
		}
		fprintf(networkFile,"  ]\n");
	}
	
	fprintf(networkFile,"]\n");
}


CXFloat CXNetworkClusteringCoefficient(const CXNetworkRef aNetwork, CXIndex nodeIndex){
	CXSize vertexEdgesCount = aNetwork->vertexNumOfEdges[nodeIndex];
	CXIndex* vertexEdgesList = aNetwork->vertexEdgesLists[nodeIndex];
	CXSize inLevelConnections = 0;
	CXIndex ni;
	CXBitArray isNeighbor = CXNewBitArray(aNetwork->verticesCount);
	for(ni=0;ni<vertexEdgesCount;ni++){
		CXBitArraySet(isNeighbor, vertexEdgesList[ni]);
	}
	for(ni=0;ni<vertexEdgesCount;ni++){
		CXIndex neighborVertex = vertexEdgesList[ni];
		CXSize neighborEdgesCount = aNetwork->vertexNumOfEdges[neighborVertex];
		CXIndex* neighborEdgesList = aNetwork->vertexEdgesLists[neighborVertex];
		CXIndex nni;
		for(nni=0;nni<neighborEdgesCount;nni++){
			if(CXBitArrayTest(isNeighbor,neighborEdgesList[nni])){
				inLevelConnections++;
			}
		}
	}
	CXBitArrayDestroy(isNeighbor);
	if((vertexEdgesCount-1.0) > 0.0){
		return (inLevelConnections)/(CXFloat)(vertexEdgesCount*(vertexEdgesCount-1.0f));
	}else{
		return 0.0f;
	}
}


CXNetworkRef CXNewNetworkHomogeneusModel(CXSize verticesCount,CXSize degree){
	
	CXIndex try = 0;
	CXSize maxTries = 1000;
	CXSize remaining = 0;
	CXNetworkRef network = NULL;
	CXSize originalVerticesCount = verticesCount;
	for (try=0; try<maxTries; try++) {
		verticesCount = originalVerticesCount+(try/20);
		CXSize i;
		network = CXNewNetwork(verticesCount, CXFalse, CXFalse);

		CXUIntegerArray enabledVertices;
		CXUIntegerArrayInitWithCapacity(verticesCount, &enabledVertices);

		CXUIntegerArray verticesDegree;
		CXUIntegerArrayInitWithCapacity(verticesCount, &verticesDegree);
		CXUIntegerArraySetCount(verticesCount, &verticesDegree);

		for (i=0; i<verticesCount; i++) {
			CXUIntegerArrayAdd(i, &enabledVertices);
		}

		struct __cv_edge{
			CXIndex from;
			CXIndex to;
		};
		struct __cv_edge_element{
			struct __cv_edge edge;
			UT_hash_handle hh;
		};

		struct __cv_edge_element* edgesHash = NULL;


		for (i=0; i<verticesCount; i++) {
			//printf("Adding node: %"CXUIntegerScan"\n",i);
			while(verticesDegree.data[i]<degree){
				CXIndex choice = i;
				CXBool edgeExists = CXTrue;
				//printf("\tAdding edge (%"CXUIntegerScan"/%"CXUIntegerScan")\n",verticesDegree.data[i]+1,degree);
				while(choice==i || edgeExists){
					choice = enabledVertices.data[CXRandomInRange(0, enabledVertices.count)];
					//printf("\t\tEnabled: [");
					//CXIndex ii;
					//for(ii=0;ii<enabledVertices.count;ii++){
					//	if(ii){
							//printf(", ");
					//	}
						//printf("%"CXUIntegerScan,enabledVertices.data[ii]);
					//}
					//printf("]\n");
					//printf("\t\tTesting: %"CXUIntegerScan"\n",choice);
					
					struct __cv_edge edgesKey;
					struct __cv_edge_element* edgeElement = NULL;
					
					edgesKey.from = CXMIN(i, choice);
					edgesKey.to = CXMAX(i, choice);
					
					
					HASH_FIND(hh, edgesHash, &edgesKey, sizeof(struct __cv_edge), edgeElement);
					//printf("\t\t\tElement: %p\n",edgeElement);
					if(edgeElement || edgesKey.from==edgesKey.to){
						edgeExists=CXTrue;
					//	printf("\t\t\tFAIL: %"CXUIntegerScan"\n",choice);
					}else{
						edgeExists=CXFalse;
					//	printf("\t\t\tSUCCESS: %"CXUIntegerScan"\n",choice);
					}
					if(edgeExists&&enabledVertices.count<degree){
						choice=i;
						break;
					}
				}
				
				if(i==choice){
					break;
				}
				
				
				struct __cv_edge_element* edgeElement = calloc(1, sizeof(struct __cv_edge_element));
				
				edgeElement->edge.from = CXMIN(i, choice);
				edgeElement->edge.to = CXMAX(i, choice);
				
				HASH_ADD_KEYPTR(hh, edgesHash, (&(edgeElement->edge)), sizeof(struct __cv_edge), edgeElement);
				
				//printf("\t\t\tAdding: %"CXUIntegerScan"\n",choice);
				
				//printf("\t\t\tHash:[");
				
		//			CXBool oi = CXTrue;
		//			struct __cv_edge_element* s;
		//			for(s=edgesHash; s != NULL; s=s->hh.next) {
		//				if(oi){
		//					oi=CXFalse;
		//				}else{
		//					printf(", ");
		//				}
		//				printf("%"CXUIntegerScan"-%"CXUIntegerScan, s->edge.from, s->edge.to);
		//			}
		//			printf("]\n");
				
				CXNetworkAddNewEdge(network, i, choice, 1.0);
				
				verticesDegree.data[i]++;
				verticesDegree.data[choice]++;
				
				CXIndex j;
				CXIndex current = 0;
				for (j=0; j<enabledVertices.count; j++) {
					if(verticesDegree.data[enabledVertices.data[j]]<degree){
						enabledVertices.data[current] = enabledVertices.data[j];
						current++;
					}
				}
				enabledVertices.count = current;
				
				if(enabledVertices.count<degree){
					break;
				}
			}
		}
		remaining=enabledVertices.count;
		printf("Remaining\t%"CXUIntegerScan"\n",enabledVertices.count);

		struct __cv_edge_element* edgeElement, *tempElement;

		HASH_ITER(hh, edgesHash, edgeElement, tempElement) {
			HASH_DEL(edgesHash, edgeElement);
			free(edgeElement);
		}
			
		if(remaining==0){
			break;
		}
	}
	return network;
}





























CXNetworkRef CXNewNetworkRemoveChains(CXIndex* fromList,CXIndex* toList, CXSize edgesCount, CXSize verticesCount, CXBool directed, CXFloat* positions,CXFloat minRectangleSize, CXFloat maxRectangleSize,CXSize rectangleCount,CXFloat removeProbability){
	CXIndex edgeIndex = 0;
	CXSize newEdgesCount = 0;
	
	CXIndex i;
	CXFloat positionXMax = CXFloatMIN;
	CXFloat positionXMin = CXFloatMAX;
	CXFloat positionYMax = CXFloatMIN;
	CXFloat positionYMin = CXFloatMAX;
	
	for (i=0; i<verticesCount; i++) {
		CXFloat x = positions[3*i];
		CXFloat y = positions[3*i+1];
		positionXMax = CXMAX(positionXMax, x);
		positionXMin = CXMIN(positionXMin, x);
		positionYMax = CXMAX(positionYMax, y);
		positionYMin = CXMIN(positionYMin, y);
	}
	CXFloat* rectangles = calloc(rectangleCount*4, sizeof(CXFloat));
	
	for (i=0; i<rectangleCount; i++) {
		CXFloat cx = positionXMin + CXRandomFloat()*(positionXMax-positionXMin);
		CXFloat cy = positionYMin + CXRandomFloat()*(positionYMax-positionYMin);
		
		CXFloat sizex = (minRectangleSize + (maxRectangleSize-minRectangleSize)*CXRandomFloat())*(positionXMax-positionXMin);
		CXFloat sizey = (minRectangleSize + (maxRectangleSize-minRectangleSize)*CXRandomFloat())*(positionYMax-positionYMin);
		
		rectangles[i*4 + 0] = cx;
		rectangles[i*4 + 1] = cy;
		rectangles[i*4 + 2] = sizex;
		rectangles[i*4 + 3] = sizey;
		//printf("xy = (%f, %f)    rect = [%f, %f, %f, %f]\n",0.1,1.2,cx-sizex*0.5f,cy-sizey*0.5f,cx+sizex*0.5f,cy+sizey*0.5f);
	}
	
	double* probabilityVertices = calloc(verticesCount,sizeof(double));
	CXIndex j;
	for (j=0; j<verticesCount; j++) {
		probabilityVertices[j] = 1.0;
		CXFloat x = positions[3*j];
		CXFloat y = positions[3*j+1];
		for (i=0; i<rectangleCount; i++) {
			CXFloat cx = rectangles[i*4 + 0];
			CXFloat cy = rectangles[i*4 + 1];
			CXFloat sizex = rectangles[i*4 + 2];
			CXFloat sizey = rectangles[i*4 + 3];
			//printf("xy = (%f, %f)    rect = [%f, %f, %f, %f]\n",x,y,cx-sizex*0.5f,cy-sizey*0.5f,cx+sizex*0.5f,cy+sizey*0.5f);
			
			if(cx-sizex*0.5f <= x && x <= cx+sizex*0.5f && cy-sizey*0.5f <= y && y <= cy+sizey*0.5f){
				probabilityVertices[j] *= (1.0-removeProbability);
			}
		}
	}
	
	for (edgeIndex=0; edgeIndex<edgesCount; edgeIndex++) {
		//printf("%f\n",probabilityVertices[fromList[edgeIndex]] * probabilityVertices[fromList[edgeIndex]]);
		if(CXRandomFloat() < probabilityVertices[fromList[edgeIndex]] * probabilityVertices[toList[newEdgesCount]]){ //not remove
			fromList[newEdgesCount] = fromList[edgeIndex];
			toList[newEdgesCount] = toList[edgeIndex];
			newEdgesCount++;
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, directed);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, newEdgesCount);
	
	free(rectangles);
	free(probabilityVertices);
	return theNetwork;
}

CXNetworkRef CXNewNetworkRemovingChains(const CXNetworkRef originalNetwork){
	CXNetworkRef theNetwork =NULL;
	CXFloat* positions = NULL;
	CXSize verticesCount = originalNetwork->verticesCount;
	CXSize edgesCount = originalNetwork->edgesCount;
	CXPropertyType positionType = CXUnknownPropertyType;
	positions = CXNetworkPropertyWithName(originalNetwork, "Position", &positionType);
	
	CXIndex* fromList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	CXIndex* toList = calloc(originalNetwork->edgesCount, sizeof(CXIndex));
	
	CXSize chains = 0;
	CXBitArray removedEdges = CXNewBitArray(edgesCount);
	CXBitArray modifiedEdges = CXNewBitArray(edgesCount);
	do{
		chains = 0;
		CXBitArrayClearAll(removedEdges, edgesCount);
		CXBitArrayClearAll(modifiedEdges, edgesCount);
		CXIndex edgeIndex = 0;
		CXSize newEdgesCount = 0;
		CXNetworkRef currentNetwork;
		
		if(theNetwork){
			currentNetwork = theNetwork;
		}else{
			currentNetwork = originalNetwork;
		}
		
		memcpy(fromList, currentNetwork->edgeFromList, sizeof(CXSize)*currentNetwork->edgesCount);
		memcpy(toList, currentNetwork->edgeToList, sizeof(CXSize)*currentNetwork->edgesCount);
		
		CXIndex vi;
		for (vi=0; vi<verticesCount; vi++) {
			CXSize neighCount = currentNetwork->vertexNumOfEdges[vi];
			if(neighCount==2){
				CXIndex* neigh = currentNetwork->vertexEdgesLists[vi];
				CXIndex vertex1 = neigh[0];
				CXIndex vertex2 = neigh[1];
				CXIndex edgeIndex1 = currentNetwork->vertexEdgesIndices[vi][0];
				CXIndex edgeIndex2 = currentNetwork->vertexEdgesIndices[vi][1];
				if(!CXBitArrayTest(removedEdges, edgeIndex1) && !CXBitArrayTest(removedEdges, edgeIndex1) &&
				   !CXBitArrayTest(modifiedEdges, edgeIndex1) && !CXBitArrayTest(modifiedEdges, edgeIndex1) &&
				   !CXNetworkAreAdjacent(currentNetwork, vertex1, vertex2)){
					fromList[edgeIndex1] = vertex1;
					toList[edgeIndex1] = vertex2;
					CXBitArraySet(modifiedEdges, edgeIndex1);
					CXBitArraySet(removedEdges, edgeIndex2);
					chains++;
				}
			}
		}
		
		
		for (edgeIndex=0; edgeIndex<currentNetwork->edgesCount; edgeIndex++) {
			if(!CXBitArrayTest(removedEdges, edgeIndex)){
				fromList[newEdgesCount] = fromList[edgeIndex];
				toList[newEdgesCount] = toList[edgeIndex];
				newEdgesCount++;
			}
		}
		
		if(theNetwork){
			free(theNetwork);
		}
		theNetwork = CXNewNetwork(verticesCount, CXFalse, originalNetwork->directed);
		CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, newEdgesCount);
		edgesCount = theNetwork->edgesCount;
		printf("chains:%"CXSizeScan"\n",chains);
	}while(chains>0);
	
	if(positions && positionType==CXVector3DPropertyType && theNetwork){
		CXNetworkAppendProperty(theNetwork, "Position", CXVector3DPropertyType, positions);
	}
	
	free(fromList);
	free(toList);
	CXBitArrayDestroy(removedEdges);
	CXBitArrayDestroy(modifiedEdges);
	return theNetwork;
	
}



CXSize CXNetworkNumberOfConnectedComponents(const CXNetworkRef theNetwork, CXGenericArray* connectedComponents){
	CXSize verticesCount = theNetwork->verticesCount;
	CXSize edgesCount = theNetwork->edgesCount;
	
	CXUIntegerArray groups;
	CXUIntegerArrayInitWithCapacity(verticesCount, &groups);
	CXUIntegerArraySetCount(verticesCount, &groups);
	
	CXUIntegerArray edgesGroups;
	CXUIntegerArrayInitWithCapacity(edgesCount, &edgesGroups);
	CXUIntegerArraySetCount(edgesCount, &edgesGroups);
	
	CXUIntegerArray vertexList;
	CXUIntegerArrayInitWithCapacity(verticesCount, &vertexList);
	CXUIntegerArraySetCount(verticesCount, &vertexList);
	
	CXBitArray visited = CXNewBitArray(verticesCount);
	CXBitArray inList = CXNewBitArray(verticesCount);
	
	
	CXSize numGroups = 0;
	CXIndex vIndex;
	CXSize largestGroup=0;
	CXSize largestGroupSize=0;
	CXSize currentGroupSize=0;
	
	for(vIndex=0;vIndex<verticesCount;vIndex++){
		if(!CXBitArrayTest(visited, vIndex)){
			numGroups++;
			groups.data[vIndex]=numGroups-1;
			currentGroupSize++;
			CXSize listSize=1;
			CXIndex currentVertex;
			vertexList.data[0]=vIndex;
			CXBitArrayClearAll(inList, verticesCount);
			CXBitArraySet(inList, vIndex);
			while(listSize>0){
				currentVertex=vertexList.data[listSize-1];
				if(!CXBitArrayTest(visited, currentVertex)){
					CXBitArraySet(visited, currentVertex);
					
					CXSize edgesCount = theNetwork->vertexNumOfEdges[currentVertex];
					CXIndex* verticesList = theNetwork->vertexEdgesLists[currentVertex];
					CXIndex* edgesIndices = theNetwork->vertexEdgesIndices[currentVertex];
					
					CXIndex edgeIndex;
					
					CXBool addedToGroup = CXFalse;
					for(edgeIndex=0;edgeIndex<edgesCount;edgeIndex++){
						CXIndex linkedVertex = verticesList[edgeIndex];
						
						edgesGroups.data[edgesIndices[edgeIndex]] = numGroups-1;
						
						if(!CXBitArrayTest(inList,linkedVertex) && !CXBitArrayTest(visited,linkedVertex)){
							vertexList.data[listSize]=linkedVertex;
							listSize++;
							
							CXBitArraySet(inList, linkedVertex);
							
							groups.data[linkedVertex]=numGroups-1;
							currentGroupSize++;
							
							addedToGroup = CXTrue;
						}
					}
					if(theNetwork->directed){
						CXSize edgesCount = theNetwork->vertexNumOfInEdges[currentVertex];
						CXIndex* verticesList = theNetwork->vertexInEdgesLists[currentVertex];
						CXIndex* edgesIndices = theNetwork->vertexInEdgesIndices[currentVertex];
						
						CXIndex edgeIndex;
						//NSLog(@" Links from node %d listsize %d",currentVertex,listSize);
						for(edgeIndex=0;edgeIndex<edgesCount;edgeIndex++){
							CXIndex linkedVertex = verticesList[edgeIndex];
							
							edgesGroups.data[edgesIndices[edgeIndex]] = numGroups-1;
							
							if(!CXBitArrayTest(inList,linkedVertex) && !CXBitArrayTest(visited,linkedVertex)){
								vertexList.data[listSize]=linkedVertex;
								listSize++;
								
								CXBitArraySet(inList, linkedVertex);
								
								groups.data[linkedVertex]=numGroups-1;
								currentGroupSize++;
								
								addedToGroup = CXTrue;
							}
						}
					}
					if(!addedToGroup){
						CXBitArrayClear(inList, vertexList.data[listSize-1]);
						listSize--;
					}
				}else{
					CXBitArrayClear(inList, vertexList.data[listSize-1]);
					listSize--;
				}
			}
			if(currentGroupSize>largestGroupSize){
				largestGroupSize=currentGroupSize;
				largestGroup=numGroups-1;
			}
		}
	}
	
	if(connectedComponents){
		if(!connectedComponents->data){
			CXGenericArrayInitWithCapacity(numGroups, connectedComponents);
		}
		connectedComponents->count = 0;
	
		
		CXFloatArray* subVerticesLists = calloc(numGroups, sizeof(CXFloatArray));
		CXUIntegerArray* subEdgesFrom = calloc(numGroups, sizeof(CXUIntegerArray));
		CXUIntegerArray* subEdgesTo = calloc(numGroups, sizeof(CXUIntegerArray));
		CXFloatArray* subEdgesWeight = NULL;
		
		if(theNetwork->edgeWeighted){
			subEdgesWeight = calloc(numGroups, sizeof(CXFloatArray));
		}
		
		CXUIntegerArray newVerticesIndices;
		CXUIntegerArrayInitWithCapacity(verticesCount, &newVerticesIndices);
		CXUIntegerArraySetCount(verticesCount, &newVerticesIndices);
		
		CXIndex i;
	
		for(i=0;i<numGroups;i++){
			CXFloatArrayInitWithCapacity(10, subVerticesLists+i);
			CXUIntegerArrayInitWithCapacity(10, subEdgesFrom+i);
			CXUIntegerArrayInitWithCapacity(10, subEdgesTo+i);
			if(theNetwork->edgeWeighted){
				CXFloatArrayInitWithCapacity(10, subEdgesWeight+i);
			}
		}
	
		
		for(i=0;i<verticesCount;i++){
			CXIndex group = groups.data[i];
			newVerticesIndices.data[i] = subVerticesLists[group].count;
			CXFloatArrayAdd(i, subVerticesLists+group);
		}
		
		for(i=0;i<edgesCount;i++){
			CXIndex fromVertex = theNetwork->edgeFromList[i];
			CXIndex toVertex = theNetwork->edgeToList[i];
			
			if(edgesGroups.data[i]==groups.data[fromVertex]&&edgesGroups.data[i]==groups.data[toVertex]){
				CXUIntegerArrayAdd(newVerticesIndices.data[fromVertex], subEdgesFrom + edgesGroups.data[i]);
				CXUIntegerArrayAdd(newVerticesIndices.data[toVertex], subEdgesTo + edgesGroups.data[i]);
			}
			if(theNetwork->edgeWeighted){
				CXFloatArrayAdd(theNetwork->edgesWeights[i], subEdgesWeight + edgesGroups.data[i]);
			}
		}
		
		for(i=0;i<numGroups;i++){
			CXSize groupVerticesCount = subVerticesLists[i].count;
			CXNetworkRef groupNetwork = CXNewNetwork(groupVerticesCount, theNetwork->edgeWeighted, theNetwork->directed);
			if(!theNetwork->edgeWeighted){
				CXNetworkAddNewEdges(groupNetwork, subEdgesFrom[i].data, subEdgesTo[i].data, NULL, subEdgesFrom[i].count);
			}else{
				CXNetworkAddNewEdges(groupNetwork, subEdgesFrom[i].data, subEdgesTo[i].data, subEdgesWeight[i].data, subEdgesFrom[i].count);
			}
			CXNetworkAppendProperty(groupNetwork, "Original Index", CXNumberPropertyType, subVerticesLists[i].data);
			CXGenericArrayAdd(groupNetwork, connectedComponents);
		}
		
		for(i=0;i<numGroups;i++){
			CXFloatArrayDestroy(subVerticesLists+i);
			CXUIntegerArrayDestroy(subEdgesFrom+i);
			CXUIntegerArrayDestroy(subEdgesTo+i);
			if(theNetwork->edgeWeighted){
				CXFloatArrayDestroy(subEdgesWeight+i);
			}
		}
		CXUIntegerArrayDestroy(&newVerticesIndices);
		
		free(subVerticesLists);
		free(subEdgesFrom);
		free(subEdgesTo);
		if(theNetwork->edgeWeighted){
			free(subEdgesWeight);
		}
	}
	
	CXUIntegerArrayDestroy(&groups);
	CXUIntegerArrayDestroy(&edgesGroups);
	CXUIntegerArrayDestroy(&vertexList);
	
	CXBitArrayDestroy(visited);
	CXBitArrayDestroy(inList);
	
	
	return numGroups;
}

CXNetworkRef CXNewSubNetworkFromNetwork(const CXNetworkRef theNetwork, const CXUIntegerArray verticesIndices){
	CXSize verticesCount = theNetwork->verticesCount;
	CXSize edgesCount = theNetwork->edgesCount;
	
	
	CXFloatArray largestVerticesList;
	CXUIntegerArray largestEdgesFrom;
	CXUIntegerArray largestEdgesTo;
	CXFloatArray largestEdgesWeight;
	
	CXFloatArrayInitWithCapacity(10, &largestVerticesList);
	CXUIntegerArrayInitWithCapacity(10, &largestEdgesFrom);
	CXUIntegerArrayInitWithCapacity(10, &largestEdgesTo);
	
	if(theNetwork->edgeWeighted){
		CXFloatArrayInitWithCapacity(10, &largestEdgesWeight);
	}
	
	CXUIntegerArray newVerticesIndices;
	CXUIntegerArrayInitWithCapacity(verticesCount, &newVerticesIndices);
	CXUIntegerArraySetCount(verticesCount, &newVerticesIndices);
	
	
	CXBitArray inSelected = CXNewBitArray(verticesCount);
	
	CXIndex i;
	
	for(i=0;i<verticesIndices.count;i++){
		CXBitArraySet(inSelected, verticesIndices.data[i]);
		newVerticesIndices.data[verticesIndices.data[i]] = i;
		CXFloatArrayAdd(verticesIndices.data[i], &largestVerticesList);
	}
	
	for(i=0;i<edgesCount;i++){
		CXIndex fromVertex = theNetwork->edgeFromList[i];
		CXIndex toVertex = theNetwork->edgeToList[i];
		
		if(CXBitArrayTest(inSelected, fromVertex) && CXBitArrayTest(inSelected, toVertex)){
			CXUIntegerArrayAdd(newVerticesIndices.data[fromVertex], &largestEdgesFrom);
			CXUIntegerArrayAdd(newVerticesIndices.data[toVertex], &largestEdgesTo);
		}
		if(theNetwork->edgeWeighted){
			CXFloatArrayAdd(theNetwork->edgesWeights[i], &largestEdgesWeight);
		}
	}
	
	CXSize groupVerticesCount = verticesIndices.count;
	
	CXNetworkRef groupNetwork = CXNewNetwork(groupVerticesCount, theNetwork->edgeWeighted, theNetwork->directed);
	
	if(!theNetwork->edgeWeighted){
		CXNetworkAddNewEdges(groupNetwork, largestEdgesFrom.data, largestEdgesTo.data, NULL, largestEdgesFrom.count);
	}else{
		CXNetworkAddNewEdges(groupNetwork, largestEdgesFrom.data, largestEdgesTo.data, largestEdgesWeight.data, largestEdgesFrom.count);
	}
	
	CXNetworkAppendProperty(groupNetwork, "Original Index", CXNumberPropertyType, largestVerticesList.data);
	
	if(theNetwork->vertexNames){
		CXString* names = calloc(groupVerticesCount, sizeof(CXString));
		CXString* propertyData = theNetwork->vertexNames;
		for (i=0; i<groupVerticesCount; i++) {
			names[i] = propertyData[verticesIndices.data[i]];
		}
		CXNetworkAppendProperty(groupNetwork, "name", CXStringPropertyType, names);
		free(names);
	}
	
	CXIndex propIndex = 0;
	for (propIndex=0; propIndex< theNetwork->propertiesCount; propIndex++) {
		CXPropertyType propertyType = theNetwork->propertiesTypes[propIndex];
		CXString propertyName = theNetwork->propertiesNames[propIndex];
		
		CXIndex i;
		switch (propertyType) {
			case CXStringPropertyType:{
				CXString* values = calloc(groupVerticesCount, sizeof(CXString));
				CXString* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount; i++) {
					values[i] = propertyData[verticesIndices.data[i]];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			case CXNumberPropertyType:{
				CXFloat* values = calloc(groupVerticesCount, sizeof(CXFloat));
				CXFloat* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount; i++) {
					values[i] = propertyData[verticesIndices.data[i]];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			case CXVector2DPropertyType:{
				CXFloat* values = calloc(groupVerticesCount*2, sizeof(CXFloat));
				CXFloat* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount; i++) {
					values[i*2] = propertyData[verticesIndices.data[i]*2];
					values[i*2+1] = propertyData[verticesIndices.data[i]*2+1];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			case CXVector3DPropertyType:{
				CXFloat* values = calloc(groupVerticesCount*3, sizeof(CXFloat));
				CXFloat* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount; i++) {
					CXIndex selectedIndex = verticesIndices.data[i];
					values[i*3] = propertyData[selectedIndex*3];
					values[i*3+1] = propertyData[selectedIndex*3+1];
					values[i*3+2] = propertyData[selectedIndex*3+2];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			default:
				break;
		}
	}
	
	CXFloatArrayDestroy(&largestVerticesList);
	CXUIntegerArrayDestroy(&largestEdgesFrom);
	CXUIntegerArrayDestroy(&largestEdgesTo);
	
	if(theNetwork->edgeWeighted){
		CXFloatArrayDestroy(&largestEdgesWeight);
	}
	
	CXUIntegerArrayDestroy(&newVerticesIndices);
	CXBitArrayDestroy(inSelected);

	return groupNetwork;
}

CXNetworkRef CXNewNetworkFromLargestComponent(const CXNetworkRef theNetwork){
	CXSize verticesCount = theNetwork->verticesCount;
	CXSize edgesCount = theNetwork->edgesCount;
	
	CXUIntegerArray groups;
	CXUIntegerArrayInitWithCapacity(verticesCount, &groups);
	CXUIntegerArraySetCount(verticesCount, &groups);
	
	CXUIntegerArray edgesGroups;
	CXUIntegerArrayInitWithCapacity(edgesCount, &edgesGroups);
	CXUIntegerArraySetCount(edgesCount, &edgesGroups);
	
	CXUIntegerArray vertexList;
	CXUIntegerArrayInitWithCapacity(verticesCount, &vertexList);
	CXUIntegerArraySetCount(verticesCount, &vertexList);
	
	CXBitArray visited = CXNewBitArray(verticesCount);
	CXBitArray inList = CXNewBitArray(verticesCount);
	
	
	CXSize numGroups = 0;
	CXIndex vIndex;
	CXSize largestGroup=0;
	CXSize largestGroupSize=0;
	CXSize currentGroupSize=0;
	
	for(vIndex=0;vIndex<verticesCount;vIndex++){
		if(!CXBitArrayTest(visited, vIndex)){
			numGroups++;
			groups.data[vIndex]=numGroups-1;
			currentGroupSize++;
			CXSize listSize=1;
			CXIndex currentVertex;
			vertexList.data[0]=vIndex;
			CXBitArrayClearAll(inList, verticesCount);
			CXBitArraySet(inList, vIndex);
			while(listSize>0){
				currentVertex=vertexList.data[listSize-1];
				if(!CXBitArrayTest(visited, currentVertex)){
					CXBitArraySet(visited, currentVertex);
					
					CXSize edgesCount = theNetwork->vertexNumOfEdges[currentVertex];
					CXIndex* verticesList = theNetwork->vertexEdgesLists[currentVertex];
					CXIndex* edgesIndices = theNetwork->vertexEdgesIndices[currentVertex];
					
					CXIndex edgeIndex;
					
					CXBool addedToGroup = CXFalse;
					for(edgeIndex=0;edgeIndex<edgesCount;edgeIndex++){
						CXIndex linkedVertex = verticesList[edgeIndex];
						
						edgesGroups.data[edgesIndices[edgeIndex]] = numGroups-1;
						
						if(!CXBitArrayTest(inList,linkedVertex) && !CXBitArrayTest(visited,linkedVertex)){
							vertexList.data[listSize]=linkedVertex;
							listSize++;
							
							CXBitArraySet(inList, linkedVertex);
							
							groups.data[linkedVertex]=numGroups-1;
							currentGroupSize++;
							
							addedToGroup = CXTrue;
						}
					}
					if(theNetwork->directed){
						CXSize edgesCount = theNetwork->vertexNumOfInEdges[currentVertex];
						CXIndex* verticesList = theNetwork->vertexInEdgesLists[currentVertex];
						CXIndex* edgesIndices = theNetwork->vertexInEdgesIndices[currentVertex];
						
						CXIndex edgeIndex;
						//NSLog(@" Links from node %d listsize %d",currentVertex,listSize);
						for(edgeIndex=0;edgeIndex<edgesCount;edgeIndex++){
							CXIndex linkedVertex = verticesList[edgeIndex];
							
							edgesGroups.data[edgesIndices[edgeIndex]] = numGroups-1;
							
							if(!CXBitArrayTest(inList,linkedVertex) && !CXBitArrayTest(visited,linkedVertex)){
								vertexList.data[listSize]=linkedVertex;
								listSize++;
								
								CXBitArraySet(inList, linkedVertex);
								
								groups.data[linkedVertex]=numGroups-1;
								currentGroupSize++;
								
								addedToGroup = CXTrue;
							}
						}
					}
					if(!addedToGroup){
						CXBitArrayClear(inList, vertexList.data[listSize-1]);
						listSize--;
					}
				}else{
					CXBitArrayClear(inList, vertexList.data[listSize-1]);
					listSize--;
				}
			}
			if(currentGroupSize>largestGroupSize){
				largestGroupSize=currentGroupSize;
				largestGroup=numGroups-1;
			}
		}
	}
	
	CXFloatArray largestVerticesList;
	CXUIntegerArray largestEdgesFrom;
	CXUIntegerArray largestEdgesTo;
	CXFloatArray largestEdgesWeight;
	
	CXFloatArrayInitWithCapacity(10, &largestVerticesList);
	CXUIntegerArrayInitWithCapacity(10, &largestEdgesFrom);
	CXUIntegerArrayInitWithCapacity(10, &largestEdgesTo);
	
	CXUIntegerArray verticesIndices;
	CXUIntegerArrayInitWithCapacity(10, &verticesIndices);
	
	if(theNetwork->edgeWeighted){
		CXFloatArrayInitWithCapacity(10, &largestEdgesWeight);
	}
	
	CXUIntegerArray newVerticesIndices;
	CXUIntegerArrayInitWithCapacity(verticesCount, &newVerticesIndices);
	CXUIntegerArraySetCount(verticesCount, &newVerticesIndices);
	
	
	
	CXIndex i;
	CXUIntegerArray groupSizes;
	CXUIntegerArrayInitWithCapacity(numGroups, &groupSizes);
	CXUIntegerArraySetCount(numGroups, &groupSizes);
	
	for(i=0;i<verticesCount;i++){
		CXIndex group = groups.data[i];
		newVerticesIndices.data[i] = groupSizes.data[group];
		groupSizes.data[group]++;
	}
	
	CXIndex largestComponentIndex = 0;
	CXSize largestComponentSize = 0;
	
	for(i=0;i<numGroups;i++){
		if(groupSizes.data[i]>largestComponentSize){
			largestComponentSize = groupSizes.data[i];
			largestComponentIndex = i;
		}
	}
	
	for(i=0;i<verticesCount;i++){
		CXIndex group = groups.data[i];
		if(group==largestComponentIndex){
			CXFloatArrayAdd(i, &largestVerticesList);
			CXUIntegerArrayAdd(i, &verticesIndices);
		}
	}
	
	for(i=0;i<edgesCount;i++){
		CXIndex fromVertex = theNetwork->edgeFromList[i];
		CXIndex toVertex = theNetwork->edgeToList[i];
		
		if(largestComponentIndex==groups.data[fromVertex] && largestComponentIndex==groups.data[toVertex]){
			CXUIntegerArrayAdd(newVerticesIndices.data[fromVertex], &largestEdgesFrom);
			CXUIntegerArrayAdd(newVerticesIndices.data[toVertex], &largestEdgesTo);
		}
		if(theNetwork->edgeWeighted){
			CXFloatArrayAdd(theNetwork->edgesWeights[i], &largestEdgesWeight);
		}
	}
	
	CXSize groupVerticesCount = groupSizes.data[largestComponentIndex];
	CXNetworkRef groupNetwork = CXNewNetwork(groupVerticesCount, theNetwork->edgeWeighted, theNetwork->directed);
	
	if(!theNetwork->edgeWeighted){
		CXNetworkAddNewEdges(groupNetwork, largestEdgesFrom.data, largestEdgesTo.data, NULL, largestEdgesFrom.count);
	}else{
		CXNetworkAddNewEdges(groupNetwork, largestEdgesFrom.data, largestEdgesTo.data, largestEdgesWeight.data, largestEdgesFrom.count);
	}
	
	CXNetworkAppendProperty(groupNetwork, "Original Index", CXNumberPropertyType, largestVerticesList.data);
	
	if(theNetwork->vertexNames){
		CXString* names = calloc(groupVerticesCount, sizeof(CXString));
		CXString* propertyData = theNetwork->vertexNames;
		for (i=0; i<groupVerticesCount; i++) {
			names[i] = propertyData[verticesIndices.data[i]];
		}
		CXNetworkAppendProperty(groupNetwork, "name", CXStringPropertyType, names);
		free(names);
	}
	
	CXIndex propIndex = 0;
	for (propIndex=0; propIndex< theNetwork->propertiesCount; propIndex++) {
		CXPropertyType propertyType = theNetwork->propertiesTypes[propIndex];
		CXString propertyName = theNetwork->propertiesNames[propIndex];
		
		CXIndex i;
		switch (propertyType) {
			case CXStringPropertyType:{
				CXString* values = calloc(groupVerticesCount, sizeof(CXString));
				CXString* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount; i++) {
					values[i] = propertyData[verticesIndices.data[i]];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			case CXNumberPropertyType:{
				CXFloat* values = calloc(groupVerticesCount, sizeof(CXFloat));
				CXFloat* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount; i++) {
					values[i] = propertyData[verticesIndices.data[i]];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			case CXVector2DPropertyType:{
				CXFloat* values = calloc(groupVerticesCount*2, sizeof(CXFloat));
				CXFloat* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount*2; i+=2) {
					values[i] = propertyData[verticesIndices.data[i]*2];
					values[i+1] = propertyData[verticesIndices.data[i]*2+1];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			case CXVector3DPropertyType:{
				CXFloat* values = calloc(groupVerticesCount*3, sizeof(CXFloat));
				CXFloat* propertyData = theNetwork->propertiesData[propIndex];
				for (i=0; i<groupVerticesCount*3; i+=3) {
					values[i] = propertyData[verticesIndices.data[i]*3];
					values[i+1] = propertyData[verticesIndices.data[i]*3+1];
					values[i+2] = propertyData[verticesIndices.data[i]*3+2];
				}
				CXNetworkAppendProperty(groupNetwork, propertyName, propertyType, values);
				free(values);
				break;
			}
			default:
				break;
		}
	}
	
	CXFloatArrayDestroy(&largestVerticesList);
	CXUIntegerArrayDestroy(&largestEdgesFrom);
	CXUIntegerArrayDestroy(&largestEdgesTo);
	
	if(theNetwork->edgeWeighted){
		CXFloatArrayDestroy(&largestEdgesWeight);
	}
	
	CXUIntegerArrayDestroy(&newVerticesIndices);
	CXUIntegerArrayDestroy(&groupSizes);
	
	
	CXUIntegerArrayDestroy(&groups);
	CXUIntegerArrayDestroy(&edgesGroups);
	CXUIntegerArrayDestroy(&vertexList);
	CXUIntegerArrayDestroy(&verticesIndices);
	
	CXBitArrayDestroy(visited);
	CXBitArrayDestroy(inList);
	
	
	return groupNetwork;
}



CXSize CXNetworkLargestComponentSize(const CXNetworkRef theNetwork, CXSize* connectedComponentsCount){
	CXSize verticesCount = theNetwork->verticesCount;
	CXSize edgesCount = theNetwork->edgesCount;
	
	CXUIntegerArray groups;
	CXUIntegerArrayInitWithCapacity(verticesCount, &groups);
	CXUIntegerArraySetCount(verticesCount, &groups);
	
	CXUIntegerArray edgesGroups;
	CXUIntegerArrayInitWithCapacity(edgesCount, &edgesGroups);
	CXUIntegerArraySetCount(edgesCount, &edgesGroups);
	
	CXUIntegerArray vertexList;
	CXUIntegerArrayInitWithCapacity(verticesCount, &vertexList);
	CXUIntegerArraySetCount(verticesCount, &vertexList);
	
	CXBitArray visited = CXNewBitArray(verticesCount);
	CXBitArray inList = CXNewBitArray(verticesCount);
	
	
	CXSize numGroups = 0;
	CXIndex vIndex;
	CXSize largestGroup=0;
	CXSize largestGroupSize=0;
	CXSize currentGroupSize=0;
	
	for(vIndex=0;vIndex<verticesCount;vIndex++){
		if(!CXBitArrayTest(visited, vIndex)){
			numGroups++;
			groups.data[vIndex]=numGroups-1;
			currentGroupSize++;
			CXSize listSize=1;
			CXIndex currentVertex;
			vertexList.data[0]=vIndex;
			CXBitArrayClearAll(inList, verticesCount);
			CXBitArraySet(inList, vIndex);
			while(listSize>0){
				currentVertex=vertexList.data[listSize-1];
				if(!CXBitArrayTest(visited, currentVertex)){
					CXBitArraySet(visited, currentVertex);
					
					CXSize edgesCount = theNetwork->vertexNumOfEdges[currentVertex];
					CXIndex* verticesList = theNetwork->vertexEdgesLists[currentVertex];
					CXIndex* edgesIndices = theNetwork->vertexEdgesIndices[currentVertex];
					
					CXIndex edgeIndex;
					
					CXBool addedToGroup = CXFalse;
					for(edgeIndex=0;edgeIndex<edgesCount;edgeIndex++){
						CXIndex linkedVertex = verticesList[edgeIndex];
						
						edgesGroups.data[edgesIndices[edgeIndex]] = numGroups-1;
						
						if(!CXBitArrayTest(inList,linkedVertex) && !CXBitArrayTest(visited,linkedVertex)){
							vertexList.data[listSize]=linkedVertex;
							listSize++;
							
							CXBitArraySet(inList, linkedVertex);
							
							groups.data[linkedVertex]=numGroups-1;
							currentGroupSize++;
							
							addedToGroup = CXTrue;
						}
					}
					if(theNetwork->directed){
						CXSize edgesCount = theNetwork->vertexNumOfInEdges[currentVertex];
						CXIndex* verticesList = theNetwork->vertexInEdgesLists[currentVertex];
						CXIndex* edgesIndices = theNetwork->vertexInEdgesIndices[currentVertex];
						
						CXIndex edgeIndex;
						//NSLog(@" Links from node %d listsize %d",currentVertex,listSize);
						for(edgeIndex=0;edgeIndex<edgesCount;edgeIndex++){
							CXIndex linkedVertex = verticesList[edgeIndex];
							
							edgesGroups.data[edgesIndices[edgeIndex]] = numGroups-1;
							
							if(!CXBitArrayTest(inList,linkedVertex) && !CXBitArrayTest(visited,linkedVertex)){
								vertexList.data[listSize]=linkedVertex;
								listSize++;
								
								CXBitArraySet(inList, linkedVertex);
								
								groups.data[linkedVertex]=numGroups-1;
								currentGroupSize++;
								
								addedToGroup = CXTrue;
							}
						}
					}
					if(!addedToGroup){
						CXBitArrayClear(inList, vertexList.data[listSize-1]);
						listSize--;
					}
				}else{
					CXBitArrayClear(inList, vertexList.data[listSize-1]);
					listSize--;
				}
			}
			if(currentGroupSize>largestGroupSize){
				largestGroupSize=currentGroupSize;
				largestGroup=numGroups-1;
			}
		}
	}
	
	CXFloatArray largestVerticesList;
	CXUIntegerArray largestEdgesFrom;
	CXUIntegerArray largestEdgesTo;
	CXFloatArray largestEdgesWeight;
	
	CXFloatArrayInitWithCapacity(10, &largestVerticesList);
	CXUIntegerArrayInitWithCapacity(10, &largestEdgesFrom);
	CXUIntegerArrayInitWithCapacity(10, &largestEdgesTo);
	
	if(theNetwork->edgeWeighted){
		CXFloatArrayInitWithCapacity(10, &largestEdgesWeight);
	}
	
	
	CXIndex i;
	CXUIntegerArray groupSizes;
	CXUIntegerArrayInitWithCapacity(numGroups, &groupSizes);
	CXUIntegerArraySetCount(numGroups, &groupSizes);
	
	for(i=0;i<verticesCount;i++){
		CXIndex group = groups.data[i];
		groupSizes.data[group]++;
	}
	
	CXIndex largestComponentIndex = 0;
	CXSize largestComponentSize = 0;
	
	for(i=0;i<numGroups;i++){
		if(groupSizes.data[i]>largestComponentSize){
			largestComponentSize = groupSizes.data[i];
			largestComponentIndex = i;
		}
	}
	
	CXUIntegerArrayDestroy(&groupSizes);
	
	
	CXUIntegerArrayDestroy(&groups);
	CXUIntegerArrayDestroy(&edgesGroups);
	CXUIntegerArrayDestroy(&vertexList);
	
	CXBitArrayDestroy(visited);
	CXBitArrayDestroy(inList);
	
	if(connectedComponentsCount){
		*connectedComponentsCount = numGroups;
	}
	
	return largestComponentSize;
}


CXNetworkRef CXNewNetworkFromAdjacencyMatrix(const CXBitArray adjacencyMatrix, CXSize verticesCount, CXBool directed){
	CXSize edgesCapacity = 2;
	CXIndex* fromList = calloc(edgesCapacity, sizeof(CXIndex));
	CXIndex* toList = calloc(edgesCapacity, sizeof(CXIndex));
	CXSize edgesCount = 0;
	
	for (CXIndex fromIndex=0; fromIndex<verticesCount; fromIndex++) {
		CXSize startAt = directed?0:(fromIndex+1);
		for (CXIndex toIndex=startAt; toIndex<verticesCount; toIndex++) {
			if(CXBitArrayTest(adjacencyMatrix, fromIndex*verticesCount+toIndex)){
				if(edgesCapacity<edgesCount+1){
					edgesCapacity = CXCapacityGrow(edgesCount+1);
					fromList = realloc(fromList, sizeof(CXIndex)*edgesCapacity);
					toList = realloc(toList, sizeof(CXIndex)*edgesCapacity);
				}
				fromList[edgesCount] = fromIndex;
				toList[edgesCount] = toIndex;
				edgesCount++;
			}
		}
	}
	
	CXNetworkRef theNetwork = CXNewNetwork(verticesCount, CXFalse, directed);
	CXNetworkAddNewEdges(theNetwork, fromList, toList, NULL, edgesCount);
	free(fromList);
	free(toList);
	return theNetwork;
}


CXBitArray CXNewAdjacencyMatrixFromNetwork(const CXNetworkRef theNetwork){
	CXSize verticesCount = theNetwork->verticesCount;
	CXBitArray adjacencyMatrix = CXNewBitArray(verticesCount*verticesCount);
	CXIndex* fromList = theNetwork->edgeFromList;
	CXIndex* toList = theNetwork->edgeToList;
	
	for (CXIndex edgeIndex=0; edgeIndex<theNetwork->edgesCount; edgeIndex++) {
		CXBitArraySet(adjacencyMatrix, fromList[edgeIndex]*verticesCount+toList[edgeIndex]);
		if(!theNetwork->directed){
			CXBitArraySet(adjacencyMatrix, toList[edgeIndex]*verticesCount+fromList[edgeIndex]);
		}
	}
	return adjacencyMatrix;
}




CXSize CXNetworkVerticesCount(const CXNetworkRef theNetwork){
	return theNetwork->verticesCount;
}

CXSize CXNetworkEdgesCount(const CXNetworkRef theNetwork){
	return theNetwork->edgesCount;
}

CXSize CXNetworkVertexDegree(const CXNetworkRef theNetwork, CXIndex vertexIndex){
	return theNetwork->vertexNumOfEdges[vertexIndex];
}

CXSize CXNetworkVertexInDegree(const CXNetworkRef theNetwork, CXIndex vertexIndex){
	return theNetwork->vertexNumOfInEdges[vertexIndex];
}

CXSize CXNetworkVertexNumberOfEdges(const CXNetworkRef theNetwork, CXIndex vertexIndex){
	return theNetwork->vertexNumOfEdges[vertexIndex];
}

CXSize CXNetworkVertexNumberOfInEdges(const CXNetworkRef theNetwork, CXIndex vertexIndex){
	return theNetwork->vertexNumOfInEdges[vertexIndex];
}

CXIndex CXNetworkVertexEdgeAtIndex(const CXNetworkRef theNetwork, CXIndex vertexIndex, CXIndex vertexEdgeIndex){
	return theNetwork->vertexEdgesLists[vertexIndex][vertexEdgeIndex];
}

CXIndex CXNetworkVertexInEdgeAtIndex(const CXNetworkRef theNetwork, CXIndex vertexIndex, CXIndex vertexedgeIndex){
	return theNetwork->vertexInEdgesLists[vertexIndex][vertexedgeIndex];
}




