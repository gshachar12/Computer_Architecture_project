
#include "bus_structs.h"
#ifndef CACHE_STRUCTS_H
#define CACHE_STRUCTS_H
// Constants for memory and cache configuration
#define LINE_CONTENT 0x00000000 // Content for each memory line

#define MAIN_MEMORY_SIZE (1 << 20)  // 2^20 words (1MB of memory)
#define CACHE_SIZE 256             // 256 words
#define BLOCK_SIZE 4               // 4 words per block
#define NUM_BLOCKS (CACHE_SIZE / BLOCK_SIZE) // Number of cache blocks
#define BLOCK_OFFSET_BITS 2        // log2(4) = 2 bits to index a word within a block
#define INDEX_BITS 6               // log2(64) = 6 bits for cache index
#define TAG_BITS (32 - INDEX_BITS - BLOCK_OFFSET_BITS) // Assuming 32-bit address
#define MAIN_MEMORY_STALLS 15
#define DATA_IS_READY 1

extern int snoop_bus_request;
extern int num_words_sent; 
extern uint32_t block_offset_counter; 
extern int main_memory_stalls_counter; 

// Cache Line structure
typedef struct cacheLine{
    uint32_t data[BLOCK_SIZE];  // Data: 4 words (32 bits each)
} CacheLine;

// Data Cache structure (DSRAM)
typedef struct {
    CacheLine cache[NUM_BLOCKS]; // Cache array with 64 blocks
    uint64_t cycle_count;        // Total cycles counter
    FILE *logfile;
} DSRAM;

typedef struct {
    uint32_t tag;               
    MESIState mesi_state;           // Coherence state
} CacheLine_TSRAM;

typedef struct {
    CacheLine_TSRAM cache[NUM_BLOCKS]; // Cache array with 64 blocks
    uint64_t cycle_count;        // Total cycles counter
    FILE *logfile;
} TSRAM;


typedef struct {
    DSRAM* dsram; 
    TSRAM* tsram; 
    int cache_id ; 
    int ack;
    int num_stalls; 
} CACHE;


typedef struct {
    int *memory_data;  // Pointer to the memory array
    FILE* memin;     // Log file pointer
    FILE* memout; 
} MainMemory;



#endif //CPU_STRUCTS_H
