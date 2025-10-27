#include "CXIndexManager.h"

/** Ensures the recycled-index list can hold at least `desiredCapacity` items. */
static CXBool CXIndexManagerEnsureCapacity(CXIndexManagerRef manager, CXSize desiredCapacity) {
	if (desiredCapacity <= manager->freeCapacity) {
		return CXTrue;
	}

	CXSize newCapacity = manager->freeCapacity > 0 ? manager->freeCapacity : 4;
	while (newCapacity < desiredCapacity) {
		newCapacity = CXCapacityGrow(newCapacity);
		if (newCapacity < desiredCapacity) {
			// guard against wrap-around
			newCapacity = desiredCapacity;
			break;
		}
	}

	CXIndex *newList = realloc(manager->freeList, sizeof(CXIndex) * newCapacity);
	if (!newList) {
		return CXFalse;
	}

	manager->freeList = newList;
	manager->freeCapacity = newCapacity;
	return CXTrue;
}

/** Initializes the index manager with optional preallocated capacity. */
void CXInitIndexManager(CXIndexManagerRef manager, CXSize initialCapacity, CXSize maxCapacity) {
	if (!manager) {
		return;
	}
	manager->freeList = NULL;
	manager->freeCount = 0;
	manager->freeCapacity = 0;
	manager->nextIndex = 0;
	manager->maxCapacity = maxCapacity;

	if (initialCapacity > 0) {
		CXIndexManagerEnsureCapacity(manager, initialCapacity);
	}
}

/** Allocates and initializes a heap-backed index manager. */
CXIndexManagerRef CXNewIndexManager(CXSize initialCapacity, CXSize maxCapacity) {
	CXIndexManagerRef manager = calloc(1, sizeof(CXIndexManager));
	if (!manager) {
		return NULL;
	}
	CXInitIndexManager(manager, initialCapacity, maxCapacity);
	return manager;
}

/** Clears state so the manager behaves like freshly initialized. */
void CXIndexManagerReset(CXIndexManagerRef manager) {
	if (!manager) {
		return;
	}
	manager->freeCount = 0;
	manager->nextIndex = 0;
}

/** Returns an index to the recycled pool if it falls within range. */
void CXIndexManagerAddIndex(CXIndexManagerRef manager, CXIndex index) {
	if (!manager) {
		return;
	}
	if (index >= manager->maxCapacity) {
		return; // Silently drop invalid indices
	}
	if (!CXIndexManagerEnsureCapacity(manager, manager->freeCount + 1)) {
		return;
	}
	manager->freeList[manager->freeCount++] = index;
}

/** Retrieves the next available index, either recycled or freshly issued. */
CXIndex CXIndexManagerGetIndex(CXIndexManagerRef manager) {
	if (!manager) {
		return CXIndexMAX;
	}
	if (manager->freeCount > 0) {
		return manager->freeList[--manager->freeCount];
	}
	if ((CXSize)manager->nextIndex < manager->maxCapacity) {
		return manager->nextIndex++;
	}
	return CXIndexMAX;
}

/** Adjusts the manager to reflect a new maximum index capacity. */
CXBool CXResizeIndexManager(CXIndexManagerRef manager, CXSize newMaxCapacity) {
	if (!manager) {
		return CXFalse;
	}

	// Remove any recycled indices that exceed the new maximum
	CXSize writeIndex = 0;
	for (CXSize readIndex = 0; readIndex < manager->freeCount; readIndex++) {
		CXIndex value = manager->freeList[readIndex];
		if ((CXSize)value < newMaxCapacity) {
			manager->freeList[writeIndex++] = value;
		}
	}
	manager->freeCount = writeIndex;

	if ((CXSize)manager->nextIndex > newMaxCapacity) {
		manager->nextIndex = (CXIndex)newMaxCapacity;
	}

	manager->maxCapacity = newMaxCapacity;

	// Ensure we have enough storage should every index become free.
	if (!CXIndexManagerEnsureCapacity(manager, newMaxCapacity)) {
		return CXFalse;
	}

	return CXTrue;
}

/** Releases storage owned by the manager but does not free the struct. */
void CXFreeIndexManager(CXIndexManagerRef manager) {
	if (!manager) {
		return;
	}
	if (manager->freeList) {
		free(manager->freeList);
		manager->freeList = NULL;
	}
	manager->freeCapacity = 0;
	manager->freeCount = 0;
	manager->nextIndex = 0;
	manager->maxCapacity = 0;
}
