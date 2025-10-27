//
//  CXSimpleCXQueue.m
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/23/12.
//  Copyright (c) 2012 Filipi Nascimento Silva. All rights reserved.
//

#include "CXSimpleQueue.h"


CX_INLINE CXQueueNode* getNodeFromPool(CXQueue* queue){
	if(queue->nodesPoolCount>0){
		queue->nodesPoolCount--;
		return queue->nodesPool[queue->nodesPoolCount];
	}else{
		return (CXQueueNode*)malloc(sizeof(CXQueueNode));
	}
}

CX_INLINE void putNodeInPool(CXQueue* queue,CXQueueNode* node){
	queue->nodesPoolCount++;
	if(CXUnlikely(queue->nodesPoolCapacity < queue->nodesPoolCount)){
		queue->nodesPoolCapacity = CXCapacityGrow(queue->nodesPoolCount);
		queue->nodesPool = realloc(queue->nodesPool, sizeof(CXQueueNode*)*queue->nodesPoolCapacity);
	}
	queue->nodesPool[queue->nodesPoolCount-1] = node;
}



void CXQueuePush (CXQueue* queue, CXInteger item) {
	// Create a new node
	CXQueueNode* n = getNodeFromPool(queue);
	n->item = item;
	n->next = NULL;
	
	if (queue->head == NULL) { // no head
		queue->head = n;
	} else{
		queue->tail->next = n;
	}
	queue->tail = n;
	queue->size++;
}

CXInteger CXQueuePop (CXQueue* queue) {
    // get the first item
	CXQueueNode* head = queue->head;
	CXInteger item = head->item;
	// move head pointer to next node, decrease size
	queue->head = head->next;
	queue->size--;
	// free the memory of original head
	free(head);
	return item;
}



CXBool CXQueueDequeue (CXQueue* queue,CXInteger *value) {
    // get the first item
	if (queue->size>0) {
		CXQueueNode* head = queue->head;
		CXInteger item = head->item;
		// move head poCXIntegerer to next node, decrease size
		queue->head = head->next;
		queue->size--;
		// free the memory of original head
		putNodeInPool(queue,head);
		*value = item;
		return CXTrue;
	}else{
		return CXFalse;
	}
}


CXInteger CXQueuePeek (CXQueue* queue) {
	CXQueueNode* head = queue->head;
	return head->item;
}


CXQueue CXQueueCreate () {
	CXQueue queue;
	queue.size = 0;
	queue.head = NULL;
	queue.tail = NULL;
	queue.push = &CXQueuePush;
	queue.pop = &CXQueuePop;
	queue.peek = &CXQueuePeek;
	queue.nodesPool = calloc(2, sizeof(CXQueueNode*));
	queue.nodesPoolCapacity = 2;
	queue.nodesPoolCount = 0;
	return queue;
}

void CXQueueDestroy (CXQueue* queue) {
	CXInteger value;
	while(CXQueueDequeue(queue,&value)){};
	CXIndex i;
	for (i=0; i<queue->nodesPoolCount; i++) {
		free(queue->nodesPool[i]);
	}
	free(queue->nodesPool);
}
