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

void CXQueuePush (CXQueue* queue, CXInteger item);

CXInteger CXQueuePop (CXQueue* queue);

CXInteger CXQueuePeek (CXQueue* queue);

CXQueue CXQueueCreate ();
CXBool CXQueueDequeue (CXQueue* queue, CXInteger *value);
void CXQueueDestroy (CXQueue* queue);


#endif