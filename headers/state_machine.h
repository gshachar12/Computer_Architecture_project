#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "cpu_structs.h"
#include "utils.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/******************************************************************************
* Constants
*******************************************************************************/
#define NUM_REGS 16
#define SIZE_REG 32 
#define MEMORY_SIZE (1 << 20) // 2^20 words
#define INSTRUCTION_LENGTH 9

/******************************************************************************
* Struct Definitions
*******************************************************************************/

// Forward declaration of DSRAM
struct dsram;



/******************************************************************************
* Function Prototypes
*******************************************************************************/
void print_regout_array_to_file( Core* core);
void BuildCommand(char * command_line, Command * com);
int fetch_instruction(Core *core);
void detect_raw_hazard(Core* core); 

void nullify_command(Command *src);
int decode(Core *core, Command *com);
void execute(Core *core, Command *com);
void memory_state(Command *com, Core *core, MESI_bus* bus);
void writeback_state(Command *com, Core *core);

#endif // CPU_SIMULATION_H
