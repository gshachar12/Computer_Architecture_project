#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_CORES 4

typedef enum {
    INVALID,
    SHARED,
    EXCLUSIVE,
    MODIFIED
} MESIState;

typedef enum {
    NO_COMMAND,          // Request for shared read
    BUS_RD, // Request for exclusive read (write intent)
    BUS_RDX,    // Write data back to main memory
    FLUSH     // Invalidate cache line
} BusOperation;


// Define the Queue structure


typedef struct
{
    BusOperation cmd; // 0: no command, 1: busrd, 2:busrdx, 3:flush
    int requesting_id; 
    int requesting_address;
} MESI_bus_Command;


typedef struct {
    MESI_bus_Command *data[NUM_CORES]; // Array of pointers to Core
    int front;              // Index of the front element
    int rear;               // Index of the rear element
    int size;               // Current size of the queue
} BusArbitratorQueue;

typedef struct
{
    int bus_origid; //Originator of this transaction 0: core 0 1: core 1 2: core 2 3: core 3 4: main memory
    BusOperation bus_cmd; // 0: no command, 1: busrd, 2:busrdx, 3:flush
    int bus_addr; //word address
    int bus_data;// word data
    int wr;
    int stall;
    int busy; 
    int bus_shared; // set to 1 when answering a BusRd transaction if any of the cores has the data in the cache, otherwise set to 0.
    int bus_requesting_id;
    int bus_requesting_address;
    int bus_write_buffer;
    BusArbitratorQueue* bus_queue; 
    FILE *logfile;
} MESI_bus;
