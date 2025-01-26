#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define COLOR_RESET "\033[0m"
#define BLUE "\033[1;34m"
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define MAGENTA "\033[1;35m"
#define CYAN "\033[1;36m"
#define WHITE "\033[1;37m"
#define BLACK "\033[1;30m"

// Bright Colors
#define BRIGHT_BLACK  "\033[1;30m"
#define BRIGHT_RED    "\033[1;31m"
#define BRIGHT_GREEN  "\033[1;32m"
#define BRIGHT_YELLOW "\033[1;33m"
#define BRIGHT_BLUE   "\033[1;34m"
#define BRIGHT_MAGENTA "\033[1;35m"
#define BRIGHT_CYAN   "\033[1;36m"
#define BRIGHT_WHITE  "\033[1;37m"

/******************************************************************************
*utilies 
*******************************************************************************/

/******************************************************************************
* Function: HexCharToInt
*
* Description: Hex Char -> Int  
*******************************************************************************/
int HexCharToInt(char h);


/******************************************************************************
* Function: Hex_2_Int_2s_Comp
*
* Description: Signed Hex -> Int 
*******************************************************************************/
int Hex_2_Int_2s_Comp(char * h);


/******************************************************************************
* Function: Int_2_Hex
*
* Description: signed integer -> hex char array of length 8
*******************************************************************************/

void Int_2_Hex(int dec_num, char hex_num[9]);
void modify_file_line(const char *filename, int line_number, const char *new_content) ;
int count_lines(FILE *file);


#endif