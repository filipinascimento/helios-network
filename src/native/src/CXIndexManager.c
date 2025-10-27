#include "CXIndexManager.h"

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

CXIndexManagerRef CXNewIndexManager(CXSize initialCapacity, CXSize maxCapacity) {
	CXIndexManagerRef manager = calloc(1, sizeof(CXIndexManager));
	if (!manager) {
		return NULL;
	}
	CXInitIndexManager(manager, initialCapacity, maxCapacity);
	return manager;
}

void CXIndexManagerReset(CXIndexManagerRef manager) {
	if (!manager) {
		return;
	}
	manager->freeCount = 0;
	manager->nextIndex = 0;
}

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
