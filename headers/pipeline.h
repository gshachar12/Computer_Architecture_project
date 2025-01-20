#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state_machine.h"

/* Function prototypes */
int state_machine(Core *core, Command *com, int *stall, MESI_bus* mesi_bus, int* hazard);
int detect_data_hazard(DecodeBuffers *decode_buf, ExecuteBuffer *execute_buf, MESI_bus* mesi_bus);
int pipeline(Core* core, int clock, MESI_bus* mesi_bus, int* num_fetched_commands,  int* num_executed_commands, int* last_command, int* hazard); 

#endif /* PIPELINE_H */
