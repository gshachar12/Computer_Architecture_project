#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#include "cpu_structs.h" 


// Function declarations
void initialize_main_memory(MainMemory* main_memory, FILE* memin, FILE* memout );
void initialize_DSRAM(DSRAM *dsram, FILE *log_file);
void initialize_TSRAM(TSRAM *tsram, FILE *log_file);
void initialize_mesi_bus(MESI_bus *bus, FILE *log_file);
void initialize_regout_array(char regout_array[][9]);
void initialize_command(Command *cmd);
void initialize_instruction_array(Command **instruction_array, int instruction_count);
void initialize_pipeline_array(Command **pipeline_array, int instruction_count);
void initialize_core_buffers(Core *core);
void initialize_cache(CACHE *cache, FILE *DSRAM_log_filename, FILE *TSRAM_log_filename, int core_id);
void initialize_core(Core* core, int core_id, int instruction_count, FILE* imem_file, FILE* DSRAM_log_filename, FILE* TSRAM_log_filename, FILE* regout, FILE* status_file,  FILE* trace_file);
#endif // INITIALIZATION_H
