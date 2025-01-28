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


echo Compiling bus_arbitrator.c...

gcc -c bus_arbitrator.c -o obj/bus_arbitrator.o
if errorlevel 1 exit /b


echo Compiling main.c...

gcc -c main.c -o obj/main.o
if errorlevel 1 exit /b

echo linking...
REM Link the object files to create the executable inside the exe folder
gcc obj/main.o obj/bus_arbitrator.o obj/pipeline.o obj/state_machine.o obj/cache.o obj/initialize.o obj/asm.o obj/utils.o -o exe/sim.exe
if errorlevel 1 exit /b

echo running exe...

REM Run the executable (main_program.exe) from the exe folder
exe\sim.exe ^
log_files/imem0.txt ^
log_files/imem1.txt ^
log_files/imem2.txt ^
log_files/imem3.txt ^
log_files/memin.txt ^
log_files/memout.txt ^
log_files/regout0.txt ^
log_files/regout1.txt ^
log_files/regout2.txt ^
log_files/regout3.txt ^
log_files/core0trace.txt ^
log_files/core1trace.txt ^
log_files/core2trace.txt ^
log_files/core3trace.txt ^
log_files/bustrace.txt ^
log_files/dsram0.txt ^
log_files/dsram1.txt ^
log_files/dsram2.txt ^
log_files/dsram3.txt ^
log_files/tsram0.txt ^
log_files/tsram1.txt ^
log_files/tsram2.txt ^
log_files/tsram3.txt ^
log_files/stats0.txt ^
log_files/stats1.txt ^
log_files/stats2.txt ^
log_files/stats3.txt ^
asm_programs/program0.txt ^
asm_programs/program1.txt ^
asm_programs/program2.txt ^
asm_programs/program3.txt

if errorlevel 1 exit /b

REM Optional: Clean up object files after building the executable
REM del obj\*.o asm_programs/program.asm imems

echo Build and execution complete!
pause

