#include "headers/state_machine.h"
#include "headers/initialize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void detect_hazard(Core* core, MESI_bus* bus)
{   
    int RAW_hazard=0; // check for RAW hazard
    int MEM_hazard=0; // check for MEM hazard
 
    detect_raw_hazard(core); ///////////////////////////////////////////////////////////////////////////////////////////////// need to remove
    printf("\nbus busy: %d, memory stalls: %d\n", bus->busy, core->cache->memory_stalls); 
    if(bus->busy && core->cache -> memory_stalls)
        MEM_hazard = WB;
    core->hazard = MEM_hazard>core->hazard? MEM_hazard: core->hazard; 
}

int state_machine(Core* core, MESI_bus* mesi_bus) {
    Command *com=core->current_instruction;
    switch (com->state) {
    case FETCH:
        if (fetch_instruction(core) == -1) {
            return -1; // No more instructions to fetch
        }

        break;

    case DECODE:
        decode(core, com);
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
    dest->imm = src->imm;
    dest->state = src->state;
    dest->hazard = src->hazard;
    dest->jump_address = src->jump_address;

    // Copy nested structure
    dest->control_signals = src->control_signals;

    return dest;
}

int finished(Core* core)
{

    for(int i = FETCH; i<WB;i++)
        if(strcmp(core->pipeline_array[i]->inst, "DONE")!=0)
            return 0;
    return 1; 

}

int pipeline(Core* core, int clock, MESI_bus* mesi_bus, int* last_command) 
{
    printf("\nPC=%d\n", core->pc);
    detect_hazard(core, mesi_bus); 
    int first_command=core->hazard;
    printf("%s \nhazard %d\n%s ", MAGENTA, core->hazard, WHITE);
    for(int i = FETCH; i<=WB;i++)
        core->pipeline_array[i]->state =i; 

    for(int i=WB-1; i>=first_command; i--)
    {
        core->pipeline_array[i+1] = copy_command(core->pipeline_array[i]); 
        core->pipeline_array[i+1]->state = i+1;  
    }
    

    if(core->hazard>0 || core->halted || core->pc >= core->IC )
    {
        printf("core hazard: %d, core halted: %d, core pc: %d, IC: %d\n", core->hazard, core->halted, core->pc, core->IC);
        // core->pipeline_array[first_command]->state =0; 
        nullify_command(core->pipeline_array[first_command]);
    }
    else 
    {
        core->pipeline_array [0]= (Command *)malloc(sizeof(Command));
        initialize_command(core->pipeline_array[0]); 
    }

  
    for(int i=*last_command; i>=first_command; i--)
    {

    if(strcmp(core->pipeline_array[i]->inst, "DONE")==0) 
        continue; 

    printf("\n%s ---------------------------------------------------------------------------------------------------%s\n", RED, WHITE);
    
    core->current_instruction = core->pipeline_array[i];
    state_machine( core,  mesi_bus); 
    // if(core->current_instruction->opcode == 15 && i > DECODE)
    // {
    //     printf("%s jump address in pipeline: %d, opcode: %d %s\n",RED, core->current_instruction->jump_address, core->current_instruction->opcode, WHITE);
    //     exit(1);
    // }
    //printf("%s jump address in pipeline: %d, opcode: %d %s\n",RED, core->current_instruction->jump_address, core->current_instruction->opcode, WHITE);
    printf("state: %d\n",core->pipeline_array[i]->state );
    printf("\n%sState: %s %s%s\n", WHITE, GREEN, (char *[]){"fetch", "decode", "execute", "memory", "writeback"}[core->pipeline_array[i]->state], WHITE);
    } 


    if(strcmp(core->pipeline_array[DECODE]->inst, "DONE")!=0 && strcmp(core->pipeline_array[DECODE]->inst, "NOP")!=0)
    {
        core->current_instruction = core->pipeline_array[DECODE];   
        state_machine( core,  mesi_bus); 

    }

  

    if(*last_command<WB)
        (*last_command)++; 
    for(int i=FETCH; i<=WB; i++)
    {
        printf("%s %s %s %s %s, ", WHITE, GREEN, core->pipeline_array[i]->inst, (char *[]){"fetch", "decode", "execute", "memory", "writeback"}[core->pipeline_array[i]->state], WHITE);
    }        

        if(core->pipeline_array[DECODE]->btaken == 1)
        {
        core->pc = (core->decode_buf->rd_value & 0x3FF);
        printf("\n\nBranch taken. PC=%d\n\n", core->pc); 
        }

    
    return finished(core); 
}