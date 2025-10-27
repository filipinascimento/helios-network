//
//  CXSimpleCXQueue.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/23/12.
//  Copyright (c) 2012 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXNetwork_CXSimpleCXQueue_h
#define CXNetwork_CXSimpleCXQueue_h
#include "CXCommons.h"


typedef struct CXQueueNode {
	CXInteger item;
	struct CXQueueNode* next;
} CXQueueNode;

/** Simple FIFO queue with a recyclable node pool for low-GC execution. */
typedef struct CXQueue {
	CXQueueNode* head;
	CXQueueNode* tail;
	
	void (*push) (struct CXQueue*, CXInteger);
	
	CXInteger (*pop) (struct CXQueue*);
	
	CXInteger (*peek) (struct CXQueue*);
	
	CXInteger size;
	CXQueueNode** nodesPool;
	CXInteger nodesPoolCount;
	CXInteger nodesPoolCapacity;
	
} CXQueue;

/** Appends an item to the tail of the queue. */
void CXQueuePush (CXQueue* queue, CXInteger item);

/** Removes and returns the head item. Undefined when called on an empty queue. */
CXInteger CXQueuePop (CXQueue* queue);

/** Peeks at the head element without removing it. */
CXInteger CXQueuePeek (CXQueue* queue);

/** Constructs a queue instance with function pointers configured. */
CXQueue CXQueueCreate ();
/** Attempts to dequeue into `value`, returning CXFalse when the queue is empty. */
CXBool CXQueueDequeue (CXQueue* queue, CXInteger *value);
/** Releases all allocated nodes and resets the queue. */
void CXQueueDestroy (CXQueue* queue);


#endif
