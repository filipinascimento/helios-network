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

/**
 * State container that tracks reserved and recycled indices for nodes or edges.
 */
typedef struct {
	CXIndex *freeList;    // LIFO stack of recycled indices
	CXSize freeCount;     // Number of entries currently stored
	CXSize freeCapacity;  // Allocated capacity for freeList
	CXIndex nextIndex;    // Next virgin index to emit
	CXSize maxCapacity;   // Hard limit for allocation
} CXIndexManager;

typedef CXIndexManager* CXIndexManagerRef;

/** Initializes an existing manager structure in-place. */
void CXInitIndexManager(CXIndexManagerRef manager, CXSize initialCapacity, CXSize maxCapacity);
/** Allocates and initializes a new index manager on the heap. */
CXIndexManagerRef CXNewIndexManager(CXSize initialCapacity, CXSize maxCapacity);
/** Clears state so that allocation starts from zero again. */
void CXIndexManagerReset(CXIndexManagerRef manager);
/** Returns an index to the pool so it can be reused. */
void CXIndexManagerAddIndex(CXIndexManagerRef manager, CXIndex index);
/** Retrieves the next available index, growing the pool on demand. */
CXIndex CXIndexManagerGetIndex(CXIndexManagerRef manager);
/** Adjusts the hard maximum capacity for the manager. */
CXBool CXResizeIndexManager(CXIndexManagerRef manager, CXSize newMaxCapacity);
/** Releases any heap allocations associated with the manager. */
void CXFreeIndexManager(CXIndexManagerRef manager);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CXNetwork_CXIndexManager_h */
