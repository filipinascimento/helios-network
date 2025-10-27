
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "CXIndexManager.h"

// typedef struct {
//     int *freeIndices; // Circular array of free indices
//     int readIndex;    // Index to read the next free slot
//     int writeIndex;   // Index to write the next free slot
//     int capacity;     // Capacity of the freeIndices array
//     int maxCapacity;  // Maximum capacity of the network (size of the node buffer)
// } CXIndexManager;


// void CXInitIndexManager(CXIndexManager *manager, int initialCapacity, int maxCapacity) {
// manager->freeIndices = calloc(initialCapacity, sizeof(int));
//     manager->readIndex = 0;
//     manager->writeIndex = 0;
//     manager->capacity = initialCapacity;
//     manager->maxCapacity = maxCapacity;
// }

// void CXAddIndex(CXIndexManager *manager, int index) {
//     if ((manager->writeIndex + 1) % manager->capacity == manager->readIndex) {
//         // Resize the freeIndices array
//         int newCapacity = manager->capacity * 2;
//         if (newCapacity > manager->maxCapacity){
//             newCapacity = manager->maxCapacity;
//         }
//         int *newArray = realloc(manager->freeIndices, sizeof(int) * newCapacity);

//         if (!newArray) {
//             // Handle realloc failure
//             return;
//         }

//         if (manager->writeIndex < manager->readIndex) {
//             // Move the part from readIndex to the end of the old array to the end of the new array
//             memcpy(newArray + newCapacity - (manager->capacity - manager->readIndex),
//                    newArray + manager->readIndex,
//                    sizeof(int) * (manager->capacity - manager->readIndex));
//             manager->readIndex = newCapacity - (manager->capacity - manager->readIndex);
//         }

//         manager->freeIndices = newArray;
//         manager->capacity = newCapacity;
//     }

//     manager->freeIndices[manager->writeIndex] = index;
//     manager->writeIndex = (manager->writeIndex + 1) % manager->capacity;
// }

// int CXGetIndex(CXIndexManager *manager) {
//     if (manager->readIndex == manager->writeIndex) {
//         return -1; // No free index available
//     }
//     int index = manager->freeIndices[manager->readIndex];
//     manager->readIndex = (manager->readIndex + 1) % manager->capacity;
//     return index;
// }


int getRandomActiveNode(bool *activeNodes, CXSize maxCapacity) {
    // Check if there's at least one active node
    bool hasActiveNode = false;
    for (CXIndex i = 0; i < maxCapacity; ++i) {
        if (activeNodes[i]) {
            hasActiveNode = true;
            break;
        }
    }
    if (!hasActiveNode) {
        return -1; // No active nodes available
    }

    // Attempt to find an active node randomly
    for (CXIndex attempt = 0; attempt < maxCapacity; ++attempt) {
        CXIndex randomIndex = rand() % maxCapacity;
        if (activeNodes[randomIndex]) {
            return randomIndex; // Active node found
        }
    }

    // Optionally, scan for an active node if random attempts fail
    CXIndex startIndex = rand() % maxCapacity;
    CXIndex currentIndex = startIndex;
    do {
        if (activeNodes[currentIndex]) {
            return currentIndex; // Active node found
        }
        currentIndex = (currentIndex + 1) % maxCapacity;
    } while (currentIndex != startIndex);

    return -1; // No active node found after full scan
}


int main(int argc, char *argv[]) {
    // Initialize the IndexManager
    CXIndexManager manager;
    CXSize initialCapacity = 10;
    CXSize maxCapacity = 1000;
    CXInitIndexManager(&manager, initialCapacity, maxCapacity);
    CXIndex intSequenceIndex = 0;
    // Setup for simulation
    CXIndex numIterations = 1000;
    double probabilityOfAddition = 0.5;
    bool *activeNodes = calloc(maxCapacity, sizeof(bool));
    CXSize totalNodes = 0;
    // Simulation loop
    for (CXIndex i = 0; i < numIterations; ++i) {
        if (rand() / (double)RAND_MAX < probabilityOfAddition) {
            // Attempt to add a node
            CXIndex index = CXIndexManagerGetIndex(&manager);
            if (index != -1) {
                activeNodes[index] = true; // Mark node as active
                printf("Adding node %d\n", index);
                totalNodes++;
            }else{ // use sequence
                printf("Not available. Using sequence %d\n", totalNodes);
                activeNodes[totalNodes] = true; // Mark node as active
                printf("Adding node %d\n", totalNodes);
                totalNodes++;
            }
            
        } else {
            // Attempt to remove a node
            int nodeToRemove = getRandomActiveNode(activeNodes, maxCapacity);
            if (nodeToRemove != -1) {
                activeNodes[nodeToRemove] = false; // Mark node as inactive
                CXIndexManagerAddIndex(&manager, nodeToRemove);
                printf("Removing node %d\n", nodeToRemove);
                totalNodes--;
            }
        }
    }

// Verification
// ... Verify the consistency of activeNodes and the state of IndexManager
// ... Verify that the number of active nodes is correct
    int numActiveNodes = 0;
    for (int i = 0; i < maxCapacity; ++i) {
        if (activeNodes[i]) {
            ++numActiveNodes;
        }
    }
    printf("Number of active nodes: %d\n", numActiveNodes);

    // Cleanup
    free(activeNodes);
    free(manager.freeIndices);
    return 0;
}