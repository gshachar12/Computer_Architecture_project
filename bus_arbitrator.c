#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers/cache_structs.h"
static int lastGrantedCore = -1;


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#define YELLOW "\033[1;33m"
#define WHITE "\033[1;37m"
#define MAGENTA "\033[1;35m"



// Initialize the BusArbitratorQueue
void initializeQueue(BusArbitratorQueue *BusArbitratorQueue) {
    BusArbitratorQueue->front = 0;
    BusArbitratorQueue->rear = -1;
    BusArbitratorQueue->size = 0;
}

// Check if the BusArbitratorQueue is empty
bool isQueueEmpty(BusArbitratorQueue *BusArbitratorQueue) {
    return BusArbitratorQueue->size == 0;
    
}

// Check if the BusArbitratorQueue is full
bool isQueueFull(BusArbitratorQueue *BusArbitratorQueue) {
    return BusArbitratorQueue->size == NUM_CORES;
}

// EnBusArbitratorQueue a Core pointer to the BusArbitratorQueue
bool enqueue(BusArbitratorQueue *BusArbitratorQueue, MESI_bus_Command *element) {
    if (isQueueFull(BusArbitratorQueue)) {
        fprintf(stderr, "BusArbitratorQueue is full. Cannot enQueue element.\n");
        return false;
    }
    BusArbitratorQueue->rear = (BusArbitratorQueue->rear + 1) % NUM_CORES;
    BusArbitratorQueue->data[BusArbitratorQueue->rear] = element;
    BusArbitratorQueue->size++;

    printf("%senqueued %d%s\n", YELLOW,  element->requesting_id, WHITE);

    return true;
}



// DeBusArbitratorQueue a Core pointer from the BusArbitratorQueue
MESI_bus_Command *dequeue(BusArbitratorQueue *BusArbitratorQueue) {
    if (isQueueEmpty(BusArbitratorQueue)) {
        fprintf(stderr, "BusArbitratorQueue is empty. Cannot deQueue element.\n");
        return NULL;
    }
    
    MESI_bus_Command *element = BusArbitratorQueue->data[BusArbitratorQueue->front];
    BusArbitratorQueue->front = (BusArbitratorQueue->front + 1) % NUM_CORES;

        printf("%sdequeued %d%s\n", MAGENTA,  element->requesting_id, WHITE);

    BusArbitratorQueue->size--;
    return element;
}

// Peek at the front element of the BusArbitratorQueue
MESI_bus_Command *peek(BusArbitratorQueue *BusArbitratorQueue) {
    if (isQueueEmpty(BusArbitratorQueue)) {
        fprintf(stderr, "BusArbitratorQueue is empty. Nothing to peek at.\n");
        return NULL;
    }
    return BusArbitratorQueue->data[BusArbitratorQueue->front];
}

int roundRobinArbitrator(MESI_bus* bus, int busRequests[NUM_CORES]) {
    if (!bus->busy) {
        return -1;
    }

    // round robin
    for (int i = 0; i < NUM_CORES; i++) {
        int coreId = (lastGrantedCore + i) % NUM_CORES;
       if (busRequests[coreId]) {
            lastGrantedCore = coreId;
            return coreId;
        }
    }
    return -1;
}



