#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state_machine.h"
#include "initialize.h"

/* Function prototypes */
int state_machine(Core *core, Command *com, int *stall, MESI_bus* mesi_bus);
void detect_data_hazard(DecodeBuffers *decode_buf, ExecuteBuffer *execute_buf, MESI_bus* mesi_bus);
int pipeline(Core* core, int clock, MESI_bus* mesi_bus, int* last_command); 

#endif /* PIPELINE_H */
