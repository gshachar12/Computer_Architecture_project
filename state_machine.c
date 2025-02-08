#include "headers/utils.h"
#include "headers/cpu_structs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

//int main_memory[MEMORY_SIZE]; // Main memory array
// registers 
/* IO register array*/
int pc; 
int clk; 

//Function to print the register file to a file (all registers in the same row)
void print_regout_array_to_file( Core* core) {

    FILE *file = core->regout_file;  // Open in append mode to add to the file
    if (file == NULL)
    {
        perror("Error opening file for writing");
        return;
    }
    printf("\nwriting in file\n");
    // Iterate through the register file and print each register value in the same row
    for (int i = 0; i < NUM_REGS; i++) {
        printf("\nregister %d %s", i, core->regout_array[i]);
        fprintf(file, "%s ", core->regout_array[i]);  // Print each register value separated by space
    }

    fprintf(file, "\n");  // End the row with a newline

    fclose(file);  // Close the file after writing
}


/******************************************************************************
* Function: BuildCommand
*
* Description: extracts the data from the command line and places it in the command struct
*******************************************************************************/
void BuildCommand(char * command_line, Command * com)
{
    strcpy(com->inst, command_line);

	com->opcode = (int) strtol ((char[]) { command_line[0], command_line[1], 0 }, NULL, 16);
	com->rd     = (int) strtol ((char[]) { command_line[2], 0 }, NULL, 16);
	com->rs     = (int) strtol ((char[]) { command_line[3], 0 }, NULL, 16);
	com->rt     = (int) strtol ((char[]) { command_line[4], 0 }, NULL, 16);
	com->imm   = (int) strtol ((char[]) { command_line[5], command_line[6], command_line[7], 0 }, NULL, 16);

	if (com->imm >= 2048)
  	  {
      com->imm -= 4096;  // if the number is greater then 2048 it means that sign bit it on and hence we have to deduce 2^12 from the number
      }

}



int fetch_instruction(Core *core) {
    // Position the file pointer at the beginning (or desired location)
    // Debugging print: print the entire instruction
    printf("Fetched instruction: %s\n", core->instruction_array[core->pc]);
    core->pipeline_array[FETCH] = core->instruction_array[core->pc]; 
    // Increment program counter (pc)
    core->pc++;
    return 1; // Success
    
}


void detect_raw_hazard(Core *core) {
    if (strcmp(core->pipeline_array[DECODE]->inst, "DONE") == 0 || strcmp(core->pipeline_array[DECODE]->inst, "NOP") == 0)
        {    core->hazard = 0; 
    return; // No hazard detected
    }

    Command* decode = core->pipeline_array[DECODE]; // Decode stage instruction
    int decode_opcode = decode->opcode;
    printf("\nDecode buffer: rd %d  rs %d rt %d \n", decode->rd, decode->rs, decode->rt);

    // Iterate over pipeline stages from EXEC to WB
    for (int i = EXEC; i < WB; i++) {
        Command* com = core->pipeline_array[i]; // Other instructions in the pipeline

        if (com == NULL) {
            continue; // Skip null instructions
        }

        printf("\nPipeline stage %d: rd %d  rs %d rt %d \n", i, com->rd, com->rs, com->rt);

        // Check for hazards based on opcode
        switch (decode_opcode) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                if ((com->opcode >= 0 && com->opcode <=8) || com->opcode == 16 || com->opcode == 15)
                {
                    if(com->opcode == 15 && (decode->rs == 15 ||decode->rt == 15 || decode->rd == 15))
                    {
                    core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                    }
                    // General RAW hazard detection for R-type instructions
                    if ((decode->rs != 0 && decode->rs == com->rd) || 
                        (decode->rt != 0 && decode->rt == com->rd)) {
                                        core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                }
                }
                break;

            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
                // RAW hazard detection for branch instructions
                if(com->opcode == 15 && (decode->rs == 15 ||decode->rt == 15 || decode->rd == 15))
                    {
                    core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                    }
                if ((com->opcode >= 0 && com->opcode <= 8) || com->opcode == 16)
                {
                if ((decode->rs != 0 && decode->rs == com->rd) || 
                    (decode->rt != 0 && decode->rt == com->rd) ||
                    (decode->rd != 0 && decode->rd == com->rd)) {
                                        core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                }

                }
                break;

            case 15:
            if ((com->opcode >= 0 && com->opcode <= 8) || com->opcode == 16)
            {
                // RAW hazard detection for JAL (Jump and Link)
                if (decode->rd == com->rd) {
                    core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                }
            }
                break;
            case 16:
            // RAW hazard detection for LW (depends on rs)
                if ((com->opcode >= 0 && com->opcode <=8) || com->opcode == 16 || com->opcode == 15)
                {
                    if(com->opcode == 15 && (decode->rs == 15 ||decode->rt == 15 || decode->rd == 15))
                    {
                    core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                    }
                    // General RAW hazard detection for R-type instructions
                    if ((decode->rs != 0 && decode->rs == com->rd) || 
                        (decode->rt != 0 && decode->rt == com->rd)) {
                    core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                }
                }
                break;
            // Store Word
            case 17:
                if ((com->opcode >= 0 && com->opcode <=8) || com->opcode == 16 || com->opcode == 15)
                {
                    if(com->opcode == 15 && (decode->rs == 15 ||decode->rt == 15 || decode->rd == 15))
                    {
                    core->hazard = EXEC;  
                    return; // Hazard detected // Hazard detected
                    }
                    // General RAW hazard detection for Store instructions
                if ((decode->rs != 0 && decode->rs == com->rd) || 
                    (decode->rt != 0 && decode->rt == com->rd) ||
                    (decode->rd != 0 && decode->rd == com->rd)) {
                    core->hazard = EXEC;  
                    return; // Hazard detected
                }
                }
                
                break;        
            default:
                // No specific RAW hazard logic for other instructions
                break;
        }
    }
    core->hazard = 0; 
    return; // No hazard detected
}



void nullify_command(Command *src) {

    // Copy string
    
    strcpy(src->inst , "DONE");
    // Copy primitive fields
    src->opcode = -1;
    src->rd = -1;
    src->rs = -1;
    src->rt = -1;
    src->imm = -1;
    src->hazard = -1;
    src->jump_address = 0;
    src->btaken = 0;

    // Copy nested structure
    // dest->control_signals = rc->control_signals;
   
}

int decode(Core *core, Command *com) {
    // Build the Command struct from the command line
    BuildCommand(com->inst, com);
    Int_2_Hex(com->imm, core->regout_array[1]);
    printf("\nentered decode: %s\n", com->inst);
    //printf("opcode of com is: %d\n", com->opcode);

    core->decode_buf->rs = com->rs;
    core->decode_buf->rt = com->rt;
    core->decode_buf->rd = com->rd;
    printf("command decoded: opcode = %d, rd = %d, rs = %d, rt = %d, imm = %d\n", com->opcode, com->rd, com->rs, com->rt, com->imm);


    detect_raw_hazard(core);
    if (core->hazard)
        { 
            printf ("\n%s waiting for hazard to end%s\n ",RED, WHITE ); 
            return 0; 
        }
   // *(core->regout_array[1]) = com->imm;
    core->decode_buf->rs_value = Hex_2_Int_2s_Comp(core->regout_array[com->rs]);
    core->decode_buf->rt_value = Hex_2_Int_2s_Comp(core->regout_array[com->rt]);
    core->decode_buf->rd_value = Hex_2_Int_2s_Comp(core->regout_array[com->rd]);
    printf("values loaded in decode: rd_val = %d, rs_val = %d, rt_val = %d\n", core->decode_buf->rd_value, core->decode_buf->rs_value, core->decode_buf->rt_value);
    //printf("\nno hazard: command decoded: opcode = %d, rd = %d, rs = %d, rt = %d, imm = %d\n", core->decode_buf->rd_value, core->decode_buf->rs_value , core->decode_buf->rt_value );

    // Process the opcode and set control signals based on it
    switch (com->opcode) {
        case 0: // add
        case 1: // sub
        case 2: // and
        case 3: // or
        case 4: // xor
        case 5: // mul
            com->btaken = 0;
            break;

        case 6: // sll (shift left logical)
        case 7: // sra (shift right arithmetic)
        case 8: // srl (shift right logical)
            com->btaken = 0;
            break;

        case 9: // beq (branch if equal)
            if(core->decode_buf->rs_value == core->decode_buf->rt_value) com->btaken = 1;
            else com->btaken = 0;
            break;  
        case 10: // bne (branch if not equal)
            if(core->decode_buf->rs_value != core->decode_buf->rt_value) com->btaken = 1;
            else com->btaken = 0;
            break;
        case 11: // blt (branch if less than)
            if(core->decode_buf->rs_value < core->decode_buf->rt_value) com->btaken = 1;
            else com->btaken = 0;
            break;
        case 12: // bgt (branch if greater than)
            if(core->decode_buf->rs_value > core->decode_buf->rt_value) com->btaken = 1;
            else com->btaken = 0;
            break;

        case 13: // ble (branch if less or equal)
            if(core->decode_buf->rs_value <= core->decode_buf->rt_value) com->btaken = 1;
            else com->btaken = 0;
            break;

        case 14: // bge (branch if greater or equal)
            if(core->decode_buf->rs_value >= core->decode_buf->rt_value) com->btaken = 1;
            else com->btaken = 0;
            break;

        case 15: // jal (jump and link)
            com->btaken = 1;

            
            com->jump_address = core->pc-1;
            printf("jump entered decode, saving in jump address:%d", com->jump_address);
            break;

        case 16: // lw (load word)
            com->btaken = 0;
            break;

        case 17: // sw (store word)
            com->btaken = 0;
            break;

        case 20: // halt
            com->btaken = 0;
            core->halted = 1;
            break;

        default:
            printf("Core %d: Unrecognized opcode: %d\n", core->core_id, com->opcode);
            break;
    }
    printf("jump address in decode: %d, for opcode: %d\n", com->jump_address, com->opcode);
//     if(com->btaken == 1)
//         {
//         core->pc = (core->decode_buf->rd_value & 0x3FF);
//         printf("\n\nBranch taken. PC=%d\n\n", core->pc); 
//         }
//    printf("jump address in decode: %d\n", com->jump_address);
   return 0; //decode is finished
 }
void execute(Core *core, Command *com) {
    
    printf("------------------------jump address in exec: %d\n", com->jump_address);
    int alu_result = 0;
    int address = 0;
    int memory_or_not = 0; 
    int sa;
    int shift;
    printf("\nentered exec: %s\n", com->inst);
    switch (com->opcode) {
        case 0: // ADD (R-type)
            alu_result = core->decode_buf->rs_value + core->decode_buf->rt_value;

            memory_or_not = 0;
            break;

        case 1: // SUB (R-type)
            alu_result = core->decode_buf->rs_value - core->decode_buf->rt_value;
            memory_or_not = 0;
            break;

        case 2: // AND (R-type)
            alu_result = core->decode_buf->rs_value & core->decode_buf->rt_value;
            memory_or_not = 0;
            break;

        case 3: // OR (R-type)
            alu_result = core->decode_buf->rs_value | core->decode_buf->rt_value;
            memory_or_not = 0;
            break;

        case 4: // XOR (R-type)
            alu_result = core->decode_buf->rs_value ^ core->decode_buf->rt_value;
            memory_or_not = 0;
            break;

        case 5: // MUL (R-type)
            alu_result = core->decode_buf->rs_value * core->decode_buf->rt_value;
            memory_or_not = 0;
            break;

        case 6: // SLL (Shift Left Logical)
            alu_result = core->decode_buf->rs_value << core->decode_buf->rt_value;
            alu_result = alu_result;
            memory_or_not = 0;
            break;

        case 7: // SRA (Shift Right Arithmetic)
            sa = core->decode_buf->rs_value;
            shift = core->decode_buf->rt_value & 31;
            alu_result = sa >> shift;
            memory_or_not = 0;
            break;

        case 8: // SRL (Shift Right Logical)
            alu_result = (unsigned)core->decode_buf->rs_value >>core->decode_buf->rt_value;
            alu_result = alu_result;
            memory_or_not = 0;
            break;

        case 9: // BEQ (Branch if Equal)
            break;

        case 10: // BNE (Branch if Not Equal)
            break;

        case 11: // BLT (Branch if Less Than)
            break;

        case 12: // BGT (Branch if Greater Than)
            break;

        case 13: // BLE (Branch if Less or Equal)
            break;

        case 14: // BGE (Branch if Greater or Equal)
            break;

        case 15: // JAL (Jump and Link)
            //alu_result = core->pc;
            //core->pc = (core->decode_buf->rd_value & 0x3FF);  // Jump to target address
            //core->execute_buf->is_branch = 1;
            //memory_or_not = 0;
            break;

        case 16: // LW (Load Word)

            address = core->decode_buf->rs_value + core->decode_buf->rt_value;  // Base + offset
            alu_result = address;
            memory_or_not = 1;
            break;

        case 17: // SW (Store Word)
            address = core->decode_buf->rs_value + core->decode_buf->rt_value;  // Base + offset
            alu_result = address;
            memory_or_not = 1;
            break;

        case 20: // HALT
            com->control_signals.halt = 1;
            memory_or_not = 0;
            return;

        default:
            printf("Core %d: Unrecognized opcode: %d\n", core->core_id, com->opcode);
            break;
    }


    // Update the execute buffer
    core->execute_buf->alu_result = alu_result;
    core->execute_buf->destination = com->rd;
    core->execute_buf->mem_address = address;
    core->execute_buf->memory_or_not = memory_or_not;


    core->execute_buf->rd_value = core->decode_buf->rd_value;
    core->execute_buf->is_branch = core->execute_buf->is_branch || 0;  // Ensure default value is 0
    core->execute_buf->mem_busy = memory_or_not;  // Set mem_busy if a memory operation is in progress
    printf("EXECUTE BUFFER: %s alu_result%s: %d, destination: %d, mem_address: %d, memory_or_not: %d\n", YELLOW, WHITE,  core->execute_buf->alu_result, core->execute_buf->destination, core->execute_buf->mem_address, core->execute_buf->memory_or_not);
    

}

void memory_state(Command *com, Core *core, MESI_bus* mesi_bus) {
    uint32_t data = 0;
    // If memory access is required (i.e., for Load/Store operations)
    core->mem_buf.destination_register = core->execute_buf->destination;  // Store destination register for writing back
    printf("core->execute_buf->memory_or_not: %d, address: %d, data(if writing): %d\n", core->execute_buf->memory_or_not, core->execute_buf->mem_address, core->execute_buf->rd_value);
    if (core->execute_buf->memory_or_not == 1) {  // Memory operation indicator (load/store)

        if (com->opcode == 16) {  // Load Word (LW)
            // Cache read instead of direct memory read for the specific core

            bool hit = cache_read(core->cache, core->execute_buf->mem_address, &data, mesi_bus);  // Pass the logfile        
            if (hit) {
                core->read_hit_counter++; 
                core->mem_buf.load_result = data;  // Store loaded data in the buffer
                printf("Memory Read- Data is ready: Loaded value %d from address %d\n", core->mem_buf.load_result, core->execute_buf->mem_address);
                core->cache -> memory_stalls = 0; 
            } 

            else 
            {
                core->read_miss_counter++; 
                core->cache -> memory_stalls = 1; 
            }

        }

        if (com->opcode == 17) {  // Store Word (SW)
            // Cache write instead of direct memory write for the specific core
            
            bool hit = cache_write(core->cache, core->execute_buf->mem_address, core->execute_buf->rd_value,  mesi_bus);  // Pass the logfile

            if(hit)
            {
                core->write_hit_counter++; 
                core->cache -> memory_stalls = 0; 
            }

            else
            {
                core->write_miss_counter++; 
                core->cache -> memory_stalls = 1; 
            }
            printf("Memory Write (Cache): Stored value %d to address %d\n", core->execute_buf->rd_value, core->execute_buf->mem_address);
        }
    } else {

        // No memory operation, directly use the ALU result
        core->mem_buf.load_result = core->execute_buf->alu_result;
        core->mem_buf.destination_register = core->execute_buf->destination;
        printf("\ncore->mem_buf %d\n", core->mem_buf.load_result);
        printf("No memory operation needed.\n");
    }
}

void writeback_state(Command *com, Core *core) {
    uint32_t tag, index, block_offset;
    // Check if the instruction writes back to a register
    printf("WRITEBACK: rd = %d, value_to_store: %d, opcode: %d, jump_address: %d, btaken: %d", com->rd, core->mem_buf.load_result, com->opcode, com->jump_address, com->btaken);
    switch (com->opcode) {
        case 0: // ADD
        case 1: // SUB
        case 2: // AND
        case 3: // OR
        case 4: // XOR
        case 5: // MUL
        case 6: // SLL (Shift Left Logical)
        case 7: // SRA (Shift Right Arithmetic)
        case 8: // SRL (Shift Right Logical)
            // Write back the result from the memory buffer to the register file

            Int_2_Hex(core->mem_buf.load_result, core->regout_array[core->mem_buf.destination_register]);

            //printf("data written into register = %d", Hex_2_Int_2s_Comp(core->regout_array[core->mem_buf.destination_register]));
            break;
        
        case 15: // JAL (Jump and Link)
            // Store the return address (PC + 1) into the link register (usually $ra or $15)
            printf("\nwriting to register 15: %d\n", com->jump_address);
            Int_2_Hex(com->jump_address, core->regout_array[15]);
            break;

        case 16: // LW (Load Word)
            // Write the loaded data to the destination register
            get_cache_address_parts(core->execute_buf->mem_address, &tag, &index, &block_offset);
            CacheLine *dsram_line = &core->cache->dsram->cache[index];
            Int_2_Hex(dsram_line->data[block_offset], core->regout_array[core->mem_buf.destination_register]);
            break;

        case 17: // SW (Store Word) - No write-back to register
            break;

        case 20: // HALT - No write-back
            break;

        default:
            printf("opcode %d: no need for writeback.\n", com->opcode);
            break;
    }
    core->wb_buf->finished =1;
    core->cache -> memory_stalls = 0; 
}
