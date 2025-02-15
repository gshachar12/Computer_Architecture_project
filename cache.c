#include "headers/bus_arbitrator.h"
#include "headers/utils.h"

#define BLOCK_OFFSET_BITS 2       // log2(4) = 2 bits to index a word within a block
#define INDEX_BITS 6              // log2(64) = 6 bits for cache index
#define TAG_BITS (32 - INDEX_BITS - BLOCK_OFFSET_BITS) // Assuming 32-bit address
#define NUM_CACHES 2
 int num_words_sent=0; 
 int modified_block_memory_write = 0; 
 uint32_t block_offset_counter=0; 
 int main_memory_stalls_counter=0; 


 void log_main_memory(MainMemory *main_memory) {
    // Log DSRAM state
    char hex_num[9];
    for (int i = 0; i < MAIN_MEMORY_SIZE; i++) {
        Int_2_Hex(main_memory->memory_data[i], hex_num);
        fprintf(main_memory->memout, "%s\n" , hex_num);
       

    }
    fflush(main_memory->memout);
}



// Extract the tag, index, and block offset from an address
void get_cache_address_parts(uint32_t address, uint32_t *tag, uint32_t *index, uint32_t *block_offset) {
    *block_offset = address & ((1 << BLOCK_OFFSET_BITS) - 1);  // Extract block offset (2 bits)
    *index = (address >> BLOCK_OFFSET_BITS) & ((1 << INDEX_BITS) - 1);  // Extract index (6 bits)
    *tag = address >> (BLOCK_OFFSET_BITS + INDEX_BITS);  // Extract the remaining bits as tag
}
// Function to log cache state to a text file
void log_mesibus(MESI_bus *bus, int cycle) {
    // Check if the logfile is initialized
    if (bus->logfile == NULL) {
        printf("Error: Logfile not initialized for DSRAM.\n");
        return; // Prevent further execution if logfile is not available
    }

 
    // Log the bus details in a cleaner format with two-digit padding for numbers < 10
    if (bus->bus_cmd != 0 && !bus->stall) {
     fprintf(bus->logfile, "%d   %d    %d   0x%08X   %d       %d     %d\n", 
            cycle, 
            bus->bus_origid, 
            bus->bus_cmd, 
            bus->bus_addr, 
            bus->bus_data, 
            bus->bus_shared, 
            bus->bus_requesting_id 
            );
    }
}


// Function to log cache state to a text file
void log_cache_state(CACHE* cache) {
    // Log DSRAM state
    if (cache->dsram->logfile == NULL) {
        printf("Error: Logfile not initialized for DSRAM.\n");
        return; // Prevent further execution if logfile is not available
    }

    fprintf(cache->dsram->logfile, "DSRAM Cache State:%d\n", cache->dsram->cycle_count);
    fprintf(cache->dsram->logfile, "Block Number |Data[0]    Data[1]    Data[2]    Data[3]\n");
    fprintf(cache->dsram->logfile, "---------------------------------------------------\n");

    for (int i = 0; i < NUM_BLOCKS; i++) {
        CacheLine *dsram_line = &cache->dsram->cache[i];
        fprintf(cache->dsram->logfile, "%12d | ", i);  // Log block number
        
        for (int j = 0; j < BLOCK_SIZE; j++) {
            fprintf(cache->dsram->logfile, "0x%08X ", dsram_line->data[j]);  // Log data in each block
        }
        fprintf(cache->dsram->logfile, "\n");
    }

    fprintf(cache->dsram->logfile, "---------------------------------------------------\n");
    fflush(cache->dsram->logfile);

    // Log TSRAM state
    if (cache->tsram->logfile == NULL) {
        printf("Error: Logfile not initialized for TSRAM.\n");
        return; // Prevent further execution if logfile is not available
    }

    fprintf(cache->tsram->logfile, "TSRAM Cache State:\n");
    fprintf(cache->tsram->logfile, "Block Number | MESI State | Tag        \n");
    fprintf(cache->tsram->logfile, "---------------------------------------------------\n");

    for (int i = 0; i < NUM_BLOCKS; i++) {
        CacheLine_TSRAM *tsram_line = &cache->tsram->cache[i];
        fprintf(cache->tsram->logfile, "%12d | %-11s | 0x%08X\n", 
                i, 
                (tsram_line->mesi_state == INVALID ? "INVALID" :
                 (tsram_line->mesi_state == SHARED ? "SHARED" :
                 (tsram_line->mesi_state == EXCLUSIVE ? "EXCLUSIVE" : "MODIFIED"))),
                tsram_line->tag);  // Log MESI state and tag
    }

    fprintf(cache->tsram->logfile, "---------------------------------------------------\n");
    fflush(cache->tsram->logfile);
}
void send_data_from_main_memory_to_bus(MainMemory *main_memory, MESI_bus *bus, int address)
{
    
    bus->bus_data = main_memory->memory_data[address];
    bus->bus_addr = address;
    bus->bus_origid = 4;
    printf("Data sent to the bus from main memory! Data: %u\n", bus->bus_data);
    printf("%smain memory: %d\n%s", RED,main_memory->memory_data[0], WHITE);
}

/******************************************************************************
* Function: check_shared_bus
*
* Description: check if a block is in another cache. if yes, return array of all caches_id that have the block
*******************************************************************************/
int* check_shared_bus(CACHE* caches[], int origid, int address) {
    uint32_t tag, index, block_offset;
    get_cache_address_parts(address, &tag, &index, &block_offset);
    //printf("%s origid: %d %s, ", RED, origid, WHITE);
    // Dynamically allocate memory for cache_indexes array based on found_count
    int *cache_indexes = NULL;
    int found_count = 0;

    // Check other caches
    for (int i = 0; i < NUM_CACHES; i++) {
        if (i == origid) continue; // Skip the requesting cache
        CacheLine_TSRAM *tsram_line = &(caches[i]->tsram->cache[index]);
        if (tsram_line->mesi_state != INVALID && tsram_line->tag == tag) {  // Assuming 0 means INVALID
            // Data found in cache i 
            //printf("Bus: Data found in cache %d\n", i);
            // Dynamically resize the cache_indexes array to hold the found cache index
            cache_indexes = realloc(cache_indexes, (found_count + 1) * sizeof(int));
            if (cache_indexes == NULL) {
                fprintf(stderr, "\nMemory allocation failed for cache_indexes.\n");
                exit(EXIT_FAILURE);
            }
            cache_indexes[found_count] = i;  // Store the cache index
            found_count++;  // Increment the found count
        }
    }

    // Return NULL if no caches have the block
    if (found_count == 0) {
        printf("NO\n");
        return NULL;  // Return NULL if no caches have the data
    }

    // Print and return the array of cache indexes
    printf("YES\n");
    for (int i = 0; i < found_count; i++) {
        printf("Caches indexs that own the block: %d\n", cache_indexes[i]);
    }
    return cache_indexes;
}


/******************************************************************************
* Function: send_op_to_bus
*
* Description: send command to the bus
*******************************************************************************/
void send_op_to_bus(MESI_bus *bus, int origid, BusOperation cmd, int addr) {


    //MESI_bus_Command *queue_command = (MESI_bus_Command *)malloc(sizeof(MESI_bus_Command));

    printf("Sending operation to the bus: origid: %d, cmd: %d, addr: %d, requesting_id: %d\n", origid, cmd, addr, origid);
    printf("mesibus_data = %d\n", bus->bus_data);
    // Set bus data to arbitrary value
    bus->bus_data = 0;

    // queue_command->cmd = cmd;
    // queue_command->requesting_id = origid;
    // queue_command->requesting_address = addr;



    // Set bus fields if bus is available

    if(!bus->busy)
    {
        printf("\n%s bus is free, uploading core %d to RESA bus%s\n", YELLOW, origid, WHITE);
        // if(!isQueueEmpty(bus->bus_queue))
        //     {
        //         queue_command = dequeue(bus->bus_queue);
        //     }

    
        bus->bus_origid             = origid;
        bus->bus_cmd                = cmd;
        bus->bus_addr               = addr;
        bus->bus_requesting_id      = origid;
        bus->bus_requesting_address = addr;
        bus->busy = 1;
    }

    // enqueue command until the bus is free
    else
    {

    printf("\n%s bus is taken by core %d%s\n", YELLOW, bus->bus_requesting_id, WHITE);

    //enqueue( bus ->bus_queue, queue_command);  // enqueue in bus arbitrator queue
    }

    // Print the values after assigning them
    printf("bus_origid: %d\n", bus->bus_origid);
    printf("bus_cmd: %d\n", bus->bus_cmd);
    printf("bus_addr: %d\n", bus->bus_addr);
    printf("bus_requesting_id: %d\n", bus->bus_requesting_id);
    return;
}


/******************************************************************************
* Function: send_data_to_bus
*
* Description: send data to the bus. can happen when flushing/send data from other cache to another.
*******************************************************************************/

void send_data_to_bus(MESI_bus *bus, int data, int origid, int bus_shared, int address, int requesting_id) {
    bus->bus_data = data;
    bus->bus_origid = origid;
    bus->bus_shared = bus_shared;
    bus->bus_addr = address;
    bus->bus_requesting_id = requesting_id;   
}

int write_to_main_memory(CACHE *requesting, MainMemory* main_memory, uint32_t index)
    {
        static int requesting_address;
        static int modified_flag;

        if(modified_block_memory_write == 0)
        {
            printf("tag for first iteration: %d", requesting->tsram->cache[index].tag);
            if (requesting->tsram->cache[index].mesi_state == MODIFIED)
            modified_flag = 1;
            else 
            modified_flag = 0;
        }

        printf("mesi state: %d, modified flag = %d\n", requesting->tsram->cache[index].mesi_state, modified_flag);
        if (modified_flag)
        {
            //modified_flag = 1;
            
            if(modified_block_memory_write < BLOCK_SIZE)
            {
                if(modified_block_memory_write == 0)
                {
                    requesting_address = requesting->tsram->cache[index].tag<<8 | index <<2; 
                }  
            
                printf("\nindex: %d tag: %d\n", index,requesting->tsram->cache[index].tag ); 
                main_memory->memory_data[requesting_address + modified_block_memory_write] = requesting->dsram->cache->data[(requesting_address + modified_block_memory_write)%CACHE_SIZE];
                printf("\n%s modified address %d data %d%s\n", YELLOW, requesting_address + modified_block_memory_write, main_memory->memory_data[requesting_address + modified_block_memory_write], WHITE ); 
                modified_block_memory_write++; 
            }

    
        }
        return 0; 

    }



int flush_from_main_memory(CACHE *requesting, MainMemory* main_memory, uint32_t address, MESI_bus *bus, uint32_t index)
{
    
     if(main_memory_stalls_counter<MAIN_MEMORY_STALLS) //empty stalls for fetching data from main memory
    {
        write_to_main_memory(requesting, main_memory, index);
        main_memory_stalls_counter++; 
        bus->stall =1;
        printf("\nstalling for: %d cycles\n", main_memory_stalls_counter);
        requesting->tsram->cache[index].tag = address >> (BLOCK_OFFSET_BITS + INDEX_BITS); //was added to fix tag problems
        return 0; 

    }

    else
    {   bus->stall =0;
        if(num_words_sent<BLOCK_SIZE){
            printf("\nblocks sent: %d\n", block_offset_counter );
            printf("test for or:%d\n", (address & ~3) + block_offset_counter);
            send_data_from_main_memory_to_bus(main_memory, bus, (address & ~3) + block_offset_counter);
            bus->bus_cmd=FLUSH;
            num_words_sent++;
            printf("data: %d, stored into index:%d, with offset:%d, bus_addr:%d\n", bus->bus_data, index, block_offset_counter, bus->bus_addr);
            requesting->dsram->cache[index].data[block_offset_counter] = bus->bus_data;
            
            //printf("read %d, block offset %d, index %d", requesting->dsram->cache[index].data[bus->bus_addr], block_offset_counter,  bus->bus_addr, index);
            if(bus->wr)
                {        
                requesting->dsram->cache[0].data[bus->bus_requesting_address%CACHE_SIZE] = bus->bus_write_buffer; 
                printf("data: %d, stored into index:%d, with offset:%d, bus_addr:%d\n", bus->bus_write_buffer, index, block_offset_counter, bus->bus_addr);
                //bus->wr=0;
                }

            printf("%s                    \nreceived ack\n                        %s",YELLOW, WHITE );

            block_offset_counter++;
        }
        if(num_words_sent == BLOCK_SIZE) // transaction finished
        {
        printf("\nfinished stalling: %d\n",main_memory_stalls_counter );
        bus->busy =0;
        bus->bus_cmd = NO_COMMAND;
        block_offset_counter = 0; 
        num_words_sent=0;
        modified_block_memory_write =0; 
        main_memory_stalls_counter = 0; 
        requesting -> memory_stalls = 0; 
       
        
        }
        return 1; 


    }
}


int flush_from_cache(CACHE *requesting, CACHE* modified_cache, MainMemory* main_memory, uint32_t address, MESI_bus *bus, uint32_t index)
{ 
    printf("flushing from cache: %d to %d", modified_cache->cache_id, requesting->cache_id);

    if(num_words_sent<BLOCK_SIZE)
    {
        send_data_to_bus(bus, modified_cache->dsram->cache[index].data[block_offset_counter], bus->bus_origid, 1, (address & ~3)+block_offset_counter, requesting->cache_id);
        requesting->dsram->cache[index].data[block_offset_counter] = bus->bus_data;
        main_memory->memory_data[bus->bus_addr] = bus->bus_data;
        printf("%s flush from cache %s %d", CYAN, WHITE, main_memory->memory_data[bus->bus_addr] );


        requesting->dsram->cache[index].data[bus->bus_addr] = bus->bus_data;
        if(bus->wr)
            {        
            requesting->dsram->cache[index].data[bus->bus_addr] = bus->bus_write_buffer; 
            bus->wr=0;
            }

        bus->bus_cmd=FLUSH;

        block_offset_counter++;
        num_words_sent++;
        return 0;
    }
    else
    {
        printf("\nfinished stalling: %d\n",main_memory_stalls_counter );
        bus->bus_cmd = NO_COMMAND;
        block_offset_counter = 0; 
        num_words_sent=0;
        main_memory_stalls_counter = 0; 
        bus->busy =0;
        return 1; 
    }


}

/******************************************************************************
* Function: snoop_bus
*
* Description: simulating all caches(except the one that initiated the transaction) to check what's on the bus
* maybe they need to send their data to the bus, or invalidate their cache line 
* should occur every cycle
*******************************************************************************/
int snoop_bus(CACHE *caches[], MESI_bus *bus, MainMemory *main_memory, int clock_cycle) { //change to pointers
    uint32_t tag, index, block_offset;
    get_cache_address_parts(bus->bus_requesting_address, &tag, &index, &block_offset);
    int *caches_owning_block_id_array;
     
    MESI_bus* data_path_bus;
    // Debug print to see cache address parts
    

        switch (bus->bus_cmd) 
        {
            case NO_COMMAND:
                // No command, just snooping
                printf("\nSnooping NO_COMMAND: No operation.\n");
                bus->bus_shared = 0;  // Data is not shared
                bus->bus_addr = 0;
                break;
            case BUS_RD:
                caches_owning_block_id_array = check_shared_bus(caches,bus->bus_requesting_id, bus->bus_requesting_address); // Check if the data is in another cache, if yes, return the cache id

                // Bus read operation : When a BusRd (Bus Read) transaction occurs on the bus, it indicates that a processor or cache is requesting a block of data from the memory system.
                printf("\nSnooping BUS_READ\n"); 
                if(bus->stall)
                {
                    caches[bus->bus_requesting_id]->ack= flush_from_main_memory(caches[bus->bus_requesting_id], main_memory, bus->bus_requesting_address, bus,  index);
                    return 0;
                }
                if (caches_owning_block_id_array != NULL) //block found
                {
                    // Data found in another cache
                if(caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == MODIFIED)
                {
                    printf("\nMODIFIED Data found in cache %d, fetching from there...\n", caches_owning_block_id_array[0]);
                    caches[bus->bus_requesting_id]->ack= flush_from_cache(caches[bus->bus_requesting_id], caches[caches_owning_block_id_array[0]], main_memory, bus->bus_requesting_address, bus,  index);
                    caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = SHARED;
                    caches[bus->bus_origid]->tsram->cache[index].mesi_state = SHARED;

                }
                else
                {
                    printf("\nFOUND A VALID BLOCK: Data found in cache %d, fetching from there...\n", caches_owning_block_id_array[0]);
                    caches[bus->bus_requesting_id]->ack= flush_from_main_memory(caches[bus->bus_requesting_id], main_memory, bus->bus_requesting_address, bus,  index);
                }

                if(caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == SHARED)
                    caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = SHARED;

                if(caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == EXCLUSIVE || caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == MODIFIED)
                    {
                    caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state= SHARED; 
                    caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = SHARED;
                    }

                }

                else //invalid
                {       
                    printf("\nDIDN'T FIND A BLOCK: FETCHING DATA FROM MAIN MEMORY\n");
                    caches[bus->bus_requesting_id]->ack= flush_from_main_memory(caches[bus->bus_requesting_id], main_memory, bus->bus_requesting_address, bus,  index);
                    printf("\nack: %d\n",caches[bus->bus_requesting_id]->ack );
                    caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = EXCLUSIVE;

                    log_cache_state(caches[bus->bus_requesting_id] );
                    
                }

                break;


            case BUS_RDX:
                caches_owning_block_id_array = check_shared_bus(caches,bus->bus_requesting_id, bus->bus_requesting_address); // Check if the data is in another cache, if yes, return the cache id

                // Bus read exclusive operation : When a BusRdX (Bus Read Exclusive) transaction occurs on the bus, it indicates that a processor or cache is requesting a block of data from the memory system with the intent to write to it.
                printf("Snooping BUS_READ_EXCLUSIVE\n");

                bus->wr = 1; 
                //printf("Caches Owning Block IDs: %d\n", caches_owning_block_id_array[1]);
                if (caches_owning_block_id_array != NULL) {
                    // Data found in another cache
                     
                    if(caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == SHARED){ ////////should be a loop on the array!!!!!!!!!!!!!!!
                        {
                            printf("FOUND A SHARED BLOCK: bring it to the requesting cache! invalidate others\n", caches_owning_block_id_array);
                            //printf("size of array: %d\n", sizeof(caches_owning_block_id_array));
                            for(int i = 0; (caches_owning_block_id_array[i] <= 4 && caches_owning_block_id_array[i] >= 0);  i++)
                            {   
                                printf("data in cache %d is invalid, INVALIDATE block number: %d\n", caches_owning_block_id_array[i], index);
                                caches[caches_owning_block_id_array[i]]->tsram->cache[index].mesi_state = INVALID;  // Data is invalid in the cache with the shared data
                                
                            }
                        
                            caches[bus->bus_requesting_id]->ack= flush_from_main_memory(caches[bus->bus_requesting_id], main_memory, bus->bus_requesting_address, bus,  index);
                            caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = MODIFIED;  // Data is exclusive in the requesting cache (will be modified later)                    
                            caches[bus->bus_requesting_id]->dsram->cache[index].data[block_offset] = bus->bus_write_buffer; 
                            
                        }
                    }

                    else if (caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == EXCLUSIVE){
                        {
                            caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state = INVALID;  // Data is invalid in the cache with the shared data
                            printf("FOUND AN EXCLUSIVE BLOCK: Data found in cache %d, fetching from there...\n", caches_owning_block_id_array);
                            caches[bus->bus_requesting_id]->ack=flush_from_main_memory(caches[bus->bus_requesting_id],main_memory, bus->bus_requesting_address, bus, index);
                            caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = MODIFIED;  // Data is exclusive in the requesting cache (will be modified later)

                            bus->bus_shared = 0;  // Data is not shared
                        }
                    }

                    else if(caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state == MODIFIED){
                            printf("FOUND A MODIFIED BLOCK: Data found in cache %d, fetching from there...\n", caches_owning_block_id_array);
                            caches[bus->bus_requesting_id]->ack=flush_from_cache(caches[bus->bus_requesting_id], caches[caches_owning_block_id_array[0]], main_memory, bus->bus_requesting_address, bus, index);
                            caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = MODIFIED;  // Data is exclusive in the requesting cache (will be modified later)
                            caches[caches_owning_block_id_array[0]]->tsram->cache[index].mesi_state = INVALID;  // Data is invalid in the cache with the shared data
                            bus->bus_shared = 0;  // Data is not shared
                        }
                    }
                    else
                    {
                        //fetch from main memory 
                        caches[bus->bus_requesting_id]->ack=flush_from_main_memory(caches[bus->bus_requesting_id], main_memory, bus->bus_requesting_address, bus, index); 
                        printf("---------ack: %d", caches[bus->bus_requesting_id]->ack);
                        if(caches[bus->bus_requesting_id]->ack)
                            caches[bus->bus_requesting_id]->tsram->cache[index].mesi_state = MODIFIED;  // Data is exclusive in the requesting cache
                    }

                    break;

            case FLUSH:

                if(bus->bus_origid == 4)
                    caches[bus->bus_requesting_id]->ack = flush_from_main_memory(caches[bus->bus_requesting_id], main_memory, bus->bus_requesting_address, bus, index); 
                else
                    caches[bus->bus_requesting_id]->ack= flush_from_cache(caches[bus->bus_requesting_id], caches[bus->bus_origid], main_memory, bus->bus_requesting_address, bus,  index);
                

                if(!bus->busy)
                {


                    bus->bus_requesting_address=-1;
                    bus->bus_origid = -1;  // Set the originator of the bus transaction
                    bus->bus_cmd = 0;        // Set the bus command (e.g., BUS_RD, BUS_RDX, etc.)
                    bus->bus_addr = 0;       // Set the address for the bus operation
                    bus->bus_data = 0;          // Set the data for write operations (optional)
                    bus->bus_shared = 0;      // Set shared state for read operations (1 if shared, 0 if exclusive)
                    bus->wr=0;
                    bus->bus_write_buffer=0;
                    bus->stall=0;
                    log_cache_state(caches[bus->bus_requesting_id]);

                    //if (caches_owning_block_id_array!=NULL)
                      //  log_cache_state(caches[caches_owning_block_id_array[0]]);
                }
                break;


            default:
                printf("Unknown bus operation.\n");
            break;
            return caches[bus->bus_requesting_id]->ack; 
        }
}


/******************************************************************************
* Function: cache_read
*
* Description: read command. first check if the data is in the cache, if not, send a bus transaction.
* after that, all other caches will see the transaction. if it's in another cache, they will send the data to the bus and shared = 1. then we call read_from_bus
*******************************************************************************/
bool cache_read(CACHE* cache,  uint32_t address, uint32_t *data, MESI_bus *mesi_bus) {
    uint32_t tag, index, block_offset;
    int orig_id = cache->cache_id; 
    int cache_hit;
    get_cache_address_parts(address, &tag, &index, &block_offset);
    printf("address parts: %d %d %d %d\n", address, tag, index, block_offset);
    CacheLine *dsram_line = &cache->dsram->cache[index];
    CacheLine_TSRAM *tsram_line = &cache->tsram->cache[index];
    cache->ack = 0; 

    //3 options: cache hit, cache miss, data found in another cache, cache miss, data not found in any cache
    // 1.cache hit: valid block, tag matches, no command on the bus
    // 2.cache miss: send busrd transaction

    cache_hit = tsram_line->mesi_state != INVALID && tsram_line->tag == tag;
    
    if (cache_hit) //cache hit, valid block
    {
        *data = dsram_line->data[block_offset];
        printf("Cache hit! Data: %u\n", *data);
        cache->ack = 1; 
        return 1;
    }

    else { // Cache miss, check if the data is in another cache -> create bus transaction
        cache->ack=0;
        printf("Cache miss! sending busrd transaction...\n");
        send_op_to_bus(mesi_bus, orig_id, BUS_RD, address);  
        return 0;    
    }

        //log_cache_state(&dsram, &tsram);
}



/******************************************************************************
* Function: cache_write
*
* Description: write command. first check if the data is in the cache, if not, send a bus transaction(BUSRDX).
* after that, all other caches will see the transaction. if it's in another cache, they will send the data to the bus and shared = 1. then we call read_from_bus
*******************************************************************************/
int cache_write(CACHE* cache, uint32_t address,int data, MESI_bus *mesi_bus) {
    uint32_t tag, index, block_offset;
    int origid = cache->cache_id; 
    get_cache_address_parts(address, &tag, &index, &block_offset);
    CacheLine *dsram_line = &cache->dsram->cache[index];
    CacheLine_TSRAM *tsram_line = &cache->tsram->cache[index];
    uint64_t cycles = 0;
    // Snooping the bus to maintain cache coherence
    cache->ack = 0; 
    // Check if it's a cache hit
    printf("tsram: %d, tsramtag: %d, tag: %d\n", tsram_line->mesi_state, tsram_line->tag, tag);
    if (tsram_line->mesi_state != INVALID && tsram_line->tag == tag) {
        //printf("cache hit, tag:%d, tsram_state:%d\n", tsram_line->tag, tsram_line->mesi_state);
        if(tsram_line->mesi_state == MODIFIED || tsram_line->mesi_state == EXCLUSIVE)
        {
            printf("Cache write hit! Data is in state: %d, writing to cache...\n", tsram_line->mesi_state);
            dsram_line->data[block_offset] = data; 
            cache->ack = 1; 
            tsram_line->mesi_state = MODIFIED;  // Transition to MODIFIED because the cache line has been updated
            printf("Cache write hit!Block is now : Modified! Data written to index %u, block offset %u\n", index, block_offset);
            log_cache_state(cache);
            return 1;

        }
        else {
            printf("Cache write hit! found a shared block, sending busrdx transaction...\n");
            //cache miss! send busrdx to the bus. will be modified later
            // Check if the cache line is dirty and needs to be written back to memory
            log_cache_state(cache);  // Log both DSRAM and TSRAM states
            mesi_bus->bus_write_buffer = data; 
            send_op_to_bus(mesi_bus, origid, BUS_RDX, address); //send busrdx transaction
            log_cache_state(cache);  // Log both DSRAM and TSRAM states

            return BUS_RDX;
        }

        return 1;

        }
    else {
        printf("Cache write miss! sending busrdx transaction...\n");
        //cache miss! send busrdx to the bus. will be modified later
        // Check if the cache line is dirty and needs to be written back to memory
        mesi_bus->bus_write_buffer = data; 
        log_cache_state(cache);  // Log both DSRAM and TSRAM states
        send_op_to_bus(mesi_bus, origid, BUS_RDX, address); //send busrdx transaction
        return 0;
        }
    
    
}

