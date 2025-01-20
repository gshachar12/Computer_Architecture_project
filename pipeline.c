#include "headers/state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int state_machine(Core* core, MESI_bus* mesi_bus, int* hazard, int* num_fetched_commands ) {
    Command *com=core->current_instruction;
    switch (com->state) {
    case FETCH:
    (*num_fetched_commands)++; 
        if (fetch_instruction(core) == -1) {
            return -1; // No more instructions to fetch
        }

        break;

    case DECODE:
        *hazard = decode(core, com);
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

void nullify_command(Command *src) {

    // Copy string
    
    strcpy(src->inst , "DONE");
    // Copy primitive fields
    src->opcode = -1;
    src->rd = -1;
    src->rs = -1;
    src->rt = -1;
    src->rm = -1;
    src->imm = -1;
    src->hazard = -1;

    // Copy nested structure
    //dest->control_signals = rc->control_signals;
   
}

Command *copy_command(const Command *src) {
    // Allocate memory for the new Command
    Command *dest = (Command *)malloc(sizeof(Command));
    if (dest == NULL) {
        return NULL; // Return NULL if allocation fails
    }

    // Copy string
    strcpy(dest->inst, src->inst);

    // Copy primitive fields
    dest->opcode = src->opcode;
    dest->rd = src->rd;
    dest->rs = src->rs;
    dest->rt = src->rt;
    dest->rm = src->rm;
    dest->imm = src->imm;
    dest->state = src->state;
    dest->hazard = src->hazard;

    // Copy nested structure
    dest->control_signals = src->control_signals;

    return dest;
}


int pipeline(Core* core, int clock, MESI_bus* mesi_bus, int* num_fetched_commands,  int* num_executed_commands, int* last_command, int* hazard) {

    if(*num_executed_commands<core->IC)
    {
        int first_command;
        printf("\n%s-------------------------------------- CLOCK %d-------------------------------------------------------------%s\n", BLUE, clock, WHITE);
        printf("\ncore IC: %d\n", core->IC); 
        printf("\nnum executed commands %d\n", *num_executed_commands);
        printf("\nnum fetched commands %d\n", *num_fetched_commands);
         printf("%s \nhazard %d\n%s ", MAGENTA, *hazard, WHITE);
        *hazard = detect_raw_hazard(core); 
         if (*hazard)  
            first_command =EXEC;
        else
            first_command =FETCH;

        for(int i = FETCH; i<=WB;i++)
            core->pipeline_array[i]->state =i; 
        if (strcmp(core->pipeline_array[WB]->inst, "NOP") != 0 && strcmp(core->pipeline_array[WB]->inst, "DONE") != 0)
                (*num_executed_commands)++; 

        for(int i=WB-1; i>=first_command; i--)
        {
            core->pipeline_array[i+1] = copy_command(core->pipeline_array[i]); 
            core->pipeline_array[i+1]->state = i+1;  
        }
        

        if(*num_fetched_commands == core->IC)
        {
            
           // core->pipeline_array[first_command]->state =0; 
           nullify_command(core->pipeline_array[first_command]);
        }
        else 
        core->pipeline_array[0] =core->instruction_array[core->pc]; 

        printf("first command %d, last cocmmand %d", first_command, *last_command); 


        for(int i=first_command; i<=*last_command; i++)
        {

        if(strcmp(core->pipeline_array[i]->inst, "DONE")==0) 
            continue; 

        printf("\n%s ---------------------------------------------------------------------------------------------------%s\n", RED, WHITE);
        core->current_instruction = core->pipeline_array[i];
        printf("\npipeline inst %s\n", core->pipeline_array[i]->inst);
        state_machine( core,  mesi_bus, hazard, num_fetched_commands); 
        printf("state: %d\n",core->pipeline_array[i]->state );

        printf("\n%sState: %s %s%s\n", WHITE, GREEN, (char *[]){"fetch", "decode", "execute", "memory", "writeback"}[core->pipeline_array[i]->state], WHITE);

        }             

        if(*last_command<WB)
            (*last_command)++; 
        printf("last command %d\n", *last_command);
        for(int i=FETCH; i<=WB; i++)
        {
            printf("%s %s %s%s%s, ", WHITE, GREEN, core->pipeline_array[i]->inst, (char *[]){"fetch", "decode", "execute", "memory", "writeback"}[core->pipeline_array[i]->state], WHITE);
        }        
    return 0; 
    }

    return 1; 
}

    

