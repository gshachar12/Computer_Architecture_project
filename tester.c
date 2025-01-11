#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "headers/initialize.h"


int main() {
    // Create instances of DSRAM and TSRAM for two caches
    CACHE cache0, cache1, cache2, cache3;
    CACHE* Cache_array[] = {&cache0, &cache1, &cache2, &cache3};
    MESI_bus mesi_bus;
    MainMemory main_memory;

    main_memory.memory_data = (int *)malloc(MAIN_MEMORY_SIZE * sizeof(int));
    FILE* tsram0 = fopen("log_files/tsram0.txt", "wt");
    FILE* tsram1 = fopen("log_files/tsram1.txt", "wt");
    FILE* tsram2 = fopen("log_files/tsram2.txt", "wt");
    FILE* tsram3 = fopen("log_files/tsram3.txt", "wt");

    FILE* dsram0 = fopen("log_files/dsram0.txt", "wt");
    FILE* dsram1= fopen("log_files/dsram1.txt", "wt");
    FILE* dsram2 = fopen("log_files/dsram2.txt", "wt");
    FILE* dsram3 = fopen("log_files/dsram3.txt", "wt");
    printf("\nFinished opening log files");


    initialize_cache(&cache0, dsram0, tsram0, 0);
    initialize_cache(&cache1, dsram1, tsram1, 1);
    initialize_cache(&cache2, dsram2, tsram2, 2);
    initialize_cache(&cache3, dsram3, tsram3, 3);
    printf("\nFinished initializing caches");

    initialize_main_memory(&main_memory, "mem_files/main_memory.txt");
    log_main_memory(&main_memory);
    printf("\nFinished initializing main_memory");
    initialize_mesi_bus(&mesi_bus, "mem_files/mesi_bus.txt");
    printf("\nFinished initializing mesi_bus\n");
    printf("ack in main %d\n", (&cache0)->ack);
    int address = 0x00000000;
    int data;
    int clock =0; 
    
    log_cache_state(&cache0);
    cache_read(&cache0, address, &data, &mesi_bus); //simulate read transaction ->should be read miss -> should fetch from main memory : suppose 20 cycles 
    printf("\nFinished reading\n");
    for(clock=0; clock<=25; clock++)
       {
    snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
    log_mesibus(&mesi_bus, clock);}
    printf("\n-----------------------------------------------------------------------------------------\n");

    int init_clock = clock;
    cache_read(&cache1, address, &data, &mesi_bus); //simulate read transaction ->should be read miss -> should fetch from main memory : suppose 20 cycles 
    printf("\n-----------------------------------------------------------------------------------------\n");

    printf("Data after read op: %d\n", data);

    for(clock; clock<=init_clock+5; clock++)
       {
    snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
    log_mesibus(&mesi_bus, clock);}
      printf("\n-----------------------------------------------------------------------------------------\n");
      printf("cache_write\n");
   printf("mesinbys parametes: %d %d %d\n", mesi_bus.bus_data, mesi_bus.bus_addr, mesi_bus.bus_origid);
    cache_write(&cache0, address, &mesi_bus, 42); //simulate write transaction ->should be write hit!
      printf("\n-----------------------------------------------------------------------------------------\n");
   snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
//     cache_read(&cache0, address, &data, &mesi_bus); //simulate read transaction ->should be read hit! 
//     printf("Data after read op: %d\n", data);
//     snoop_bus(Cache_array, address, &mesi_bus, &main_memory); //cache0 should send data to bus

//     cache_read(&cache1, address, &data, &mesi_bus); //simulate read transaction ->should be read miss -> should fetch from cache0! : suppose 4 cycles
//     snoop_bus(Cache_array, address, &mesi_bus, &main_memory); //cache0 should send data to bus
 }