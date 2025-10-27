//
//  CXSimpleCXQueue.m
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/23/12.
//  Copyright (c) 2012 Filipi Nascimento Silva. All rights reserved.
//

#include "CXSimpleQueue.h"


/** Returns an available node, reusing entries from the pool when possible. */
CX_INLINE CXQueueNode* getNodeFromPool(CXQueue* queue){
	if(queue->nodesPoolCount>0){
		queue->nodesPoolCount--;
		return queue->nodesPool[queue->nodesPoolCount];
	}else{
		return (CXQueueNode*)malloc(sizeof(CXQueueNode));
	}
}

/** Returns a node to the pool, growing the pool array when necessary. */
CX_INLINE void putNodeInPool(CXQueue* queue,CXQueueNode* node){
	queue->nodesPoolCount++;
	if(CXUnlikely(queue->nodesPoolCapacity < queue->nodesPoolCount)){
		queue->nodesPoolCapacity = CXCapacityGrow(queue->nodesPoolCount);
		queue->nodesPool = realloc(queue->nodesPool, sizeof(CXQueueNode*)*queue->nodesPoolCapacity);
	}
	queue->nodesPool[queue->nodesPoolCount-1] = node;
}



/** Enqueues an item at the tail of the queue. */
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

/** Removes and returns the head item. Caller must ensure the queue is non-empty. */
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



/** Attempts to dequeue into `value`, using the node pool for reuse. */
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


/** Returns the head item without removing it. */
CXInteger CXQueuePeek (CXQueue* queue) {
	CXQueueNode* head = queue->head;
	return head->item;
}


/** Constructs a queue with pre-wired function pointers and node pool. */
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

/** Empties the queue and releases all pooled nodes. */
void CXQueueDestroy (CXQueue* queue) {
	CXInteger value;
	while(CXQueueDequeue(queue,&value)){};
	CXIndex i;
	for (i=0; i<queue->nodesPoolCount; i++) {
		free(queue->nodesPool[i]);
	}
	free(queue->nodesPool);
}
