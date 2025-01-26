#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/asm.h"
#include "headers/pipeline.h"
// Function to initialize Core
#define NUM_ARGS 32
static int lastGrantedCore = -1;
int read_miss_counter = 0; 
int read_hit_counter = 0; 
int write_miss_counter = 0; 
int write_hit_counter = 0; 
int decode_stall_counter = 0; 
int mem_stall_counter =0;
void log_cache_status(Core* core, int clock)
{
   fprintf(core->status_file, "%d FETCH DECODE EXEC MEM WB R2 R3 R4 R5 R6 R7 R8 R9 R10 R11 R12 R13 R14 R15");
   fprintf (core->status_file, "cycles %d ",clock );
   fprintf (core->status_file, "\ninstructions %d", core->IC);
   fprintf (core->status_file, "\nread_hit %d ", read_hit_counter);
   fprintf (core->status_file, "\nwrite_hit %d", write_hit_counter);
   fprintf (core->status_file, "\nread_miss %d", read_miss_counter);
   fprintf (core->status_file, "\nwrite_miss %d", write_miss_counter);
   fprintf (core->status_file, "\ndecode_stall %d",decode_stall_counter );
   fprintf (core->status_file, "\nmem_stall %d", mem_stall_counter);
}

bool busAvailable(MESI_bus* bus) {
    if(bus->bus_cmd == NO_COMMAND && bus->stall == 0) {
        return true;
    }
    
    return false;
}

int roundRobinArbitrator(MESI_bus* bus, int busRequests[NUM_CORES]) {
    if (!busAvailable(bus)) {
        return -1;
    }

    // round robin
    for (int i = 0; i < NUM_CORES; i++) {
        int coreId = (lastGrantedCore + i) % NUM_CORES;
       // if (busRequests[coreId]) {
            lastGrantedCore = coreId;
            return coreId;
        //}
    }
    return -1;
}


int simulate_cores( Core* cores[], MESI_bus* bus, MainMemory* main_memory)
{
    printf("%d Loaded Instructions\n\n", cores[0]->IC);

    CACHE* caches[] = {cores[0]->cache, cores[1]->cache, cores[2]->cache, cores[3]->cache};
    Core* core = cores[0]; 

    const int max_cycles = 10000; // Prevent infinite loops
    int finished =0; 
    int clock =0;
    int last_command = 0;
    int busRequests[NUM_CORES]; 
    int core_id;
    while(clock<max_cycles && !finished)
    {

        printf("\n%s-------------------------------------- CLOCK %d-------------------------------------------------------------%s\n", BLUE, clock, WHITE);

        for (int core_id=0; core_id < NUM_CORES-2; core_id++)
        {


        printf("\n%sRunning core: %d%s\n", BRIGHT_CYAN,core_id, WHITE );
        //core_id = roundRobinArbitrator(bus, busRequests);
        //core = cores[core_id]; 
        finished = pipeline( cores[core_id], clock, bus, &last_command);
        log_mesibus(bus, clock);
        log_cache_status(cores[core_id], clock); 
        snoop_bus(caches, bus, main_memory, clock); //main memory data should be fetched to cache0
        printf("%s\n\n\n------------------------------------------------------------------------------------ %s\n\n", RED, WHITE, clock);
        printf("\n\n");
        }
        
        clock++; 

        
    }

    for (int core_id=0; core_id < NUM_CORES; core_id++)
    {
        print_regout_array_to_file(cores[core_id]);
    }
}

int main(int argc, char *argv[])
{
    char* program; 
    char* imem_file;
    char* dmem_file;
    MainMemory main_memory;
    Core core0, core1, core2, core3;
    Core* cores[] = {&core0, &core1, &core2, &core3};
    MESI_bus mesi_bus;

    // check that we have 27 cmd line parameters
	if (argc != NUM_ARGS) {
		printf("\nincorrect usage, num args is supposed to be %d but %d were given\n", NUM_ARGS, argc);
		exit(1);
	}

    FILE* imem0 = fopen(argv[1], "wt");
	FILE* imem1 = fopen(argv[2], "wt");
    FILE* imem2 = fopen(argv[3], "wt");
	FILE* imem3 = fopen(argv[4], "wt");

    FILE* memin = fopen(argv[5], "rt");
    FILE* memout = fopen(argv[6], "wt");

    FILE* regout0 = fopen(argv[7], "wt");
	FILE* regout1 = fopen(argv[8], "wt");
    FILE* regout2 = fopen(argv[9], "wt");
	FILE* regout3 = fopen(argv[10], "wt");

    FILE* core0trace = fopen(argv[11], "wt");
	FILE* core1trace = fopen(argv[12], "wt");
    FILE* core2trace = fopen(argv[13], "wt");
    FILE* core3trace = fopen(argv[14], "wt");

    FILE* bustrace = fopen(argv[15], "wt");

    FILE* dsram0_logfile = fopen(argv[16], "wt");
    FILE* dsram1_logfile = fopen(argv[17], "wt");
    FILE* dsram2_logfile = fopen(argv[18], "wt");
    FILE* dsram3_logfile = fopen(argv[19], "wt");

    FILE* tsram0_logfile = fopen(argv[20], "wt");
    FILE* tsram1_logfile = fopen(argv[21], "wt");
    FILE* tsram2_logfile = fopen(argv[22], "wt");
    FILE* tsram3_logfile = fopen(argv[23], "wt");

    FILE* stats0 = fopen(argv[24], "wt");
    FILE* stats1 = fopen(argv[25], "wt");
    FILE* stats2 = fopen(argv[26], "wt");
    FILE* stats3 = fopen(argv[27], "wt");


    FILE* program0 = fopen(argv[28], "rt");
    FILE* program1 = fopen(argv[29], "rt");
    FILE* program2 = fopen(argv[30], "rt");
    FILE* program3 = fopen(argv[31], "rt");

    main_memory.memory_data = (int *)malloc(MAIN_MEMORY_SIZE * sizeof(int));
    FILE* dsram_log_files[] = {dsram0_logfile, dsram1_logfile, dsram2_logfile, dsram3_logfile};
    FILE* tsram_log_files[] = {tsram0_logfile, tsram1_logfile, tsram2_logfile, tsram3_logfile};
    FILE* imem_files[] = {imem0, imem1, imem2, imem3};
    FILE* status_files[] = {stats0, stats1, stats2, stats3}; 
    FILE* register_files[] = {regout0, regout1, regout2, regout3};
    FILE* programs[] = {program0, program1, program2, program3};
    int instruction_counts[4]; 
    initialize_main_memory(&main_memory, memin, memout);  // need to initialize from memin!!!!!!
    initialize_mesi_bus(&mesi_bus, bustrace);

    for (int core_id=0; core_id < NUM_CORES; core_id++)
    {
        instruction_counts[core_id] = interpret_file(programs[core_id], imem_files[core_id]);
    }


    fclose(imem0);
    fclose(imem1);
    fclose(imem2);
    fclose(imem3);


    imem0 = fopen(argv[1], "rt");
	imem1 = fopen(argv[2], "rt");
    imem2 = fopen(argv[3], "rt");
	imem3 = fopen(argv[4], "rt");

    for(int core_id = 0; core_id<NUM_CORES;core_id++)
    {
        initialize_core(cores[core_id], core_id,  instruction_counts[core_id] ,  imem_files[core_id], dsram_log_files[core_id], tsram_log_files[core_id], register_files[core_id], status_files[core_id]);
        printf("Core %d initialized.\n", core_id);
    }


    printf("%d Loaded Instructions\n\n", cores[0]->IC);
    simulate_cores( cores, &mesi_bus, &main_memory );
    log_main_memory(&main_memory);


    // close files

    fclose(imem0);
    fclose(imem1);
    fclose(imem2);
    fclose(imem3);

    fclose(memin);
    fclose(memout);

    fclose(regout0);
    fclose(regout1);
    fclose(regout2);
    fclose(regout3);

    fclose(core0trace);
    fclose(core1trace);
    fclose(core2trace);
    fclose(core3trace);

    fclose(bustrace);

    fclose(dsram0_logfile);
    fclose(dsram1_logfile);
    fclose(dsram2_logfile);
    fclose(dsram3_logfile);

    fclose(tsram0_logfile);
    fclose(tsram1_logfile);
    fclose(tsram2_logfile);
    fclose(tsram3_logfile);

    fclose(stats0);
    fclose(stats1);
    fclose(stats2);
    fclose(stats3);

	
    return EXIT_SUCCESS;
     
}
