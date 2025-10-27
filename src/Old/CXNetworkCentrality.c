//
//  CXNetworkCentrality.c
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 8/27/13.
//  Copyright (c) 2013 Filipi Nascimento Silva. All rights reserved.
//

#include "CXNetworkCentrality.h"
#include "fib/fib.h"
#include <float.h>


#if CX_ENABLE_PARALLELISM

CXBool CXNetworkCalculateCentrality_weighted_parallel_implementation(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CXSize verticesCount = network->verticesCount;
	
	CXSize unrolledLoops = kCXDefaultParallelBlocks;
	CXInteger* currentProgress = NULL;
	void (*updateCallback)(CXOperationControl*)  = NULL;
	if(operationControl){
		operationControl->maxProgress = verticesCount;
		operationControl->currentProgress = 0;
		currentProgress = &(operationControl->currentProgress);
		if(operationControl->maxParallelBlocks>0){
			unrolledLoops = operationControl->maxParallelBlocks;
		}
		updateCallback = operationControl->updateCallback;
	}
	
	
	CXFloatArrayReallocToCapacity(verticesCount, centrality);
	CXFloatArraySetCount(verticesCount, centrality);
		//memset(centrality->data, 0, verticesCount*sizeof(CXFloat));
	CXIndex i;
	for (i=0; i<verticesCount; i++) {
		if(network->verticesEnabled[i]){
			centrality->data[i]  = 0.0f;
		}
	}
	CXSize unrolledSize = 1 + ((verticesCount - 1) / unrolledLoops);
	
	CXParallelForStart(centralityLoop, blockIndex, unrolledLoops){
		
		const CXFloat* verticesWeights = network->verticesWeights;
		const CXBool* verticesEnabled = network->verticesEnabled;
		
		CXUIntegerArray* P = calloc(verticesCount, sizeof(CXUIntegerArray));
		double* sigma = calloc(verticesCount, sizeof(double));
		double* d = calloc(verticesCount, sizeof(double));
		double* delta = calloc(verticesCount, sizeof(double));
		
		double* localCentrality = calloc(verticesCount, sizeof(double));
		
		double* seen = calloc(verticesCount, sizeof(double));
		
		CXIndex i;
		for (i=0; i<verticesCount; i++) {
			CXUIntegerArray newArray;
			CXUIntegerArrayInitWithCapacity(1, &newArray);
			P[i] = newArray;
		}
		
		CXIndex s;
		CXIntegerStack S = CXIntegerStackMake();
		struct fibheap* Q = fh_makekeyheap();
		
		CXSize maxIndex = CXMIN((blockIndex+1)*unrolledSize, verticesCount);
		for(s=blockIndex*unrolledSize;s<maxIndex;s++){
			if(currentProgress){
				CXAtomicIncrementInteger(currentProgress);
				if(updateCallback){
					updateCallback(operationControl);
				}
			}
			if(CXUnlikely(!verticesEnabled[s])){
				continue;
			}
			CXFloat sWeight = verticesWeights[s];
			S.count = 0;
			for (i=0; i<verticesCount; i++) {
				P[i].count=0;
				d[i] = -1.0;
				sigma[i] = 0.0;
				delta[i] = 0.0;
				seen[i] = -1.0;
			}
			sigma[s] = 1.0f;
			seen[s] = 0.0f;
			CXInteger v;
			CXInteger prev;
			fh_data sdata;
			sdata.data = s;
			sdata.prev = s;
			fh_insertkey(Q,0.0,sdata);
			
			fh_data vdata;
			double dist;
			while (fh_dequeue(Q,&vdata,&dist)) {
				v = vdata.data;
				prev = vdata.prev;
				
				if(d[v]!=-1.0){
					continue;
				}
				sigma[v]+=sigma[prev];
				CXIntegerStackPush(v, &S);
				d[v] = dist;
				
				CXSize vEdgesCount = network->vertexNumOfEdges[v];
				CXIndex* vNeighbor = network->vertexEdgesLists[v];
				CXIndex* vEdge = network->vertexEdgesIndices[v];
				CXFloat* edgesWeights = network->edgesWeights;
				CXIndex w,e;
				for (e=0; e<vEdgesCount; e++) {
					w = vNeighbor[e];
					double weight = exp(-edgesWeights[vEdge[e]]);
					if(CXLikely(verticesEnabled[w])){
						double vwdist = d[v]+weight;
						if(d[w]<0.0 && ( seen[w]<0.0 || vwdist < seen[w])){
							seen[w]= vwdist;
							fh_data wdata;
							wdata.data = w;
							wdata.prev = v;
							fh_insertkey(Q, vwdist, wdata);
							sigma[w] = 0.0;
							P[w].count = 0;
							CXUIntegerArrayAdd(v, P+w);
						}else{
							if(vwdist==seen[w]){
								sigma[w]+= sigma[v];
								CXUIntegerArrayAdd(v, P+w);
							}
						}
					}
				}
			}
			while(S.count>0){
				CXIndex w = CXIntegerStackPop(&S);
				CXIndex v,p;
				CXSize PCount = P[w].count;
				CXUInteger* PData = P[w].data;
				for (p=0; p<PCount; p++) {
					v = PData[p];
					delta[v] += sigma[v]/sigma[w]*(1.0+delta[w]);
				}
				if(w!=s){
					localCentrality[w] += sWeight*delta[w];
				}
			}
		}
		for (i=0; i<verticesCount; i++){
			CXUIntegerArrayDestroy(P+i);
		}
		free(P);
		free(sigma);
		free(d);
		free(delta);
		free(seen);
		CXIntegerArrayDestroy(&S);
		fh_deleteheap(Q);
		
		CXParallelLoopCriticalRegionStart(centralityLoop){
			CXFloat* centralityData = centrality->data;
			for (i=0; i<verticesCount; i++) {
				centralityData[i] += localCentrality[i];
			}
		}
		CXParallelLoopCriticalRegionEnd(centralityLoop);
		free(localCentrality);
	}CXParallelForEnd(centralityLoop);
	
	return CXTrue;
}

#endif //CX_ENABLE_PARALLELISM



CXBool CXNetworkCalculateCentrality_weighted_implementation(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CXSize verticesCount = network->verticesCount;
	
	CXInteger* currentProgress = NULL;
	void (*updateCallback)(CXOperationControl*)  = NULL;
	if(operationControl){
		operationControl->maxProgress = verticesCount;
		operationControl->currentProgress = 0;
		currentProgress = &(operationControl->currentProgress);
		updateCallback = operationControl->updateCallback;
	}
	
	CXFloatArrayReallocToCapacity(verticesCount, centrality);
	CXFloatArraySetCount(verticesCount, centrality);
		//memset(centrality->data, 0, verticesCount*sizeof(CXFloat));
	CXIndex i;
	for (i=0; i<verticesCount; i++) {
		if(network->verticesEnabled[i]){
			centrality->data[i]  = 0.0;
		}
	}
	
	const CXFloat* verticesWeights = network->verticesWeights;
	const CXBool* verticesEnabled = network->verticesEnabled;
	
	CXFloat* centralityData = centrality->data;
	
	CXUIntegerArray* P = calloc(verticesCount, sizeof(CXUIntegerArray));
	double* sigma = calloc(verticesCount, sizeof(double));
	double* d = calloc(verticesCount, sizeof(double));
	double* delta = calloc(verticesCount, sizeof(double));
	double* seen = calloc(verticesCount, sizeof(double));
	
	for (i=0; i<verticesCount; i++) {
		CXUIntegerArray newArray;
		CXUIntegerArrayInitWithCapacity(1, &newArray);
		P[i] = newArray;
	}
	CXIndex s;
	CXIntegerStack S = CXIntegerStackMake();
	struct fibheap* Q = fh_makekeyheap();
	
	
	for (s=0; s<verticesCount; s++) {
		if(currentProgress){
			CXAtomicIncrementInteger(currentProgress);
			if(updateCallback){
				updateCallback(operationControl);
			}
		}
		if(CXUnlikely(!verticesEnabled[s])){
			continue;
		}
		CXFloat sWeight = verticesWeights[s];
			//printf("%ld/%ld\n",s,verticesCount);
		S.count = 0;
		for (i=0; i<verticesCount; i++) {
			P[i].count=0;
			d[i] = -1.0;
			sigma[i] = 0.0;
			delta[i] = 0.0;
			seen[i] = -1.0;
		}
		sigma[s] = 1.0f;
		seen[s] = 0.0f;
		CXInteger v;
		CXInteger prev;
		fh_data sdata;
		sdata.data = s;
		sdata.prev = s;
		fh_insertkey(Q,0.0,sdata);
		
		fh_data vdata;
		double dist;
		while (fh_dequeue(Q,&vdata,&dist)) {
			v = vdata.data;
			prev = vdata.prev;
			
			if(d[v]!=-1.0){
				continue;
			}
			sigma[v]+=sigma[prev];
			CXIntegerStackPush(v, &S);
			d[v] = dist;
			
			CXSize vEdgesCount = network->vertexNumOfEdges[v];
			CXIndex* vNeighbor = network->vertexEdgesLists[v];
			CXIndex* vEdge = network->vertexEdgesIndices[v];
			CXFloat* edgesWeights = network->edgesWeights;
			CXIndex w,e;
			for (e=0; e<vEdgesCount; e++) {
				w = vNeighbor[e];
				// double weight = exp(-edgesWeights[vEdge[e]]);
				double weight = 1.0/edgesWeights[vEdge[e]];
				if(CXLikely(verticesEnabled[w])){
					double vwdist = d[v]+weight;
					if(d[w]<0.0 && ( seen[w]<0.0 || vwdist < seen[w])){
						seen[w]= vwdist;
						fh_data wdata;
						wdata.data = w;
						wdata.prev = v;
						fh_insertkey(Q, vwdist, wdata);
						sigma[w] = 0.0;
						P[w].count = 0;
						CXUIntegerArrayAdd(v, P+w);
					}else{
						if(vwdist==seen[w]){
							sigma[w]+= sigma[v];
							CXUIntegerArrayAdd(v, P+w);
						}
					}
				}
			}
		}
		while(S.count>0){
			CXIndex w = CXIntegerStackPop(&S);
			CXIndex v,p;
			CXSize PCount = P[w].count;
			CXUInteger* PData = P[w].data;
			for (p=0; p<PCount; p++) {
				v = PData[p];
				delta[v] += sigma[v]/sigma[w]*(1.0+delta[w]);
					//if(v==2538){
						//printf("delta[v]: %f  sigma[v]: %f  sigma[w]: %f delta[w]: %f  s:%"CXIndexScan"  v:%"CXIndexScan"  w:%"CXIndexScan"\n",delta[v],sigma[v],sigma[w],delta[w], s,v,w);
						//}
			}
			if(w!=s){
				centralityData[w] += sWeight*delta[w];
					//if(w==2538&&s<=338){
					//	printf("sWeight: %f  delta[w]: %f  centralityData[w]: %f  s:%"CXIndexScan"\n",sWeight,delta[w],centralityData[w], s);
					//}
			}
		}
	}
		//CXQueueDestroy(&Q);
	CXIntegerArrayDestroy(&S);
	free(P);
	free(sigma);
	free(d);
	free(delta);
	free(seen);
	
	fh_deleteheap(Q);
	
	/*
	 printf("----\nFinished\n-----\n");
	 CXSize N = network->verticesCount;
	 for (i=0; i<verticesCount; i++) {
	 printf("%g\n",centralityData[i]/(N-1)/(N-2));
	 }
	 */
	return CXTrue;
}







#if CX_ENABLE_PARALLELISM
CXBool CXNetworkCalculateCentrality_parallel_implementation(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CXSize verticesCount = network->verticesCount;
	
	CXSize unrolledLoops = kCXDefaultParallelBlocks;
	CXInteger* currentProgress = NULL;
	void (*updateCallback)(CXOperationControl*)  = NULL;
	if(operationControl){
		operationControl->maxProgress = verticesCount;
		operationControl->currentProgress = 0;
		currentProgress = &(operationControl->currentProgress);
		if(operationControl->maxParallelBlocks>0){
			unrolledLoops = operationControl->maxParallelBlocks;
		}
		updateCallback = operationControl->updateCallback;
	}
	
	
	CXFloatArrayReallocToCapacity(verticesCount, centrality);
	CXFloatArraySetCount(verticesCount, centrality);
		//memset(centrality->data, 0, verticesCount*sizeof(CXFloat));
	CXIndex i;
	for (i=0; i<verticesCount; i++) {
		if(network->verticesEnabled[i]){
			centrality->data[i]  = 0.0f;
		}
	}
	CXSize unrolledSize = 1 + ((verticesCount - 1) / unrolledLoops);
	
	CXParallelForStart(centralityLoop, blockIndex, unrolledLoops){
		
		
		const CXFloat* verticesWeights = network->verticesWeights;
		const CXBool* verticesEnabled = network->verticesEnabled;
		
		CXUIntegerArray* P = calloc(verticesCount, sizeof(CXUIntegerArray));
		CXInteger* sigma = calloc(verticesCount, sizeof(CXInteger));
		CXInteger* d = calloc(verticesCount, sizeof(CXInteger));
		double* delta = calloc(verticesCount, sizeof(double));
		
		double* localCentrality = calloc(verticesCount, sizeof(double));
		
		CXIndex i;
		for (i=0; i<verticesCount; i++) {
			CXUIntegerArray newArray;
			CXUIntegerArrayInitWithCapacity(1, &newArray);
			P[i] = newArray;
		}
		
		CXIndex s;
		CXIntegerStack S = CXIntegerStackMake();
		CXQueue Q = CXQueueCreate();
		
		CXSize maxIndex = CXMIN((blockIndex+1)*unrolledSize, verticesCount);
		for(s=blockIndex*unrolledSize;s<maxIndex;s++){
			if(currentProgress){
				CXAtomicIncrementInteger(currentProgress);
				if(updateCallback){
					updateCallback(operationControl);
				}
			}
			if(CXUnlikely(!verticesEnabled[s])){
				continue;
			}
			CXFloat sWeight = verticesWeights[s];
			S.count = 0;
			for (i=0; i<verticesCount; i++) {
				P[i].count=0;
				d[i] = CXIntegerMAX;
				sigma[i] = 0;
				delta[i] = 0;
			}
			sigma[s] = 1;
			d[s] = 0;
			CXInteger v;
			CXQueuePush(&Q, s);
			while (CXQueueDequeue(&Q,&v)) {
				CXIntegerStackPush(v, &S);
				CXSize vEdgesCount = network->vertexNumOfEdges[v];
				CXIndex* vNeighbor = network->vertexEdgesLists[v];
				CXIndex w,e;
				for (e=0; e<vEdgesCount; e++) {
					w = vNeighbor[e];
					if(CXLikely(verticesEnabled[w])){
						if(d[w]==CXIntegerMAX){
							d[w] = d[v] + 1;// FIXME: Change to the w-v weight;
								//fh_insertkey(Q, d[v], w);
								//if(d[w]<=2){
							CXQueuePush(&Q, w);
								//}
						}
						if(d[w]== d[v]+1){
							sigma[w]+= sigma[v];
							CXUIntegerArrayAdd(v, &(P[w]));
						}
					}
				}
			}
			while(S.count>0){
				CXIndex w = CXIntegerStackPop(&S);
				CXIndex v,p;
				CXSize PCount = P[w].count;
				CXUInteger* PData = P[w].data;
				for (p=0; p<PCount; p++) {
					v = PData[p];
					delta[v] += sigma[v]/(double)sigma[w]*(1.0+delta[w]);
				}
				if(w!=s){
					localCentrality[w] += sWeight*delta[w];
				}
			}
		}
		for (i=0; i<verticesCount; i++){
			CXUIntegerArrayDestroy(P+i);
		}
		free(P);
		free(sigma);
		free(d);
		free(delta);
		CXIntegerArrayDestroy(&S);
		CXQueueDestroy(&Q);
		
		CXParallelLoopCriticalRegionStart(centralityLoop){
			CXFloat* centralityData = centrality->data;
			for (i=0; i<verticesCount; i++) {
				centralityData[i] += localCentrality[i];
			}
		}
		CXParallelLoopCriticalRegionEnd(centralityLoop);
		free(localCentrality);
		
	}CXParallelForEnd(centralityLoop);
	
	return CXTrue;
}

#endif //CX_ENABLE_PARALLELISM



CXBool CXNetworkCalculateCentrality_implementation(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CXSize verticesCount = network->verticesCount;
	
	CXInteger* currentProgress = NULL;
	void (*updateCallback)(CXOperationControl*)  = NULL;
	if(operationControl){
		operationControl->maxProgress = verticesCount;
		operationControl->currentProgress = 0;
		currentProgress = &(operationControl->currentProgress);
		updateCallback = operationControl->updateCallback;
	}
	
	CXFloatArrayReallocToCapacity(verticesCount, centrality);
	CXFloatArraySetCount(verticesCount, centrality);
		//memset(centrality->data, 0, verticesCount*sizeof(CXFloat));
	CXIndex i;
	for (i=0; i<verticesCount; i++) {
		if(network->verticesEnabled[i]){
			centrality->data[i]  = 0.0;
		}
	}
	
	const CXFloat* verticesWeights = network->verticesWeights;
	const CXBool* verticesEnabled = network->verticesEnabled;
	
	CXFloat* centralityData = centrality->data;
	
	CXUIntegerArray* P = calloc(verticesCount, sizeof(CXUIntegerArray));
	CXInteger* sigma = calloc(verticesCount, sizeof(CXInteger));
	CXInteger* d = calloc(verticesCount, sizeof(CXInteger));
	double* delta = calloc(verticesCount, sizeof(double));
	for (i=0; i<verticesCount; i++) {
		CXUIntegerArray newArray;
		CXUIntegerArrayInitWithCapacity(1, &newArray);
		P[i] = newArray;
	}
	CXIndex s;
	CXIntegerStack S = CXIntegerStackMake();
	CXQueue Q = CXQueueCreate();
	
	for (s=0; s<verticesCount; s++) {
		if(currentProgress){
			CXAtomicIncrementInteger(currentProgress);
			if(updateCallback){
				updateCallback(operationControl);
			}
		}
		if(CXUnlikely(!verticesEnabled[s])){
			continue;
		}
		CXFloat sWeight = verticesWeights[s];
			//printf("%ld/%ld\n",s,verticesCount);
		S.count = 0;
		for (i=0; i<verticesCount; i++) {
			P[i].count=0;
			d[i] = CXIntegerMAX;
			sigma[i] = 0;
			delta[i] = 0;
		}
		sigma[s] = 1;
		d[s] = 0;
		CXInteger v;
		CXQueuePush(&Q, s);
		while (CXQueueDequeue(&Q,&v)) {
			CXIntegerStackPush(v, &S);
			CXSize vEdgesCount = network->vertexNumOfEdges[v];
			CXIndex* vNeighbor = network->vertexEdgesLists[v];
			CXIndex w,e;
			for (e=0; e<vEdgesCount; e++) {
				w = vNeighbor[e];
				if(CXLikely(verticesEnabled[w])){
					if(d[w]==CXIntegerMAX){
						d[w]= d[v] + 1;// FIXME: Change to the w-v weight;
						CXQueuePush(&Q, w);
					}
					if(d[w] == d[v] + 1){
						sigma[w]+= sigma[v];
						CXUIntegerArrayAdd(v, P+w);
						
					}
				}
			}
		}
		while(S.count>0){
			CXIndex w = CXIntegerStackPop(&S);
			CXIndex v,p;
			CXSize PCount = P[w].count;
			CXUInteger* PData = P[w].data;
			for (p=0; p<PCount; p++) {
				v = PData[p];
				delta[v] += sigma[v]/(double)sigma[w]*(1.0+delta[w]);
			}
			if(w!=s){
				centralityData[w] += sWeight*delta[w];
			}
		}
	}
	CXQueueDestroy(&Q);
	CXIntegerArrayDestroy(&S);
	free(P);
	free(sigma);
	free(d);
	free(delta);
	
	
	/*
	 printf("----\nFinished\n-----\n");
	 CXSize N = network->verticesCount;
	 for (i=0; i<verticesCount; i++) {
	 printf("%g\n",centralityData[i]/(N-1)/(N-2));
	 }
	 */
	return CXTrue;
}


CXBool CXNetworkCalculateCentrality(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CX_BenchmarkPrepare(CXNetworkCalculateCentrality);
	CX_BenchmarkStart(CXNetworkCalculateCentrality);
	
	CXBool returnValue;
	
#if CX_ENABLE_PARALLELISM
	CXInteger maxParallelBlocksCount = kCXDefaultParallelBlocks;
	CXSize problemSize = network->verticesCount;
	if(operationControl){
		maxParallelBlocksCount = operationControl->maxParallelBlocks;
	}
	
	if(network->edgeWeighted){
		if(network&&problemSize>=128&&maxParallelBlocksCount>1){
			returnValue = CXNetworkCalculateCentrality_weighted_parallel_implementation(network, centrality, operationControl);
		}else{
			returnValue = CXNetworkCalculateCentrality_weighted_implementation(network, centrality, operationControl);
		}
	}else{
		if(network&&problemSize>=128&&maxParallelBlocksCount>1){
			returnValue = CXNetworkCalculateCentrality_parallel_implementation(network, centrality, operationControl);
		}else{
			returnValue = CXNetworkCalculateCentrality_implementation(network, centrality, operationControl);
		}
	}
#else
	
	if(network->edgeWeighted){
		returnValue = CXNetworkCalculateCentrality_weighted_implementation(network, centrality, operationControl);
	}else{
		returnValue = CXNetworkCalculateCentrality_implementation(network, centrality, operationControl);
	}
#endif //CX_ENABLE_PARALLELISM
	
	CX_BenchmarkStop(CXNetworkCalculateCentrality);
	CX_BenchmarkPrint(CXNetworkCalculateCentrality);
	return returnValue;
}



























//////Stress




#if CX_ENABLE_PARALLELISM
CXBool CXNetworkCalculateStressCentrality_parallel_implementation(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CXSize verticesCount = network->verticesCount;
	
	CXSize unrolledLoops = kCXDefaultParallelBlocks;
	CXInteger* currentProgress = NULL;
	void (*updateCallback)(CXOperationControl*)  = NULL;
	if(operationControl){
		operationControl->maxProgress = verticesCount;
		operationControl->currentProgress = 0;
		currentProgress = &(operationControl->currentProgress);
		if(operationControl->maxParallelBlocks>0){
			unrolledLoops = operationControl->maxParallelBlocks;
		}
		updateCallback = operationControl->updateCallback;
	}
	
	
	CXFloatArrayReallocToCapacity(verticesCount, centrality);
	CXFloatArraySetCount(verticesCount, centrality);
	//memset(centrality->data, 0, verticesCount*sizeof(CXFloat));
	CXIndex i;
	for (i=0; i<verticesCount; i++) {
		if(network->verticesEnabled[i]){
			centrality->data[i]  = 0.0f;
		}
	}
	CXSize unrolledSize = 1 + ((verticesCount - 1) / unrolledLoops);
	
	CXParallelForStart(centralityLoop, blockIndex, unrolledLoops){
		
		
		const CXFloat* verticesWeights = network->verticesWeights;
		const CXBool* verticesEnabled = network->verticesEnabled;
		
		CXUIntegerArray* P = calloc(verticesCount, sizeof(CXUIntegerArray));
		CXInteger* sigma = calloc(verticesCount, sizeof(CXInteger));
		CXInteger* d = calloc(verticesCount, sizeof(CXInteger));
		double* delta = calloc(verticesCount, sizeof(double));
		
		double* localCentrality = calloc(verticesCount, sizeof(double));
		
		CXIndex i;
		for (i=0; i<verticesCount; i++) {
			CXUIntegerArray newArray;
			CXUIntegerArrayInitWithCapacity(1, &newArray);
			P[i] = newArray;
		}
		
		CXIndex s;
		CXIntegerStack S = CXIntegerStackMake();
		CXQueue Q = CXQueueCreate();
		
		CXSize maxIndex = CXMIN((blockIndex+1)*unrolledSize, verticesCount);
		for(s=blockIndex*unrolledSize;s<maxIndex;s++){
			if(currentProgress){
				CXAtomicIncrementInteger(currentProgress);
				if(updateCallback){
					updateCallback(operationControl);
				}
			}
			if(CXUnlikely(!verticesEnabled[s])){
				continue;
			}
			CXFloat sWeight = verticesWeights[s];
			S.count = 0;
			for (i=0; i<verticesCount; i++) {
				P[i].count=0;
				d[i] = CXIntegerMAX;
				sigma[i] = 0;
				delta[i] = 0;
			}
			sigma[s] = 1;
			d[s] = 0;
			CXInteger v;
			CXQueuePush(&Q, s);
			while (CXQueueDequeue(&Q,&v)) {
				CXIntegerStackPush(v, &S);
				CXSize vEdgesCount = network->vertexNumOfEdges[v];
				CXIndex* vNeighbor = network->vertexEdgesLists[v];
				CXIndex w,e;
				for (e=0; e<vEdgesCount; e++) {
					w = vNeighbor[e];
					if(CXLikely(verticesEnabled[w])){
						if(d[w]==CXIntegerMAX){
							d[w] = d[v] + 1;// FIXME: Change to the w-v weight;
							//fh_insertkey(Q, d[v], w);
							//if(d[w]<=2){
							CXQueuePush(&Q, w);
							//}
						}
						if(d[w]== d[v]+1){
							sigma[w]+= sigma[v];
							CXUIntegerArrayAdd(v, &(P[w]));
						}
					}
				}
			}
			while(S.count>0){
				CXIndex w = CXIntegerStackPop(&S);
				CXIndex v,p;
				CXSize PCount = P[w].count;
				CXUInteger* PData = P[w].data;
				for (p=0; p<PCount; p++) {
					v = PData[p];
					delta[v] += (1.0+delta[w]);
				}
				if(w!=s){
					localCentrality[w] += sigma[w]*sWeight*delta[w];
				}
			}
		}
		for (i=0; i<verticesCount; i++){
			CXUIntegerArrayDestroy(P+i);
		}
		free(P);
		free(sigma);
		free(d);
		free(delta);
		CXIntegerArrayDestroy(&S);
		CXQueueDestroy(&Q);
		
		CXParallelLoopCriticalRegionStart(centralityLoop){
			CXFloat* centralityData = centrality->data;
			for (i=0; i<verticesCount; i++) {
				centralityData[i] += localCentrality[i];
			}
		}
		CXParallelLoopCriticalRegionEnd(centralityLoop);
		free(localCentrality);
		
	}CXParallelForEnd(centralityLoop);
	
	return CXTrue;
}

#endif //CX_ENABLE_PARALLELISM



CXBool CXNetworkCalculateStressCentrality_implementation(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CXSize verticesCount = network->verticesCount;
	
	CXInteger* currentProgress = NULL;
	void (*updateCallback)(CXOperationControl*)  = NULL;
	if(operationControl){
		operationControl->maxProgress = verticesCount;
		operationControl->currentProgress = 0;
		currentProgress = &(operationControl->currentProgress);
		updateCallback = operationControl->updateCallback;
	}
	
	CXFloatArrayReallocToCapacity(verticesCount, centrality);
	CXFloatArraySetCount(verticesCount, centrality);
	//memset(centrality->data, 0, verticesCount*sizeof(CXFloat));
	CXIndex i;
	for (i=0; i<verticesCount; i++) {
		if(network->verticesEnabled[i]){
			centrality->data[i]  = 0.0;
		}
	}
	
	const CXFloat* verticesWeights = network->verticesWeights;
	const CXBool* verticesEnabled = network->verticesEnabled;
	
	CXFloat* centralityData = centrality->data;
	
	CXUIntegerArray* P = calloc(verticesCount, sizeof(CXUIntegerArray));
	CXInteger* sigma = calloc(verticesCount, sizeof(CXInteger));
	CXInteger* d = calloc(verticesCount, sizeof(CXInteger));
	double* delta = calloc(verticesCount, sizeof(double));
	for (i=0; i<verticesCount; i++) {
		CXUIntegerArray newArray;
		CXUIntegerArrayInitWithCapacity(1, &newArray);
		P[i] = newArray;
	}
	CXIndex s;
	CXIntegerStack S = CXIntegerStackMake();
	CXQueue Q = CXQueueCreate();
	
	for (s=0; s<verticesCount; s++) {
		if(currentProgress){
			CXAtomicIncrementInteger(currentProgress);
			if(updateCallback){
				updateCallback(operationControl);
			}
		}
		if(CXUnlikely(!verticesEnabled[s])){
			continue;
		}
		CXFloat sWeight = verticesWeights[s];
		//printf("%ld/%ld\n",s,verticesCount);
		S.count = 0;
		for (i=0; i<verticesCount; i++) {
			P[i].count=0;
			d[i] = CXIntegerMAX;
			sigma[i] = 0;
			delta[i] = 0;
		}
		sigma[s] = 1;
		d[s] = 0;
		CXInteger v;
		CXQueuePush(&Q, s);
		while (CXQueueDequeue(&Q,&v)) {
			CXIntegerStackPush(v, &S);
			CXSize vEdgesCount = network->vertexNumOfEdges[v];
			CXIndex* vNeighbor = network->vertexEdgesLists[v];
			CXIndex w,e;
			for (e=0; e<vEdgesCount; e++) {
				w = vNeighbor[e];
				if(CXLikely(verticesEnabled[w])){
					if(d[w]==CXIntegerMAX){
						d[w]= d[v] + 1;// FIXME: Change to the w-v weight;
						CXQueuePush(&Q, w);
					}
					if(d[w] == d[v] + 1){
						sigma[w]+= sigma[v];
						CXUIntegerArrayAdd(v, P+w);
						
					}
				}
			}
		}
		while(S.count>0){
			CXIndex w = CXIntegerStackPop(&S);
			CXIndex v,p;
			CXSize PCount = P[w].count;
			CXUInteger* PData = P[w].data;
			for (p=0; p<PCount; p++) {
				v = PData[p];
				delta[v] += (1.0+delta[w]);
			}
			if(w!=s){
				centralityData[w] += sigma[w]*sWeight*delta[w];
			}
		}
	}
	CXQueueDestroy(&Q);
	CXIntegerArrayDestroy(&S);
	free(P);
	free(sigma);
	free(d);
	free(delta);
	
	
	/*
	 printf("----\nFinished\n-----\n");
	 CXSize N = network->verticesCount;
	 for (i=0; i<verticesCount; i++) {
	 printf("%g\n",centralityData[i]/(N-1)/(N-2));
	 }
	 */
	return CXTrue;
}


CXBool CXNetworkCalculateStressCentrality(const CXNetwork* network,CXFloatArray* centrality, CXOperationControl* operationControl){
	CX_BenchmarkPrepare(CXNetworkCalculateStressCentrality);
	CX_BenchmarkStart(CXNetworkCalculateStressCentrality);
	
	CXBool returnValue;
	
#if CX_ENABLE_PARALLELISM
	CXInteger maxParallelBlocksCount = kCXDefaultParallelBlocks;
	CXSize problemSize = network->verticesCount;
	if(operationControl){
		maxParallelBlocksCount = operationControl->maxParallelBlocks;
	}
	
	if(network&&problemSize>=128&&maxParallelBlocksCount>1){
		returnValue = CXNetworkCalculateStressCentrality_parallel_implementation(network, centrality, operationControl);
	}else{
		returnValue = CXNetworkCalculateStressCentrality_implementation(network, centrality, operationControl);
	}
#else
	returnValue = CXNetworkCalculateStressCentrality_implementation(network, centrality, operationControl);
#endif //CX_ENABLE_PARALLELISM
	
	CX_BenchmarkStop(CXNetworkCalculateStressCentrality);
	CX_BenchmarkPrint(CXNetworkCalculateStressCentrality);
	return returnValue;
}

