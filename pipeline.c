#include "headers/state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int state_machine(Core* core, MESI_bus* mesi_bus, int* hazard) {
    Command *com=core->current_instruction;
    switch (com->state) {
    case FETCH:

        if (fetch_instruction(core) == -1) {
            return -1; // No more instructions to fetch
        }

        break;

    case DECODE:
        decode(core, com);
        printf("hazard  %d", *hazard);
        if(!*hazard)
        printf("decode buffer: rd value - %d", core->decode_buf->rd_value);
        
        printf("\nDECODE PIPELINE Command properties: ");
        printf("opcode: %d, rd: %d, rs: %d, rt: %d, imm: %d\n", com->opcode, com->rd, com->rs, com->rt, com->imm);
        break;

    case EXEC:

        execute(core, com);
        printf("\nExecute PIPELINE Command properties: \n");
        printf("opcode: %d, rd: %d, rs: %d, rt: %d, imm: %d\n", com->opcode, com->rd, com->rs, com->rt, com->imm);
        printf("execute buffer: mem address - %d", core->execute_buf->mem_address);
        printf("execute buffer: alu result - %d", core->execute_buf->alu_result);
        break;

    case MEM:
        memory_state(com, core, mesi_bus);
        // (*hazard) = detect_hazard(core, mesi_bus);
        break;

    case WB:
       // writeback_state(com, core);
        writeback_state(com, core);
        break;

    default:
        fprintf(stderr, "Error: Invalid pipeline state: %d\n", com->state);
        return -1;
    }

    return 0; // Indicate success
}


int pipeline(Core* core, int clock, MESI_bus* mesi_bus, int* num_fetched_commands,  int* num_executed_commands, int* first_command, int* last_command, int* hazard) {

    if(*num_executed_commands<core->IC)
    {
        printf("\n%s-------------------------------------- CLOCK %d-------------------------------------------------------------%s\n", BLUE, clock, WHITE);
        printf("\ncore IC: %d\n", core->IC); 
        printf("\nnum executed commands %d\n", *num_executed_commands);
        printf("\nnum fetched commands %d\n", *num_fetched_commands);


        for(int i=3; i>=0; i--)
        {
        if (core->pipeline_array[i+1]->state == WB)
            (*num_executed_commands)++; 

           core->pipeline_array[i+1] =core->pipeline_array[i];   

        }
        if(*num_fetched_commands == core->IC)
            (*first_command)++; 
        else
            core->pipeline_array[0]= core->instruction_array[core->pc]; 
        printf("\nlast command: %d", *last_command); 
        printf("\nfirst command: %d", *first_command); 

        for(int i=*last_command; i>=*first_command ; i--)
        {
        printf("\n%s ---------------------------------------------------------------------------------------------------%s\n", RED, WHITE);
        core->current_instruction = core->pipeline_array[i];
        if(core->pipeline_array[i]->state == FETCH)
            (*num_fetched_commands)++; 
        state_machine( core,  mesi_bus, hazard); 
        printf("\npipeline inst %s\n", core->pipeline_array[i]->inst);
        printf("\n%sState: %s%s%s\n", WHITE, GREEN, (char *[]){"fetch", "decode", "execute", "memory", "writeback"}[core->pipeline_array[i]->state], WHITE);

        core->pipeline_array[i]->state++; 


        }
        


        if(*last_command<4)
            (*last_command)++; 
    return 0; 
    }
    return 1; 
}

    

