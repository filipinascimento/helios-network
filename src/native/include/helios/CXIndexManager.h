//
//  CXIndexManager.h
//  Helios Network Core
//
//  Redesigned index pool that supports fast allocation/recycling with
//  amortised O(1) operations and dynamic growth.
//

#ifndef CXNetwork_CXIndexManager_h
#define CXNetwork_CXIndexManager_h

#include "CXCommons.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	CXIndex *freeList;    // LIFO stack of recycled indices
	CXSize freeCount;     // Number of entries currently stored
	CXSize freeCapacity;  // Allocated capacity for freeList
	CXIndex nextIndex;    // Next virgin index to emit
	CXSize maxCapacity;   // Hard limit for allocation
} CXIndexManager;

typedef CXIndexManager* CXIndexManagerRef;

void CXInitIndexManager(CXIndexManagerRef manager, CXSize initialCapacity, CXSize maxCapacity);
CXIndexManagerRef CXNewIndexManager(CXSize initialCapacity, CXSize maxCapacity);
void CXIndexManagerReset(CXIndexManagerRef manager);
void CXIndexManagerAddIndex(CXIndexManagerRef manager, CXIndex index);
CXIndex CXIndexManagerGetIndex(CXIndexManagerRef manager);
CXBool CXResizeIndexManager(CXIndexManagerRef manager, CXSize newMaxCapacity);
void CXFreeIndexManager(CXIndexManagerRef manager);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXIndexManager_h */
