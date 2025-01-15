#include <stdbool.h>
#include "headers/cache.h" 
#define NUM_CORES 4
static int lastGrantedCore = -1;

bool busAvailable(MESI_bus* bus) {
    if(bus->bus_cmd == NO_COMMAND && bus->stall == 0) {
        return true;
    }
    
    return false;
}

int roundRobinArbitrator(MESI_bus* bus, bool busRequests[NUM_CORES]) {
    if (!busAvailable(bus)) {
        return -1;
    }

    // round robin
    for (int i = 1; i <= NUM_CORES; i++) {
        int coreId = (lastGrantedCore + i) % NUM_CORES;
        if (busRequests[coreId]) {
            lastGrantedCore = coreId;
            return coreId;
        }
    }
    return -1;
}