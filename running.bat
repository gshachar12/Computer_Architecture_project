@echo off
cls

REM Create the obj and exe directories if they don't exist
if not exist obj mkdir obj
if not exist exe mkdir exe

REM Compile source files into object files inside the obj folder
echo Compiling utils.c...

gcc -c utils.c -o obj/utils.o
if errorlevel 1 exit /b

echo Compiling pipline.c...

gcc -c pipeline.c -o obj/pipeline.o
if errorlevel 1 exit /b

echo Compiling state_machine.c...

gcc -c state_machine.c -o obj/state_machine.o
if errorlevel 1 exit /b

echo Compiling cache.c...

gcc -c cache.c -o obj/cache.o
if errorlevel 1 exit /b

echo Compiling asm.c...

gcc -c asm.c -o obj/asm.o
if errorlevel 1 exit /b

echo Compiling initialize.c...

gcc -c initialize.c -o obj/initialize.o
if errorlevel 1 exit /b

echo Compiling main.c...

gcc -c main.c -o obj/main.o
if errorlevel 1 exit /b

echo linking...
REM Link the object files to create the executable inside the exe folder
gcc obj/main.o obj/pipeline.o obj/state_machine.o obj/cache.o obj/initialize.o obj/asm.o obj/utils.o -o exe/main_program.exe
if errorlevel 1 exit /b

echo running exe...

REM Run the executable (main_program.exe) from the exe folder
exe\main_program.exe asm_programs/program.asm mem_files/imem.txt mem_files/dmem.txt
if errorlevel 1 exit /b

REM Optional: Clean up object files after building the executable
REM del obj\*.o asm_programs/program.asm imem

echo Build and execution complete!
pause

