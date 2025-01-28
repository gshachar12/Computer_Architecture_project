#ifndef BUSArbitrator_H
#define BUSArbitrator_H
#include "cache_structs.h"
#include <stdbool.h> // For bool type

// Function declarations
void initializeQueue(BusArbitratorQueue *BusArbitratorQueue);
bool isQueueEmpty(BusArbitratorQueue *BusArbitratorQueue);
bool isQueueFull(BusArbitratorQueue *BusArbitratorQueue);
bool enqueue(BusArbitratorQueue *BusArbitratorQueue, MESI_bus_Command *element);
MESI_bus_Command *dequeue(BusArbitratorQueue *BusArbitratorQueue);
int roundRobinArbitrator(MESI_bus* bus, int busRequests[NUM_CORES]);
MESI_bus_Command *deQueue(BusArbitratorQueue *BusArbitratorQueue);
MESI_bus_Command *peek(BusArbitratorQueue *BusArbitratorQueue);


#endif
