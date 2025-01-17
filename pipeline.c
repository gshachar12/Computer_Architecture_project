#include "headers/state_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int detect_hazard(Core *core, MESI_bus* mesi_bus) {
    // Extract the buffers for easier reference
    DecodeBuffers *decode_buf = core->decode_buf;
    ExecuteBuffer *execute_buf = core->execute_buf;
    MemBuffer *mem_buf = &core->mem_buf;

    // // Data Hazard Detection (Read-After-Write - RAW)
    // if ((decode_buf->rs == execute_buf->destination && execute_buf->destination != 0) ||
    //     (decode_buf->rs == mem_buf->destination_register && mem_buf->destination_register != 0)) {
    //     printf("Data hazard detected: RAW on rs\n");
    //     return 1; // Stall
    // }
    // if ((decode_buf->rt == execute_buf->destination && execute_buf->destination != 0) ||
    //     (decode_buf->rt == mem_buf->destination_register && mem_buf->destination_register != 0)) {
    //     printf("Data hazard detected: RAW on rt\n");
    //     return 1; // Stall
    // }

    // // Control Hazard Detection
    // if (decode_buf->is_branch && !execute_buf->branch_resolved) {
    //     printf("Control hazard detected: Branch not resolved\n");
    //     return 1; // Stall
    // }

    // // Write-After-Write (WAW) Hazard Detection
    // if ((decode_buf->rd == execute_buf->destination && execute_buf->destination != 0) ||
    //     (decode_buf->rd == mem_buf->destination_register && mem_buf->destination_register != 0)) {
    //     printf("WAW hazard detected\n");
    //     return 1; // Stall
    // }

    // // Write-After-Read (WAR) Hazard Detection
    // if ((decode_buf->rs == mem_buf->destination_register && mem_buf->destination_register != 0) ||
    //     (decode_buf->rt == mem_buf->destination_register && mem_buf->destination_register != 0)) {
    //     printf("WAR hazard detected\n");
    //     return 1; // Stall
    // }

    // if(!core->cache->ack && mesi_bus->bus_requesting_id==core->cache->cache_id)
    // {
    //     printf("requesting core waiting transaction to finish mesi bus hazard\n ");
    //     return 1; 
    // }
    printf ("command %d stall %d", mesi_bus->bus_cmd, mesi_bus->stall);
    if(!(mesi_bus->bus_cmd == NO_COMMAND && !mesi_bus->busy))
    {
        printf("mesi bus is busy hazard\n ");
        return 1; 
    }

    // No hazards detected
    return 0;
}


int state_machine(Core* core, int num, char* log_file, MESI_bus* mesi_bus, int* hazard) {
    Command *com=core->current_instruction;
    switch (com->state) {
    case FETCH:

        if (fetch_instruction(core,num) == -1) {
            return -1; // No more instructions to fetch
        }

        modify_file_line(log_file, num + 1, "f ");
        break;

    case DECODE:
        // hazard = detect_hazard(core, mesi_bus);
        // if (hazard) {
        //     modify_file_line("pipeline_output->txt", num + 1, "- "); // Indicate stall
        //     return STALL;
        // }

        decode(core, core->instruction_array[num] );
        modify_file_line(log_file, num + 1, "d ");
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

        modify_file_line(log_file, num + 1, "e ");

        break;

    case MEM:


        if(!*hazard)
        {

            memory_state(com, core, mesi_bus);
        }
        (*hazard) = detect_hazard(core, mesi_bus);

        modify_file_line(log_file, num + 1, "m ");

        break;

    case WB:
       // writeback_state(com, core);
        writeback_state(com, core);

        modify_file_line(log_file, num + 1, "wb");

        break;

    default:
        fprintf(stderr, "Error: Invalid pipeline state: %d\n", com->state);
        return -1;
    }

    return 0; // Indicate success
}


int pipeline( Core* core, int clock, MESI_bus* mesi_bus, int* num_executed_commands, int* first_command, int* last_command, int* hazard) {
  
    int output;
    	
    char line[MAX_LINE_LENGTH];  // Array to store the line
    if (*num_executed_commands < core -> IC) {
        printf("%s\n\n\n-------------------------- Cycle number %d ------------------------: %s\n\n", BLUE, clock, WHITE);
        // Process commands in the pipeline
        for (int j = *first_command; j <= *last_command; j++) {
                    // Handle Write-back (WB) state

            core->current_instruction = core->instruction_array[j];
            if (core->instruction_array[j]->state == WB) {
                (*first_command)++;          // Move first command forward
                (*num_executed_commands)++;  // Count completed commands
                
            }

            printf("\n%s -------------------------- Command instruction: %s ------------------------: \n%s", RED, core->instruction_array[j]->inst, WHITE);

           
            state_machine(core, j, core->log_file, mesi_bus, hazard);
            printf("\n%sState: %s%s%s\n", WHITE, GREEN, (char *[]){"fetch", "decode", "execute", "memory", "writeback"}[core->instruction_array[j]->state], WHITE);
            printf("\nopcode: %d, rd: %d, rs: %d, rt: %d, imm: %d\n", core->instruction_array[j]->opcode, core->instruction_array[j]->rd,
                            core->instruction_array[j]->rs, core->instruction_array[j]->rt, core->instruction_array[j]->imm);
            printf("\nhazard %d\n",*hazard);

            printf("\n the immediate value is %d",  *(core->register_file[1]));
            // Advance command state if no stall
            if (!*hazard && core->instruction_array[j]->state < WB) {  //!stall && 

                core->instruction_array[j]->state++;  // Advance to next stage
            }


                    // If no stall and more commands exist, load the next command
            if (*hazard){
                *last_command = j; 
                modify_file_line(core->log_file, j + 1, "-- ");}


            // Handle end of instruction fetch
            if (output == -1) {
                hazard = 0; // Clear stall if no more instructions
                break;
            }
            


 printf("\n%s ------------------------------------------------------------------------------: \n%s", RED, WHITE);
        }   



        if (*last_command < core->IC -1) {
            (*last_command)++;
            modify_file_line(core->log_file, *last_command + 1, "  ");
        }
         
        return 0; 
    }
    return 1; 
}

