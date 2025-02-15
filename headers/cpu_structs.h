#ifndef CPU_STRUCTS_H
#define CPU_STRUCTS_H
#include "cache.h"  // Include cache functions and constants

/******************************************************************************
* Definitions
******************************************************************************/

#define NUM_REGS 16
#define SIZE_REG 32
#define MEMORY_SIZE (1 << 20) // 2^20 words
#define FETCH 0
#define DECODE 1
#define EXEC 2
#define MEM 3
#define WB 4 
#define STALL 1

#define INSTRUCTION_LENGTH 9
#define MAX_LINE_LENGTH 10

/******************************************************************************
* Structs
******************************************************************************/

typedef struct control_signals {
    int alu_src;
    int mem_to_reg;
    int reg_write;
    int mem_read;
    int mem_write;
    int branch;
    int jump;
    int halt; // Add halt signal for halt instruction
} ControlSignals;

typedef struct cmd {
    char inst[13];
    int opcode;
    int rd;
    int rs;
    int rt;
    int imm;  
    int btaken; 
    int jump_address;
    int state; 
    int hazard; 
    ControlSignals control_signals;
} Command;

typedef struct decode_buffers {
    int rs_value;
    int rt_value;
    int rd_value; // Only for R-type instructions
    int rs;
    int rt;
    int rd;
    int is_branch; 
} DecodeBuffers;

typedef struct {
    int alu_result;    // Store the result from the ALU operation
    int mem_address;    // Store the result from memory (if needed)
    int rd_value;      // Value for rd (used in R-type instructions)
    int destination;   // Destination register (rd or rt)
    int memory_or_not;
    int mem_busy;
    int branch_resolved; 
    int is_branch;
} ExecuteBuffer;

typedef struct {
    int load_result;   // Result from memory (after read)
    int destination_register;  // Address of the memory operation (calculated in execute)
} MemBuffer;

typedef struct {
    int finished;  
} WriteBackBuffer;

typedef struct Core {
    int core_id;
    int pc;                     // Program counter
    int IC;     
    int halted;
    int hazard; 
    char* log_file;
    char* fetch_buffer; 
    int read_miss_counter; 
    int read_hit_counter; 
    int write_miss_counter; 
    int write_hit_counter; 
    int decode_stall_counter ; 
    int mem_stall_counter ;
    int num_executed_instructions;
    int requesting; 
    char regout_array[NUM_REGS][9]; // Register file
    FILE* instruction_file; 
    FILE* regout_file;
    FILE* status_file;
    DecodeBuffers* decode_buf;   // Decode buffers
    ExecuteBuffer* execute_buf; // Execute buffer for each core
    MemBuffer mem_buf;  // Memory buffer for each core
    WriteBackBuffer* wb_buf;
    CACHE* cache; 
    MESI_bus* bus; 
    Command* current_instruction; // Current fetched instruction
    Command** instruction_array;     // Pointer to the instruction file
    Command** pipeline_array; 

} Core;



#endif // CPU_STRUCTS_H
