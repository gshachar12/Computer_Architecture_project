#define _CRT_SECURE_NO_WARNINGS  // (1) Disable MSVC warnings about fopen/sscanf

/****************************************************************************
 * Example 4-Core Simulator with pipeline debug instrumentation:
 *   - 5-stage pipeline (Fetch, Decode, Execute, Mem, WB)
 *   - Branch resolution in *Decode* (with optional flush of the Fetch stage)
 *   - R1 holds sign-extended immediate in Decode
 *   - 64-line direct-mapped data cache, 4 words/line => 256 words total
 *   - Multi-cycle bus transactions for cache misses (16 + 3 cycles)
 *   - Minimal MESI snooping (partial)
 *   - Round-robin bus arbitration
 *   - Writes to R0 ignored, R0=0 always
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

 /***************************************
  *            CONSTANTS
  ***************************************/
#define NUM_CORES            4
#define MAX_IMEM_SIZE        1024
#define MAIN_MEM_SIZE        (1 << 20)   // up to 2^20 words
#define REGFILE_SIZE         16

  // The assignment requires 256 words of data cache per core,
  // block size = 4 words => 64 lines in direct-mapped cache.
#define DCACHE_NUM_LINES     64
#define DCACHE_BLOCK_SIZE    4
#define DCACHE_NUM_WORDS     (DCACHE_NUM_LINES * DCACHE_BLOCK_SIZE)

// Address breakdown for a 20-bit address:
//  offset = 2 bits (4 words/line => 2^2=4)
//  index  = 6 bits (64 lines => 2^6=64)
//  tag    = 20 - 2 - 6 = 12 bits
static inline uint32_t getOffset(uint32_t addr) { return (addr & 0x3); }
static inline uint32_t getIndex(uint32_t addr) { return ((addr >> 2) & 0x3F); }
static inline uint32_t getTag(uint32_t addr) { return ((addr >> 8) & 0xFFF); }

// MESI states
#define MESI_INVALID   0
#define MESI_SHARED    1
#define MESI_EXCLUSIVE 2
#define MESI_MODIFIED  3

// Bus commands
#define BUS_NONE       0
#define BUS_RD         1  // BusRd
#define BUS_RDX        2  // BusRdX
#define BUS_FLUSH      3  // Flush

// Instruction opcodes
#define OPCODE_ADD     0
#define OPCODE_SUB     1
#define OPCODE_AND     2
#define OPCODE_OR      3
#define OPCODE_XOR     4
#define OPCODE_MUL     5
#define OPCODE_SLL     6
#define OPCODE_SRA     7
#define OPCODE_SRL     8
#define OPCODE_BEQ     9
#define OPCODE_BNE     10
#define OPCODE_BLT     11
#define OPCODE_BGT     12
#define OPCODE_BLE     13
#define OPCODE_BGE     14
#define OPCODE_JAL     15
#define OPCODE_LW      16
#define OPCODE_SW      17
#define OPCODE_HALT    20

/***************************************
 * INSTRUCTION FORMAT (31:0)
 *   31:24   23:20   19:16   15:12   11:0
 *   opcode | rd    | rs    | rt    | imm
 ***************************************/
typedef union {
    uint32_t raw;
    struct {
        unsigned int imm : 12;
        unsigned int rt : 4;
        unsigned int rs : 4;
        unsigned int rd : 4;
        unsigned int opcode : 8;
    } fields;
} Instruction;

/***************************************
 * DATA CACHE STRUCTURE
 ***************************************/
typedef struct {
    uint8_t  mesi;
    uint16_t tag;
    uint32_t block[DCACHE_BLOCK_SIZE];
} CacheLine;

typedef struct {
    CacheLine lines[DCACHE_NUM_LINES];
} Cache;

/***************************************
 *   PIPELINE REGISTERS
 ***************************************/
typedef struct {
    bool        valid;
    Instruction instr;
    uint32_t    pc;
    uint32_t    val_rs;
    uint32_t    val_rt;
    uint32_t    aluResult;
} PipeReg;

/***************************************
 *   CORE STRUCTURE
 ***************************************/
typedef struct {
    int         core_id;
    uint32_t    pc;
    uint32_t    regfile[REGFILE_SIZE];

    // Pipeline
    PipeReg     IF_ID;
    PipeReg     ID_EX;
    PipeReg     EX_MEM;
    PipeReg     MEM_WB;

    // Instruction memory
    Instruction imem[MAX_IMEM_SIZE];
    int         imem_size;

    // Data cache
    Cache       dcache;

    // Stall signals
    bool        decode_stall;
    bool        mem_stall;

    // For branch resolution in decode:
    bool        branchTaken;
    uint32_t    branchTarget;

    // Are we done?
    bool        halted;

    // Stats
    unsigned long cycleCount;
    unsigned long instrCount;
    unsigned long read_hit;
    unsigned long write_hit;
    unsigned long read_miss;
    unsigned long write_miss;
    unsigned long decode_stall_count;
    unsigned long mem_stall_count;
    unsigned long hazard_RAW_count;  // Only RAW now
    unsigned long hazard_WAR_count;  // We won't use these anymore, but can keep
    unsigned long hazard_WAW_count;  // for logging or ignore them

    // Optional: track if a bus txn is pending for this core
    bool        pendingBusTxn;

} Core;

/***************************************
 *   BUS TRANSACTION STRUCT
 ***************************************/
typedef struct {
    bool     active;
    int      cmd;
    int      originCore;
    uint32_t addr;
    uint32_t block[DCACHE_BLOCK_SIZE];
    bool     shared;
    int      cyclesLeft;
} BusTransaction;

/***************************************
 *   GLOBALS
 ***************************************/
static Core cores[NUM_CORES];
static uint32_t mainMem[MAIN_MEM_SIZE];

// For the bus
static BusTransaction busTxn;

// For round-robin bus arbitration
static bool     busRequest[NUM_CORES];
static int      busCmdReq[NUM_CORES];
static uint32_t busAddrReq[NUM_CORES];
static int      lastGrantedCore = 0;

// For tracing
static FILE* fcoreTrace[NUM_CORES] = { NULL, NULL, NULL, NULL };
static FILE* fbusTrace = NULL;

/***************************************
 *   FORWARD DECLARATIONS
 ***************************************/
void parseArguments(int argc, char* argv[],
    char** imemFile0, char** imemFile1, char** imemFile2, char** imemFile3,
    char** meminFile,
    char** memoutFile,
    char** regoutFile0, char** regoutFile1,
    char** regoutFile2, char** regoutFile3,
    char** coretraceFile0, char** coretraceFile1,
    char** coretraceFile2, char** coretraceFile3,
    char** bustraceFile,
    char** dsramFile0, char** dsramFile1,
    char** dsramFile2, char** dsramFile3,
    char** tsramFile0, char** tsramFile1,
    char** tsramFile2, char** tsramFile3,
    char** statsFile0, char** statsFile1,
    char** statsFile2, char** statsFile3);

void initSimulator(char* imemFiles[4], char* meminFile);
void loadImem(Core* c, const char* filename);
void loadMainMem(const char* filename);

void openTraceFiles(char* coretrace[4], char* bustrace);
void closeTraceFiles();

void runSimulation();
bool allCoresHalted();
bool pipelineDrained(Core* c);

// Pipeline stages
void fetchStage(Core* c);
void decodeStage(Core* c);
void executeStage(Core* c);
void memStage(Core* c);
void wbStage(Core* c);

// ALU
uint32_t doALU(Core* c, Instruction in, uint32_t a, uint32_t b);

// Hazards & stalls
bool checkDataHazard(Core* c);
void insertDecodeStall(Core* c);

// Cache read/write
uint32_t readFromCache(Core* c, uint32_t addr, bool* hit);
void writeToCache(Core* c, uint32_t addr, uint32_t val, bool* hit, bool writeAllocate);

// Bus
void requestBusTransaction(int cmd, int origin, uint32_t addr);
static void arbitrateBusRequests();
void busCycle();
void busSnoopAllCores(int origin, int cmd, uint32_t addr);
void finishBusTransaction();

// Traces
void traceCoreCycle(Core* c, unsigned long cycle);
void traceBusCycle(unsigned long cycle, BusTransaction* t);

// Dump
void dumpResults(char* memoutFile, char* regoutFiles[4],
    char* dsramFiles[4], char* tsramFiles[4],
    char* statsFiles[4]);

/***************************************
 *             MAIN
 ***************************************/
int main(int argc, char* argv[])
{
    if (argc < 28) {
        fprintf(stderr,
            "Usage: %s imem0 imem1 imem2 imem3 memin memout "
            "regout0 regout1 regout2 regout3 "
            "core0trace core1trace core2trace core3trace "
            "bustrace "
            "dsram0 dsram1 dsram2 dsram3 "
            "tsram0 tsram1 tsram2 tsram3 "
            "stats0 stats1 stats2 stats3\n",
            argv[0]);
        return 1;
    }

    // Map arguments
    char* imemFile0, * imemFile1, * imemFile2, * imemFile3;
    char* meminFile, * memoutFile;
    char* regoutFile0, * regoutFile1, * regoutFile2, * regoutFile3;
    char* coretraceFile0, * coretraceFile1, * coretraceFile2, * coretraceFile3;
    char* bustraceFile;
    char* dsramFile0, * dsramFile1, * dsramFile2, * dsramFile3;
    char* tsramFile0, * tsramFile1, * tsramFile2, * tsramFile3;
    char* statsFile0, * statsFile1, * statsFile2, * statsFile3;

    parseArguments(argc, argv,
        &imemFile0, &imemFile1, &imemFile2, &imemFile3,
        &meminFile,
        &memoutFile,
        &regoutFile0, &regoutFile1, &regoutFile2, &regoutFile3,
        &coretraceFile0, &coretraceFile1, &coretraceFile2, &coretraceFile3,
        &bustraceFile,
        &dsramFile0, &dsramFile1, &dsramFile2, &dsramFile3,
        &tsramFile0, &tsramFile1, &tsramFile2, &tsramFile3,
        &statsFile0, &statsFile1, &statsFile2, &statsFile3
    );

    // Initialize
    char* imemFiles[4] = { imemFile0, imemFile1, imemFile2, imemFile3 };
    initSimulator(imemFiles, meminFile);

    // Open trace files
    char* coretrace[4] = { coretraceFile0, coretraceFile1, coretraceFile2, coretraceFile3 };
    openTraceFiles(coretrace, bustraceFile);

    // Run
    runSimulation();

    // Dump
    char* regoutFiles[4] = { regoutFile0, regoutFile1, regoutFile2, regoutFile3 };
    char* dsramFiles[4] = { dsramFile0, dsramFile1, dsramFile2, dsramFile3 };
    char* tsramFiles[4] = { tsramFile0, tsramFile1, tsramFile2, tsramFile3 };
    char* statsFiles[4] = { statsFile0, statsFile1, statsFile2, statsFile3 };

    dumpResults(memoutFile, regoutFiles, dsramFiles, tsramFiles, statsFiles);

    // Close trace files
    closeTraceFiles();

    return 0;
}

/***************************************
 *    parseArguments
 ***************************************/
void parseArguments(int argc, char* argv[],
    char** imemFile0, char** imemFile1, char** imemFile2, char** imemFile3,
    char** meminFile,
    char** memoutFile,
    char** regoutFile0, char** regoutFile1,
    char** regoutFile2, char** regoutFile3,
    char** coretraceFile0, char** coretraceFile1,
    char** coretraceFile2, char** coretraceFile3,
    char** bustraceFile,
    char** dsramFile0, char** dsramFile1,
    char** dsramFile2, char** dsramFile3,
    char** tsramFile0, char** tsramFile1,
    char** tsramFile2, char** tsramFile3,
    char** statsFile0, char** statsFile1,
    char** statsFile2, char** statsFile3)
{
    *imemFile0 = argv[1];
    *imemFile1 = argv[2];
    *imemFile2 = argv[3];
    *imemFile3 = argv[4];
    *meminFile = argv[5];
    *memoutFile = argv[6];
    *regoutFile0 = argv[7];
    *regoutFile1 = argv[8];
    *regoutFile2 = argv[9];
    *regoutFile3 = argv[10];
    *coretraceFile0 = argv[11];
    *coretraceFile1 = argv[12];
    *coretraceFile2 = argv[13];
    *coretraceFile3 = argv[14];
    *bustraceFile = argv[15];
    *dsramFile0 = argv[16];
    *dsramFile1 = argv[17];
    *dsramFile2 = argv[18];
    *dsramFile3 = argv[19];
    *tsramFile0 = argv[20];
    *tsramFile1 = argv[21];
    *tsramFile2 = argv[22];
    *tsramFile3 = argv[23];
    *statsFile0 = argv[24];
    *statsFile1 = argv[25];
    *statsFile2 = argv[26];
    *statsFile3 = argv[27];
}

/***************************************
 *      initSimulator
 ***************************************/
void initSimulator(char* imemFiles[4], char* meminFile)
{
    // Zero mainMem
    for (int i = 0; i < MAIN_MEM_SIZE; i++) {
        mainMem[i] = 0;
    }
    loadMainMem(meminFile);

    // Bus initialization
    busTxn.active = false;
    busTxn.cmd = BUS_NONE;
    busTxn.originCore = -1;
    busTxn.addr = 0;
    busTxn.shared = false;
    busTxn.cyclesLeft = 0;
    for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
        busTxn.block[w] = 0;
    }

    // Round-robin initialization
    for (int c = 0; c < NUM_CORES; c++) {
        busRequest[c] = false;
        busCmdReq[c] = BUS_NONE;
        busAddrReq[c] = 0;
    }
    lastGrantedCore = 0;

    // Initialize each core
    for (int c = 0; c < NUM_CORES; c++) {
        cores[c].core_id = c;
        cores[c].pc = 0;
        loadImem(&cores[c], imemFiles[c]);

        // Zero the regfile
        for (int r = 0; r < REGFILE_SIZE; r++) {
            cores[c].regfile[r] = 0;
        }

        // pipeline regs
        cores[c].IF_ID.valid = false;
        cores[c].ID_EX.valid = false;
        cores[c].EX_MEM.valid = false;
        cores[c].MEM_WB.valid = false;

        cores[c].halted = false;
        cores[c].decode_stall = false;
        cores[c].mem_stall = false;
        cores[c].branchTaken = false;
        cores[c].branchTarget = 0;
        cores[c].pendingBusTxn = false;

        // Stats
        cores[c].cycleCount = 0;
        cores[c].instrCount = 0;
        cores[c].read_hit = 0;
        cores[c].write_hit = 0;
        cores[c].read_miss = 0;
        cores[c].write_miss = 0;
        cores[c].decode_stall_count = 0;
        cores[c].mem_stall_count = 0;
        cores[c].hazard_RAW_count = 0;
        cores[c].hazard_WAR_count = 0; // not used, but left for completeness
        cores[c].hazard_WAW_count = 0; // not used, but left for completeness

        // Cache init
        for (int i = 0; i < DCACHE_NUM_LINES; i++) {
            cores[c].dcache.lines[i].mesi = MESI_INVALID;
            cores[c].dcache.lines[i].tag = 0;
            for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
                cores[c].dcache.lines[i].block[w] = 0;
            }
        }
    }
}

/***************************************
 *      loadImem
 ***************************************/
void loadImem(Core* c, const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open IMEM file: %s\n", filename);
        exit(1);
    }
    char line[128];
    int index = 0;
    while (fgets(line, sizeof(line), f)) {
        uint32_t val;
        if (sscanf(line, "%x", &val) == 1) {
            c->imem[index].raw = val;
            index++;
            if (index >= MAX_IMEM_SIZE) break;
        }
    }
    c->imem_size = index;
    fclose(f);
}

/***************************************
 *      loadMainMem
 ***************************************/
void loadMainMem(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open memin file: %s\n", filename);
        exit(1);
    }
    char line[128];
    int index = 0;
    while (fgets(line, sizeof(line), f)) {
        uint32_t val;
        if (sscanf(line, "%x", &val) == 1) {
            if (index < MAIN_MEM_SIZE) {
                mainMem[index] = val;
                index++;
            }
        }
    }
    fclose(f);
}

/***************************************
 *      openTraceFiles / closeTraceFiles
 ***************************************/
void openTraceFiles(char* coretrace[4], char* bustrace)
{
    for (int c = 0; c < NUM_CORES; c++) {
        fcoreTrace[c] = fopen(coretrace[c], "w");
        if (!fcoreTrace[c]) {
            fprintf(stderr, "Cannot open core trace file: %s\n", coretrace[c]);
            exit(1);
        }
    }
    fbusTrace = fopen(bustrace, "w");
    if (!fbusTrace) {
        fprintf(stderr, "Cannot open bus trace file: %s\n", bustrace);
        exit(1);
    }
}

void closeTraceFiles()
{
    for (int c = 0; c < NUM_CORES; c++) {
        if (fcoreTrace[c]) {
            fclose(fcoreTrace[c]);
            fcoreTrace[c] = NULL;
        }
    }
    if (fbusTrace) {
        fclose(fbusTrace);
        fbusTrace = NULL;
    }
}

/***************************************
 *      runSimulation
 ***************************************/
void runSimulation()
{
    unsigned long globalCycle = 0;
    const unsigned long MAX_CYCLES = 2000000UL;

    while (!allCoresHalted() && globalCycle < MAX_CYCLES) {
        // 1) WB
        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                wbStage(&cores[c]);
            }
        }

        // 2) MEM
        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                memStage(&cores[c]);
            }
        }

        // Round-robin bus arbitration
        arbitrateBusRequests();

        // Bus cycle
        busCycle();

        // 3) EX
        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                executeStage(&cores[c]);
            }
        }

        // 4) ID
        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                decodeStage(&cores[c]);
            }
        }

        // 5) IF
        for (int c = 0; c < NUM_CORES; c++) {
            if (!cores[c].halted) {
                fetchStage(&cores[c]);
            }
        }

        // Debug trace: bus
        if (busTxn.active) {
            traceBusCycle(globalCycle, &busTxn);
        }

        // Debug trace: cores
        for (int c = 0; c < NUM_CORES; c++) {
            traceCoreCycle(&cores[c], globalCycle);
        }

        // inc cycleCount
        for (int c = 0; c < NUM_CORES; c++) {
            cores[c].cycleCount++;
        }

        globalCycle++;
    }
}

/***************************************
 *      allCoresHalted
 ***************************************/
bool allCoresHalted()
{
    for (int c = 0; c < NUM_CORES; c++) {
        if (!cores[c].halted || !pipelineDrained(&cores[c])) {
            return false;
        }
    }
    return true;
}

/***************************************
 *      pipelineDrained
 ***************************************/
bool pipelineDrained(Core* c)
{
    if (!c->halted)           return false;
    if (c->IF_ID.valid)       return false;
    if (c->ID_EX.valid)       return false;
    if (c->EX_MEM.valid)      return false;
    if (c->MEM_WB.valid)      return false;
    return true;
}

/***************************************
 *      FETCH
 ***************************************/
void fetchStage(Core* c)
{
    if (c->decode_stall) {
        // Skip fetching if decode_stall is set
        printf("[CORE %d] fetchStage: decode_stall=TRUE, skipping fetch\n", c->core_id);
        return;
    }
    if (!c->IF_ID.valid) {
        printf("[CORE %d] fetchStage: about to fetch at PC=0x%X\n", c->core_id, c->pc);

        if (c->pc < (uint32_t)c->imem_size) {
            c->IF_ID.instr = c->imem[c->pc];
            printf("[CORE %d] fetchStage: Fetched opcode=%d, rd=%d, rs=%d, rt=%d, imm=0x%X\n",
                c->core_id,
                c->IF_ID.instr.fields.opcode,
                c->IF_ID.instr.fields.rd,
                c->IF_ID.instr.fields.rs,
                c->IF_ID.instr.fields.rt,
                c->IF_ID.instr.fields.imm);
        }
        else {
            // NOP if out of range
            Instruction nop;
            nop.raw = 0;
            c->IF_ID.instr = nop;
        }
        c->IF_ID.pc = c->pc;
        c->IF_ID.valid = true;
        c->pc++;
    }
}

/***************************************
 *      DECODE
 ***************************************/
 // Only RAW hazard now
bool checkDataHazard(Core* c)
{
    if (!c->IF_ID.valid) return false;

    Instruction idInstr = c->IF_ID.instr;
    // Identify the source registers
    int src1 = idInstr.fields.rs;
    int src2 = idInstr.fields.rt;

    // If the opcode is branch or JAL, treat rd as an additional source
    switch (idInstr.fields.opcode) {
    case OPCODE_BEQ:
    case OPCODE_BNE:
    case OPCODE_BLT:
    case OPCODE_BGT:
    case OPCODE_BLE:
    case OPCODE_BGE:
    case OPCODE_JAL:
        src2 = idInstr.fields.rd;
        break;
    default:
        break;
    }

    // For RAW, we only need to see if EX or MEM is writing a register
    // that the ID stage needs as a read.
    // Let's gather EX and MEM stage destinations.

    bool ex_valid = c->ID_EX.valid;
    bool mem_valid = c->EX_MEM.valid;

    int ex_dest = -1;
    int mem_dest = -1;

    if (ex_valid) {
        Instruction exInstr = c->ID_EX.instr;
        switch (exInstr.fields.opcode) {
        case OPCODE_ADD: case OPCODE_SUB: case OPCODE_AND: case OPCODE_OR:
        case OPCODE_XOR: case OPCODE_MUL: case OPCODE_SLL: case OPCODE_SRA:
        case OPCODE_SRL: case OPCODE_LW: case OPCODE_JAL:
            ex_dest = exInstr.fields.rd;
            break;
        default:
            ex_dest = -1;
            break;
        }
    }

    if (mem_valid) {
        Instruction memInstr = c->EX_MEM.instr;
        switch (memInstr.fields.opcode) {
        case OPCODE_ADD: case OPCODE_SUB: case OPCODE_AND: case OPCODE_OR:
        case OPCODE_XOR: case OPCODE_MUL: case OPCODE_SLL: case OPCODE_SRA:
        case OPCODE_SRL: case OPCODE_LW: case OPCODE_JAL:
            mem_dest = memInstr.fields.rd;
            break;
        default:
            mem_dest = -1;
            break;
        }
    }

    bool hazardRAW = false;
    // RAW check: if src1 or src2 equals ex_dest or mem_dest
    // (ignoring zero register)
    if (ex_valid && ex_dest > 0) {
        if (src1 == ex_dest || src2 == ex_dest) {
            hazardRAW = true;
        }
    }
    if (mem_valid && mem_dest > 0) {
        if (src1 == mem_dest || src2 == mem_dest) {
            hazardRAW = true;
        }
    }

    if (hazardRAW) {
        c->hazard_RAW_count++;
        return true;
    }
    return false;
}

void insertDecodeStall(Core* c)
{
    c->decode_stall = true;
    c->decode_stall_count++;
    printf("[CORE %d] decodeStage: Data hazard => decode_stall=TRUE\n", c->core_id);
}

void decodeStage(Core* c)
{
    // If we were stalled, skip decode once, then clear stall
    if (c->decode_stall) {
        printf("[CORE %d] decodeStage: Was decode_stall=TRUE, clearing it & skipping decode.\n", c->core_id);
        c->decode_stall = false;
        return;
    }

    // Move instruction from IF to ID if ID_EX is free
    if (!c->ID_EX.valid && c->IF_ID.valid) {
        printf("[CORE %d] decodeStage: Decoding PC=0x%X\n", c->core_id, c->IF_ID.pc);

        // Check hazard (RAW only)
        if (checkDataHazard(c)) {
            insertDecodeStall(c);
            return;
        }

        // Move forward in the pipeline
        c->ID_EX = c->IF_ID;
        c->IF_ID.valid = false;

        Instruction in = c->ID_EX.instr;
        int rs = in.fields.rs;
        int rt = in.fields.rt;
        // read register file
        uint32_t val_rs = (rs == 0 ? 0 : c->regfile[rs]);
        uint32_t val_rt = (rt == 0 ? 0 : c->regfile[rt]);

        // sign-extend imm -> R1
        int16_t sx = (int16_t)(in.fields.imm & 0xFFF);
        if (sx & 0x0800) {
            sx |= 0xF000;
        }
        c->regfile[1] = (uint32_t)sx;

        c->ID_EX.val_rs = val_rs;
        c->ID_EX.val_rt = val_rt;

        printf("[CORE %d] decodeStage: opcode=%d, rd=%d, rs=%d, rt=%d\n",
            c->core_id, in.fields.opcode, in.fields.rd, rs, rt);
        printf("[CORE %d] decodeStage: val_rs=0x%X, val_rt=0x%X, R1=0x%X\n",
            c->core_id, val_rs, val_rt, c->regfile[1]);

        // Branch resolution in decode
        c->branchTaken = false;
        switch (in.fields.opcode) {
        case OPCODE_BEQ:
            if (val_rs == val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            }
            break;
        case OPCODE_BNE:
            if (val_rs != val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            }
            break;
        case OPCODE_BLT:
            if ((int32_t)val_rs < (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            }
            break;
        case OPCODE_BGT:
            if ((int32_t)val_rs > (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            }
            break;
        case OPCODE_BLE:
            if ((int32_t)val_rs <= (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            }
            break;
        case OPCODE_BGE:
            if ((int32_t)val_rs >= (int32_t)val_rt) {
                c->branchTaken = true;
                c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            }
            break;
        case OPCODE_JAL:
            c->branchTaken = true;
            c->branchTarget = c->regfile[in.fields.rd] & 0x3FF;
            break;
        default:
            break;
        }

        if (c->branchTaken) {
            printf("core %d: branching to 0x%X from decode (PC=%u)\n",
                c->core_id, c->branchTarget, c->ID_EX.pc);
            c->pc = c->branchTarget;
            // Flush the IF stage
            c->IF_ID.valid = false;
        }
    }
}

/***************************************
 *      EXECUTE
 ***************************************/
void executeStage(Core* c)
{
    if (!c->EX_MEM.valid && c->ID_EX.valid) {
        c->EX_MEM = c->ID_EX;
        c->ID_EX.valid = false;

        Instruction in = c->EX_MEM.instr;
        uint32_t a = c->EX_MEM.val_rs;
        uint32_t b = c->EX_MEM.val_rt;
        uint32_t aluRes = doALU(c, in, a, b);

        c->EX_MEM.aluResult = aluRes;

        // Debug
        printf("[CORE %d] executeStage: opcode=%d (PC=0x%X)\n",
            c->core_id, in.fields.opcode, c->EX_MEM.pc);
    }
}

uint32_t doALU(Core* c, Instruction in, uint32_t a, uint32_t b)
{
    switch (in.fields.opcode) {
    case OPCODE_ADD: return a + b;
    case OPCODE_SUB: return a - b;
    case OPCODE_AND: return a & b;
    case OPCODE_OR:  return a | b;
    case OPCODE_XOR: return a ^ b;
    case OPCODE_MUL: return a * b;
    case OPCODE_SLL: return a << (b & 31);
    case OPCODE_SRA: {
        int32_t sa = (int32_t)a;
        int shift = (b & 31);
        return (uint32_t)(sa >> shift);
    }
    case OPCODE_SRL: return a >> (b & 31);
    case OPCODE_LW:
    case OPCODE_SW:
        return a + b;
    default:
        return 0;
    }
}

/***************************************
 *      MEM
 ***************************************/
uint32_t readFromCache(Core* c, uint32_t addr, bool* hit)
{
    uint32_t idx = getIndex(addr);
    uint32_t tag = getTag(addr);
    uint32_t off = getOffset(addr);

    CacheLine* line = &c->dcache.lines[idx];
    if (line->mesi != MESI_INVALID && line->tag == tag) {
        *hit = true;
        c->read_hit++;
        return line->block[off];
    }
    else {
        *hit = false;
        c->read_miss++;
        requestBusTransaction(BUS_RD, c->core_id, addr);
        return 0;
    }
}

void writeToCache(Core* c, uint32_t addr, uint32_t val, bool* hit, bool writeAllocate)
{
    uint32_t idx = getIndex(addr);
    uint32_t tag = getTag(addr);
    uint32_t off = getOffset(addr);

    CacheLine* line = &c->dcache.lines[idx];
    if (line->mesi != MESI_INVALID && line->tag == tag) {
        *hit = true;
        c->write_hit++;
        // If it was shared, we must get RDX first
        if (line->mesi == MESI_SHARED) {
            requestBusTransaction(BUS_RDX, c->core_id, addr);
            line->mesi = MESI_MODIFIED;
        }
        else if (line->mesi == MESI_EXCLUSIVE) {
            line->mesi = MESI_MODIFIED;
        }
        line->block[off] = val;
    }
    else {
        *hit = false;
        c->write_miss++;
        if (writeAllocate) {
            requestBusTransaction(BUS_RDX, c->core_id, addr);
        }
        else {
            // write-no-allocate => directly to mainMem
            if (addr < MAIN_MEM_SIZE) {
                mainMem[addr] = val;
            }
        }
    }
}

void memStage(Core* c)
{
    if (!c->MEM_WB.valid && c->EX_MEM.valid) {
        if (c->mem_stall) {
            c->mem_stall_count++;
            printf("[CORE %d] memStage: mem_stall=TRUE, skipping MEM. stall_count=%lu\n",
                c->core_id, c->mem_stall_count);
            return;
        }
        c->MEM_WB = c->EX_MEM;
        c->EX_MEM.valid = false;

        Instruction in = c->MEM_WB.instr;
        uint32_t addr = c->MEM_WB.aluResult;

        switch (in.fields.opcode) {
        case OPCODE_LW: {
            bool hit = false;
            uint32_t data = readFromCache(c, addr, &hit);
            if (!hit) {
                c->mem_stall = true;
                printf("[CORE %d] memStage: LW => hit=%d, data=0x%X\n", c->core_id, hit, data);
                printf("[CORE %d] memStage: mem_stall=TRUE, waiting for bus...\n", c->core_id);
            }
            else {
                c->MEM_WB.aluResult = data;
            }
            break;
        }
        case OPCODE_SW: {
            bool hit = false;
            int rd = in.fields.rd;
            uint32_t val = c->regfile[rd];
            writeToCache(c, addr, val, &hit, true);
            if (!hit) {
                c->mem_stall = true;
                printf("[CORE %d] memStage: SW => miss => mem_stall=TRUE\n", c->core_id);
            }
            break;
        }
        case OPCODE_HALT:
            printf("[CORE %d] memStage: HALT found, will finalize in WB.\n", c->core_id);
            break;
        default:
            break;
        }
    }
}

/***************************************
 *      WB
 ***************************************/
void wbStage(Core* c)
{
    if (c->MEM_WB.valid) {
        Instruction in = c->MEM_WB.instr;
        int rd = in.fields.rd;

        switch (in.fields.opcode) {
        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        case OPCODE_MUL:
        case OPCODE_SLL:
        case OPCODE_SRA:
        case OPCODE_SRL:
        case OPCODE_LW:
            if (rd != 0 && rd != 1) {
                c->regfile[rd] = c->MEM_WB.aluResult;
            }
            c->instrCount++;
            break;
        case OPCODE_SW:
        case OPCODE_BEQ:
        case OPCODE_BNE:
        case OPCODE_BLT:
        case OPCODE_BGT:
        case OPCODE_BLE:
        case OPCODE_BGE:
            c->instrCount++;
            break;
        case OPCODE_JAL:
            c->regfile[15] = c->MEM_WB.pc + 1;  // link in R15
            c->instrCount++;
            break;
        case OPCODE_HALT:
            c->instrCount++;
            c->halted = true;
            break;
        default:
            // e.g., a NOP or unused opcode
            break;
        }
        c->MEM_WB.valid = false;
    }
}

/***************************************
 *   requestBusTransaction & Arbitrate
 ***************************************/
void requestBusTransaction(int cmd, int origin, uint32_t addr)
{
    busRequest[origin] = true;
    busCmdReq[origin] = cmd;
    busAddrReq[origin] = addr;
    // Debug line optional:
    // printf("[CORE %d] requestBusTransaction: cmd=%d addr=0x%X\n", origin, cmd, addr);
}

static void arbitrateBusRequests()
{
    if (busTxn.active) return;

    for (int i = 0; i < NUM_CORES; i++) {
        int candidate = (lastGrantedCore + 1 + i) % NUM_CORES;
        if (busRequest[candidate]) {
            busRequest[candidate] = false;
            busTxn.active = true;
            busTxn.cmd = busCmdReq[candidate];
            busTxn.addr = busAddrReq[candidate];
            busTxn.originCore = candidate;
            busTxn.shared = false;
            busTxn.cyclesLeft = 19; // 16 + 3
            for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
                busTxn.block[w] = 0;
            }
            lastGrantedCore = candidate;
            // printf("[BUS] arbitrateBusRequests: granted to core=%d cmd=%d addr=0x%X\n", 
            //        candidate, busTxn.cmd, busTxn.addr);
            break;
        }
    }
}

/***************************************
 *      busCycle
 ***************************************/
void busCycle()
{
    if (!busTxn.active) return;

    busTxn.cyclesLeft--;
    if (busTxn.cyclesLeft <= 0) {
        busSnoopAllCores(busTxn.originCore, busTxn.cmd, busTxn.addr);
        // read block from mainMem
        uint32_t baseAddr = (busTxn.addr >> 2) << 2;
        baseAddr &= 0xFFFFC;
        for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
            uint32_t a = baseAddr + i;
            if (a < MAIN_MEM_SIZE) {
                busTxn.block[i] = mainMem[a];
            }
        }
        finishBusTransaction();
    }
}

/***************************************
 *      busSnoopAllCores
 ***************************************/
void busSnoopAllCores(int origin, int cmd, uint32_t addr)
{
    for (int c = 0; c < NUM_CORES; c++) {
        if (c == origin) continue;
        uint32_t idx = getIndex(addr);
        uint32_t tag = getTag(addr);
        CacheLine* line = &cores[c].dcache.lines[idx];
        if (line->mesi != MESI_INVALID && line->tag == tag) {
            busTxn.shared = true;
            if (cmd == BUS_RD) {
                if (line->mesi == MESI_MODIFIED) {
                    // flush
                    uint32_t baseAddr = (addr >> 2) << 2;
                    for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
                        uint32_t a = baseAddr + i;
                        if (a < MAIN_MEM_SIZE) {
                            mainMem[a] = line->block[i];
                        }
                    }
                    line->mesi = MESI_SHARED;
                }
                else if (line->mesi == MESI_EXCLUSIVE) {
                    line->mesi = MESI_SHARED;
                }
            }
            else if (cmd == BUS_RDX) {
                if (line->mesi == MESI_MODIFIED) {
                    uint32_t baseAddr = (addr >> 2) << 2;
                    for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
                        uint32_t a = baseAddr + i;
                        if (a < MAIN_MEM_SIZE) {
                            mainMem[a] = line->block[i];
                        }
                    }
                }
                line->mesi = MESI_INVALID;
            }
        }
    }
}

/***************************************
 *      finishBusTransaction
 ***************************************/
void finishBusTransaction()
{
    int origin = busTxn.originCore;
    int cmd = busTxn.cmd;
    uint32_t addr = busTxn.addr;

    Core* c = &cores[origin];
    uint32_t idx = getIndex(addr);
    uint32_t tag = getTag(addr);
    CacheLine* line = &c->dcache.lines[idx];

    if (cmd == BUS_RD || cmd == BUS_RDX) {
        line->tag = tag;
        if (cmd == BUS_RD) {
            line->mesi = (busTxn.shared ? MESI_SHARED : MESI_EXCLUSIVE);
        }
        else { // BUS_RDX
            line->mesi = MESI_MODIFIED;
        }
        for (int i = 0; i < DCACHE_BLOCK_SIZE; i++) {
            line->block[i] = busTxn.block[i];
        }
        c->mem_stall = false;
        printf("[CORE %d] finishBusTransaction: Un-stalling mem_stall.\n", origin);
    }

    // Clear the bus
    busTxn.active = false;
    busTxn.cmd = BUS_NONE;
    busTxn.addr = 0;
    busTxn.originCore = -1;
    busTxn.shared = false;
    busTxn.cyclesLeft = 0;
}

/***************************************
 *      traceCoreCycle
 ***************************************/
void traceCoreCycle(Core* c, unsigned long cycle)
{
    char fetch_str[16] = "---";
    char decode_str[16] = "---";
    char exec_str[16] = "---";
    char mem_str[16] = "---";
    char wb_str[16] = "---";

    if (c->IF_ID.valid)   snprintf(fetch_str, 16, "%03X", c->IF_ID.pc);
    if (c->ID_EX.valid)   snprintf(decode_str, 16, "%03X", c->ID_EX.pc);
    if (c->EX_MEM.valid)  snprintf(exec_str, 16, "%03X", c->EX_MEM.pc);
    if (c->MEM_WB.valid)  snprintf(mem_str, 16, "%03X", c->MEM_WB.pc);

    // There's no separate pipeline register for WB alone; we sometimes
    // display it as '---' or if you want you can track it differently.

    fprintf(fcoreTrace[c->core_id],
        "%lu %s %s %s %s %s",
        cycle, fetch_str, decode_str, exec_str, mem_str, wb_str);

    // Print registers R2..R15
    for (int r = 2; r < REGFILE_SIZE; r++) {
        fprintf(fcoreTrace[c->core_id], " %08X", c->regfile[r]);
    }
    fprintf(fcoreTrace[c->core_id], "\n");
}

/***************************************
 *      traceBusCycle
 ***************************************/
void traceBusCycle(unsigned long cycle, BusTransaction* t)
{
    fprintf(fbusTrace,
        "%lu %d %d %05X %08X %d\n",
        cycle,
        t->originCore,
        t->cmd,
        (t->addr & 0xFFFFF),
        0U,
        t->shared ? 1 : 0
    );
}

/***************************************
 *      dumpResults
 ***************************************/
void dumpResults(char* memoutFile,
    char* regoutFiles[4],
    char* dsramFiles[4],
    char* tsramFiles[4],
    char* statsFiles[4])
{
    // 1) memout
    FILE* fmemout = fopen(memoutFile, "w");
    if (!fmemout) {
        fprintf(stderr, "Cannot open memout file %s\n", memoutFile);
        exit(1);
    }
    for (int i = 0; i < MAIN_MEM_SIZE; i++) {
        fprintf(fmemout, "%08X\n", mainMem[i]);
    }
    fclose(fmemout);

    // 2) regout
    for (int c = 0; c < NUM_CORES; c++) {
        FILE* freg = fopen(regoutFiles[c], "w");
        if (!freg) {
            fprintf(stderr, "Cannot open regout file %s\n", regoutFiles[c]);
            exit(1);
        }
        for (int r = 2; r < REGFILE_SIZE; r++) {
            fprintf(freg, "%08X\n", cores[c].regfile[r]);
        }
        fclose(freg);
    }

    // 3) dsram & tsram
    for (int c = 0; c < NUM_CORES; c++) {
        FILE* fds = fopen(dsramFiles[c], "w");
        FILE* fts = fopen(tsramFiles[c], "w");
        if (!fds || !fts) {
            fprintf(stderr, "Cannot open dsram/tsram files\n");
            exit(1);
        }
        for (int i = 0; i < DCACHE_NUM_LINES; i++) {
            CacheLine* line = &cores[c].dcache.lines[i];
            // dsram: the data block
            for (int w = 0; w < DCACHE_BLOCK_SIZE; w++) {
                fprintf(fds, "%08X\n", line->block[w]);
            }
            // tsram: (tag + mesi)
            uint32_t mesi = (line->mesi & 0x3);
            uint32_t ts = ((mesi << 12) | (line->tag & 0xFFF));
            fprintf(fts, "%08X\n", ts);
        }
        fclose(fds);
        fclose(fts);
    }

    // 4) stats
    for (int c = 0; c < NUM_CORES; c++) {
        FILE* fs = fopen(statsFiles[c], "w");
        if (!fs) {
            fprintf(stderr, "Cannot open stats file %s\n", statsFiles[c]);
            exit(1);
        }
        fprintf(fs, "cycles %lu\n", cores[c].cycleCount);
        fprintf(fs, "instructions %lu\n", cores[c].instrCount);
        fprintf(fs, "read_hit %lu\n", cores[c].read_hit);
        fprintf(fs, "write_hit %lu\n", cores[c].write_hit);
        fprintf(fs, "read_miss %lu\n", cores[c].read_miss);
        fprintf(fs, "write_miss %lu\n", cores[c].write_miss);
        fprintf(fs, "decode_stall %lu\n", cores[c].decode_stall_count);
        fprintf(fs, "mem_stall %lu\n", cores[c].mem_stall_count);
        fclose(fs);
    }
}