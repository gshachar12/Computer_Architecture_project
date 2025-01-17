#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/asm.h"
#include "headers/pipeline.h"
#include "headers/initialize.h"
// Function to initialize Core

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


int simulate_cores( Core* cores[], MESI_bus* bus, MainMemory* main_memory)
{
    printf("%d Loaded Instructions\n\n", cores[0]->IC);

    const int max_cycles = 10000; // Prevent infinite loops
    CACHE* caches[] = {cores[0]->cache, cores[1]->cache, cores[2]->cache, cores[3]->cache};
    int finished =0; 
    int clock =0;
    int first_command = 0;
    int last_command = 0;
    int hazard=0; 
    int saved_last_command;
    Core* core = cores[0]; 
    // Print the content of the array

    int num_executed_commands=0;
    while(clock<max_cycles && !finished)
    {

        finished = pipeline(cores[0], clock, bus, &num_executed_commands, &first_command, &last_command, &hazard);
        snoop_bus(caches, bus, main_memory, clock); //main memory data should be fetched to cache0
        log_mesibus(bus, clock);
        printf("%s\n\n\n------------------------------------------------------------: %s\n\n", BLUE, WHITE, clock);
        printf("\n\n");
        clock++; 
    }
print_register_file_to_file("log_files/register_log.txt", core);
printf("execute buffer alu out %d", core->execute_buf->alu_result);
}


int main(int argc, char *argv[])
{
    char* program; 
    char* imem_file;
    char* dmem_file;
    char* pipeline_log_file ="log_files/pipeline_log.txt";
    MainMemory main_memory;
    Core core0, core1, core2, core3;
    Core* cores[] = {&core0, &core1, &core2, &core3};
    MESI_bus mesi_bus;

    printf("welcome\n");

    // check that we have 3 cmd line parameters
	if (argc != 4) {
		printf("usage: asm program.asm imem.txt dmem.txt\n");
		exit(1);
	}
    printf("files are: %s, %s, %s\n", argv[1], argv[2],argv[3] );

    FILE* fp_asm = fopen(argv[1], "rt");
	FILE* fp_imemout = fopen(argv[2], "r+");
	FILE* fp_dmemout = fopen(argv[3], "wt");

	if (!fp_asm || !fp_imemout || !fp_dmemout) {
		printf("ERROR: couldn't open files\n");
		exit(1);
	}
    
    FILE* tsram0_logfile = fopen("log_files/tsram0.txt", "wt");
    FILE* tsram1_logfile = fopen("log_files/tsram1.txt", "wt");
    FILE* tsram2_logfile = fopen("log_files/tsram2.txt", "wt");
    FILE* tsram3_logfile = fopen("log_files/tsram3.txt", "wt");

    FILE* dsram0_logfile = fopen("log_files/dsram0.txt", "wt");
    FILE* dsram1_logfile = fopen("log_files/dsram1.txt", "wt");
    FILE* dsram2_logfile = fopen("log_files/dsram2.txt", "wt");
    FILE* dsram3_logfile = fopen("log_files/dsram3.txt", "wt");
    FILE* dsram_log_files[] = {dsram0_logfile, dsram1_logfile, dsram2_logfile, dsram3_logfile};
    FILE* tsram_log_files[] = {tsram0_logfile, tsram1_logfile, tsram2_logfile, tsram3_logfile};
    printf("tsram and dsram files are initialized");
    main_memory.memory_data = (int *)malloc(MAIN_MEMORY_SIZE * sizeof(int));

    initialize_main_memory(&main_memory, "mem_files/main_memory.txt");
    printf("\nFinished initializing main_memory\n");
    initialize_mesi_bus(&mesi_bus, "mem_files/mesi_bus.txt");
    printf("\nFinished initializing mesi_bus\n");


    int instruction_count = interpret_file(fp_asm, fp_imemout, fp_dmemout);

    char instruction[INSTRUCTION_LENGTH + 1]; // Buffer for fetched instruction

    
    
    char buffer[100];

    for (int core_id=0; core_id < NUM_CORES; core_id++)
    {
        initialize_core(cores[core_id], core_id, instruction_count, fp_imemout,pipeline_log_file,dsram_log_files[core_id], tsram_log_files[core_id] ); 
        printf("Core %d initialized.\n", core_id);
    }
    
    printf("%d Loaded Instructions\n\n", cores[0]->IC);


    simulate_cores( cores, &mesi_bus, &main_memory );


    // close files
	fclose(fp_asm);
	fclose(fp_imemout);
	fclose(fp_dmemout);
    fclose(tsram0_logfile);
    fclose(tsram1_logfile);
    fclose(tsram2_logfile);
    fclose(tsram3_logfile);
    fclose(dsram0_logfile);
    fclose(dsram1_logfile);
    fclose(dsram2_logfile);
    fclose(dsram3_logfile);
	
    return EXIT_SUCCESS;
     
}
