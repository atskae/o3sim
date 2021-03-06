#ifndef CPU_H
#define CPU_H

/*

	CPU Configuration Values

*/

#define NUM_ARCH_REGS 16
#define NUM_UNIFIED_REGS 40
#define CODE_START_ADDR 4000
#define MEM_SIZE 4000

#define IQ_SIZE 16 // max entries of instruction queue
#define ROB_SIZE 32 // max entries of reorder buffer
#define LSQ_SIZE 20 // max entries in load-store queue

#define CFQ_SIZE 8

#define MAX_COMMIT_NUM 2 // max number of instructions that can commit

#define INT_FU_LAT 1
#define MUL_FU_LAT 2
#define MEM_FU_LAT 3

#define ZERO_FLAG NUM_ARCH_REGS // index into rename table that points to the u_rd of the most recent zero-flag value

/*

	Data Structures

*/

enum { F, DRF, DP, IS, EX, MEM, WB, CM, NUM_STAGES };

typedef struct insn_t {
	char opcode[128];
	int rd;
	int rs1;
	int rs2;
	int imm;
} insn_t;

/* latch for initial parts of the pipeline (Fetch, Decode) */
typedef struct stage_t { 
	char name[128]; // for printing

	int pc;
	char opcode[128];

	// architectural register addresses	
	int rd; // destination register address
	int rs1; // source 1 register address
	int rs2; // source 2 register address
	int imm; // literal value

	int u_rd;
	int u_rs1;
	int u_rs2;

	//status 	
	int busy;
	int stalled;

	// just for printing
	int print_idx;
	int rob_idx;
	int iq_idx;
	int lsq_idx;
	int cfid;
	
} stage_t;

// functional unit
typedef struct fu_t {	

	int print_idx; // just for printing
	
	int rob_idx;

	char opcode[128];
	int u_rd;
	int u_rd_val;
	int imm; // literal

	int u_rs1_val;
	int u_rs2_val;

	// only used by memFU
	int mem_addr;
	int rd; // so LOADs can free physical registers

	// BZ and BNZ
	int pc;
	char zero_flag;

	// control insn id
	int cfid;

	int busy;

} fu_t;

// architectural register
typedef struct areg_t {
	char valid;
	int u_rd; // ptr in URF where committed value is stored 
} areg_t;

// unified register
typedef struct ureg_t {
	char taken; // for register renaming
	char valid; // if register contains valid data
	char zero_flag; // for arithmetic operations
	
	int val; // data value
} ureg_t;

// reorder buffer
typedef struct rob_entry_t {
	char taken;
	char opcode[128];
	int pc; // program counter
	
	// architectural register addresses	
	int rd; // destination register address
	
	char valid; // if insn completed
	int u_rd; // unified destination register
	//int u_rd_val; // the actual register value is in the URF
	char zero_flag;

	int lsq_idx; // load-store queue index ; only needed for memory operations
	int cfid; // control flow insn id

} rob_entry_t;

typedef struct rob_t {	
	int head_ptr;
	int tail_ptr;
	rob_entry_t entries[ROB_SIZE];
} rob_t;

// instruction queue
typedef struct iq_entry_t {
	char taken;

	int pc; // just for printing

	int cycle_dispatched; // earliest insn is issued first
	char opcode[128];
	int imm; // literal operand

	int u_rs1; // unified register address ; used for tag matching
	char u_rs1_ready; // bit indicates if ready
	int u_rs1_val;

	int u_rs2;
	char u_rs2_ready;
	int u_rs2_val;

	// only for BZ and BNZ
	int zero_flag_u_rd; // the unified register that will hold the zero-flag value
	char zero_flag_ready; 

	int rob_idx; // where to send computed value
	int lsq_idx; // where to send computed memory address ; only needed for memory operations
	int cfid; // control flow id

} iq_entry_t;

// load-store queue
typedef struct lsq_entry_y {
	char taken;	
	char done; // memory operation done, did not commit
	int rob_idx;
	int pc; 
	int cfid; // control-flow id

	char opcode[128]; // load or store
	char mem_addr_valid;
	int mem_addr;
	
	int u_rd; // target unified register addr (not value!); for load insn only
	int u_rd_ready;
	int u_rd_val;
	
	char u_rs2_ready; // data to be stored ; for store only
	int u_rs2; // register address that holds value to be stored
	int u_rs2_val; // value to be stored	
} lsq_entry_t;

typedef struct lsq_t {
	int head_ptr;
	int tail_ptr;
	lsq_entry_t entries[LSQ_SIZE];
} lsq_t;

// for control-flow insn
typedef struct saved_state_t {
	ureg_t unified_regs[NUM_UNIFIED_REGS];
	int front_rename_table[NUM_ARCH_REGS];	
} saved_state_t;

typedef struct print_info_t {
	char name[128];
	int idx; // index into cpu->print_info
} print_info_t;

typedef struct cpu_t {
	int clock;
	int stop_cycle; // when to stop the simulation
	char done; // if no more valid instructions are coming out of Fetch, stop
	
	int pc;		
	insn_t* code;
	int code_size;	
	int memory[4096];	

	areg_t arch_regs[NUM_ARCH_REGS];	
	ureg_t unified_regs[NUM_UNIFIED_REGS];
	stage_t stage[NUM_STAGES];

	int front_rename_table[NUM_ARCH_REGS + 1]; // arch reg -> unified reg mapping ; +1 for zero-flag
	int back_rename_table[NUM_ARCH_REGS + 1]; // arch reg -> commited values in unified reg file ; +1 for zero flag

	rob_t rob;
	iq_entry_t iq[IQ_SIZE];
	lsq_t lsq;

	/* functional units */
	fu_t intFU;
	fu_t mulFU;
	fu_t memFU; 

	/* control flow handling (BZ, BNZ, JUMP) */
	int cfid; // the current cfid ; change with every control-flow insn
	char cfid_freelist[CFQ_SIZE]; // list of free ids ; index = id, value = free/taken
	int cfq[CFQ_SIZE];
	int cfq_head_ptr;
	int cfq_tail_ptr;
	saved_state_t saved_state[CFQ_SIZE]; // index by using cfid

	// holds all instruction information ; to index into this, use get_code_index(pc)
	stage_t* print_info;

	// info about current cycle only	
	print_info_t print_stack[16]; // holds indicies into print_info[]
	int print_stack_ptr;
	
	char print_memory; // no need to pring memory all the time ; only when mem access occurs and at the last displayed cycle

} cpu_t;

/*

	Functions

*/

insn_t* create_code(const char* filename, int* size);
cpu_t* cpu_init(const char* filename);
int cpu_run(cpu_t* cpu, char* command);
void cpu_stop(cpu_t* cpu);

/* Pipeline stages */
int fetch(cpu_t* cpu);
int decode(cpu_t* cpu);
int issue(cpu_t* cpu);
int execute(cpu_t* cpu);
int memory(cpu_t* cpu);
int commit(cpu_t* cpu); // insn cannot writeback and commit in the same cycle ; write to arch_reg file

#endif // CPU_H
