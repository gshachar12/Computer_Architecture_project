
#include "headers/cpu_structs.h"
#define MAIN_MEMORY_SIZE (1 << 20) // Define the size as 2^20 (1 MB)
void initialize_main_memory(MainMemory *main_memory, FILE *memin, FILE *memout) {
    // Allocate memory for the main memory array
    printf("Allocating memory for main memory...\n");
    main_memory->memory_data = (int *)malloc(MAIN_MEMORY_SIZE * sizeof(int));
    if (main_memory->memory_data == NULL) {
        fprintf(stderr, "Failed to allocate memory for main memory.\n");
        return;
    }

    // Initialize all memory locations to zero
    memset(main_memory->memory_data, 0, MAIN_MEMORY_SIZE * sizeof(int));

    // Assign file pointers to the struct
    main_memory->memin = memin;
    main_memory->memout = memout;

    // Read from the memin file and populate main memory
    int index = 0;
    if (main_memory->memin != NULL) {
        while (fscanf(main_memory->memin, "%x", &main_memory->memory_data[index]) == 1) {
            index++;
            if (index >= MAIN_MEMORY_SIZE) {
                fprintf(stderr, "Warning: memin file contains more data than MAIN_MEMORY_SIZE. Truncating.\n");
                break;
            }
        }
        printf("Loaded %d addresses from memin file.\n", index);
    } else {
        fprintf(stderr, "memin file is NULL. Skipping file load.\n");
    }

    printf("Main memory initialized with %d addresses.\n", MAIN_MEMORY_SIZE);
}



void initialize_DSRAM(DSRAM *dsram, FILE *log_file) {
    // Initialize cache data

    for (int i = 0; i < NUM_BLOCKS; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            dsram->cache[i].data[j] = 0;  // Clear data
        }
    }
    dsram->cycle_count = 0;  // Reset cycle count

    // Open logfile for DSRAM
    dsram->logfile = log_file;
    if (dsram->logfile == NULL) {
        perror("Error opening log file for DSRAM");
        exit(EXIT_FAILURE);
    }

}

// Function to initialize TSRAM
void initialize_TSRAM(TSRAM *tsram, FILE *log_file) {


    // Initialize cache state and tags
    for (int i = 0; i < NUM_BLOCKS; i++) {
        tsram->cache[i].tag = 0;  // Clear the tag 
        tsram->cache[i].mesi_state = INVALID;  // Set state to INVALID
    }
    tsram->cycle_count = 0;  // Reset cycle count
    // Open logfile for TSRAM
    tsram->logfile = log_file;
    if (tsram->logfile == NULL) {
        perror("Error opening log file for TSRAM");
        exit(EXIT_FAILURE);
    }
}


void initialize_mesi_bus(MESI_bus *bus, FILE *log_file) 
{
    bus->bus_origid = 0;  // Set the originator of the bus transaction
    bus->bus_cmd = 0;        // Set the bus command (e.g., BUS_RD, BUS_RDX, etc.)
    bus->bus_addr = 0;       // Set the address for the bus operation
    bus->bus_data = 0;          // Set the data for write operations (optional)
    bus->bus_shared = 0;      // Set shared state for read operations (1 if shared, 0 if exclusive)
    bus->wr=0;
    bus->bus_write_buffer=0;
    bus->stall=0;
    bus->busy=0;
    bus->logfile = log_file;
    bus->bus_requesting_address=0;
    // initializeQueue(bus->bus_queue);
    if (bus->logfile == NULL) {
        perror("Error opening log file for DSRAM");
        exit(EXIT_FAILURE);
    }
    printf("gdalia hamelech2");
    fprintf(bus->logfile, "Cyc Orig Cmd    Addr      Data  Shared\n");	
    printf("\nFinished initializing mesi_bus\n");

}


void initialize_regout_array(char (*regout_array)[9]) {
    // Initialize each register with 8 '0' characters
    for (int i = 0; i < NUM_REGS; i++) {
        for (int j = 0; j < 8; j++) {
            regout_array[i][j] = '0';  // Fill with '0' character
        }
        regout_array[i][8] = '\0';  // Null terminate the string
    }
}


void initialize_command(Command* cmd)
{
        // Initialize current_instruction fields


    strcpy(cmd->inst, "NOP");
    cmd->opcode = 25;
    cmd->rd = 0;
    cmd->rs = 0;
    cmd->rt = 0;
    cmd->btaken =0; 
    cmd->jump_address = 0;
    cmd->rd_value = 0;
    cmd->rs_value = 0;
    cmd->rt_value = 0;

    cmd->imm = 0;
    cmd->state = 0;
    cmd->hazard = 0; 
    if (cmd == NULL) {
        perror("Memory allocation failed for instruction_array[i]");
        exit(1);
    }
}

void initialize_pipeline_array(Command** pipeline_array, int instruction_count)
{

    for (int i = 0; i < instruction_count; i++) {

        pipeline_array[i] = (Command *)malloc(sizeof(Command));
        if (pipeline_array[i] == NULL) {
            perror("Memory allocation failed for cmd");

            // Free already allocated memory
            for (int j = 0; j < i; j++) {
                free(pipeline_array[j]);
            }

            return; // Exit the function to indicate failure
        }
        initialize_command(pipeline_array[i]);
        // pipeline_array[i]->state = i;  

    }

}


void initialize_instruction_array(Command** instruction_array, int instruction_count, FILE* instruction_file)
{
    char instruction[INSTRUCTION_LENGTH + 1]; // Buffer for fetched instruction

    if (instruction_file == NULL) {
        perror("Instruction file is not open");
        return; // Failure
    }  

    // Fetch the instruction using fgets

    for (int i = 0; i < instruction_count; i++) 
    {
        instruction_array[i] = (Command *)malloc(sizeof(Command));
        
        if (instruction_array[i] == NULL) 
        {
            perror("Memory allocation failed for cmd");

            // Free already allocated memory
            for (int j = 0; j < i; j++) {
                free(instruction_array[j]);
            }

            return; // Exit the function to indicate failure
        }


        initialize_command(instruction_array[i]);
        if (fgets(instruction, sizeof(instruction), instruction_file) != NULL) 
        {
            instruction[strcspn(instruction, "\n")] = '\0';
            strcpy(instruction_array[i]->inst, instruction);  
        }


    }
    

}


void initialize_core_buffers(Core* core)
{
        // Initialize buffers to zero or default values
    core->decode_buf->rs_value = 0;
    core->decode_buf->rt_value = 0;
    core->decode_buf->rd_value = 0;
    core->decode_buf->rs = -1;
    core->decode_buf->rt = -1;
    core->decode_buf->rd = -1;
    core->decode_buf->is_branch = 0;

    core->execute_buf->alu_result = 0;
    core->execute_buf->mem_address = 0;
    core->execute_buf->rd_value = 0;
    core->execute_buf->destination = 0;
    core->execute_buf->memory_or_not = 0;
    core->execute_buf->mem_busy = 0;
    core->execute_buf->branch_resolved = 0;
    core->execute_buf->is_branch = 0;
    core->mem_buf.load_result = 0;
    core->mem_buf.destination_register = 0;
    core->mem_buf.address = 0;
    core->wb_buf->finished=0; 
}
void initialize_cache(CACHE* cache, FILE *DSRAM_log_filename, FILE *TSRAM_log_filename, int cache_id)
{
    // Initialize cache fields
    cache->cache_id = cache_id;
    cache->ack = 0;  // Initially no acknowledgment

    // Allocate and initialize DSRAM and TSRAM
    cache->dsram = (DSRAM *)malloc(sizeof(DSRAM));
    if (cache->dsram == NULL) {
        perror("Memory allocation failed for DSRAM");
        exit(EXIT_FAILURE);
    }

    cache->tsram = (TSRAM *)malloc(sizeof(TSRAM));
    if (cache->tsram == NULL) {
        perror("Memory allocation failed for TSRAM");
        exit(EXIT_FAILURE);
    }

    initialize_DSRAM(cache->dsram, DSRAM_log_filename);
    initialize_TSRAM(cache->tsram, TSRAM_log_filename);
}

void initialize_core(Core* core, int core_id, int instruction_count, FILE* imem_file, FILE* DSRAM_log_filename, FILE* TSRAM_log_filename, FILE* regout, FILE* status_file, FILE* trace_file) {
    core->cache = (CACHE *)malloc(sizeof(CACHE));  // Allocate memory for the Core struct
    core->core_id = core_id;
    core->hazard               = 0; 
    core->pc                   = 0;
    core->halted               = 0;
    core->requesting           = 0; 
    core->read_miss_counter    = 0; 
    core->write_miss_counter   = 0; 
    core->read_hit_counter     = 0; 
    core->write_hit_counter    = 0; 
    core->decode_stall_counter = 0; 
    core->mem_stall_counter    = 0;
    core->regout_file = regout; 
    core->IC = instruction_count;
    core->instruction_file = imem_file;
    core->status_file = status_file; 
    core->trace_file = trace_file; 

        // Allocate memory for instruction_array (pointer to an array of Command pointers)
    core->instruction_array = (Command **)malloc(instruction_count * sizeof(Command *));
    if (core->instruction_array == NULL) {
        perror("Memory allocation failed for instruction_array");
        free(core->instruction_array);  // Free previously allocated memory
    }
        core->pipeline_array = (Command **)malloc(5 * sizeof(Command *));
    if (core->pipeline_array == NULL) {
        perror("Memory allocation failed for instruction_array");
        free(core->pipeline_array);  // Free previously allocated memory
    }
    
    core->decode_buf = (DecodeBuffers *)malloc(instruction_count * sizeof(DecodeBuffers));

    core->execute_buf = (ExecuteBuffer *)malloc(instruction_count * sizeof(ExecuteBuffer));
    core->wb_buf = (WriteBackBuffer *)malloc(instruction_count * sizeof(WriteBackBuffer));


    initialize_instruction_array(core->instruction_array, core->IC, core->instruction_file); 
    initialize_pipeline_array(core->pipeline_array, 5); 
    initialize_regout_array(core->regout_array); 
        core->current_instruction = (Command *)malloc(sizeof(Command));
        
        if (core->current_instruction== NULL) {
            perror("Memory allocation failed for cmd");
            return; // Exit the function to indicate failure
        }

    initialize_command(core->current_instruction);

    initialize_core_buffers(core);

    initialize_cache(core->cache,DSRAM_log_filename, TSRAM_log_filename, core->core_id ); 

}