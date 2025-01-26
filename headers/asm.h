#ifndef ASM_H
#define ASM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Constants
#define IMEM_SIZE 1024
#define DMEM_SIZE 1024


// Function Declarations
/**
 * Interpret assembly file and generate memory files.
 *
 * @param program The path to the assembly file.
 * @param imem_file The path to the output instruction memory file.
 * @param dmem_file The path to the output data memory file.
 * @return 0 on success, exits on error.
 */
int interpret_file(FILE* fp_asm, FILE* fp_imemout);

#endif // ASM_H
