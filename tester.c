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
    
    cache_read(&cache0, address, &data, &mesi_bus); //simulate read transaction ->should be read miss -> should fetch from main memory : suppose 20 cycles 
    log_mesibus(&mesi_bus, clock);

   // read from empty cache 0 address 0
   int counter =0; 
   while(!cache0.ack)
      {
          clock++;
         snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
         log_mesibus(&mesi_bus, clock);
      }
   printf("\n-----------------------------------------------------------------------------------------\n");

   // read cache 1 address 0 -  

   clock++;
   cache_read(&cache1, address, &data, &mesi_bus); //simulate read transaction ->should be read miss -> should fetch from main memory : suppose 20 cycles 
   log_mesibus(&mesi_bus, clock);

   printf("\n-----------------------------------------------------------------------------------------\n");

   printf("Data after read op: %d\n", data);

   while(!cache1.ack)
   {
      clock++;
      snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
      log_mesibus(&mesi_bus, clock);
      
    }
   printf("\n-----------------------------------------------------------------------------------------\n");
   printf("cache_write\n");
   int data0 = 42;
   cache_write(&cache0, address, data0, &mesi_bus); //simulate write transaction ->should be write hit!
   log_mesibus(&mesi_bus, clock);

     while(!cache0.ack)
   {
      clock++;
      snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0

      log_mesibus(&mesi_bus, clock);
   }

   printf("\n-----------------------------------------------------------------------------------------\n");

   cache_write(&cache1, address, 69, &mesi_bus); //simulate write transaction ->should be write hit!
   log_mesibus(&mesi_bus, clock);
   
   while(!cache1.ack)
   {
      clock++;

      printf("\n\n\nclock cycle ack %d\n\n\n", clock);

      snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
      log_mesibus(&mesi_bus, clock);
   }
   clock++;
   log_cache_state(&cache0);
   log_cache_state(&cache1);

//   snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
//   log_mesibus(&mesi_bus, clock++);
   // printf("cache_write2\n");
   // log_cache_state(&cache1);
   // int data1 = 123;
   // cache_write(&cache1, address, data1, &mesi_bus); //simulate read transaction ->should be read hit! 
   // while(!cache1.ack)
   // {
   //    snoop_bus(Cache_array, address, &mesi_bus, &main_memory, clock); //main memory data should be fetched to cache0
   //    log_mesibus(&mesi_bus, clock++);
   // }


 }