/* All the print functions in simulator */

#include <stdio.h>
#include <string.h>

#include "print.h"
#include "cpu.h"

void print_insn(stage_t* stage) {

	char renamed = (strcmp(stage->name, "Decode") == 0);
	
	if(strcmp(stage->opcode, "MOVC") == 0) {
		printf("%s,R%d,#%d ", stage->opcode, stage->rd, stage->imm);
		if(renamed) printf("(%s,U%d,#%d) ", stage->opcode, stage->u_rd, stage->imm);
	}

	if(strcmp(stage->opcode, "STORE") == 0) {
		printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rs1, stage->rs2, stage->imm);
	}
}

void print_stage_content(char* name, stage_t* stage) {
	printf("%-15s: pc(%d) ", name, stage->pc);
	print_insn(stage);
	printf("\n");
}

void print_rename_table(cpu_t* cpu) {
	printf("---Rename Table---\n");
	printf("%-15s %-15s\n", "Frontend", "Backend");
	for(int i=0; i<NUM_ARCH_REGS; i++) {
		printf("R%-2i: U%-9i R%-2i: U%-9i\n", i, cpu->front_rename_table[i], i, cpu->back_rename_table[i]);
	}
}

void print_iq(iq_entry_t* iq) {
	printf("---Instruction Queue---\n");

	printf("%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s\n", "index", "taken", "opcode", "rs1", "rs1_rdy", "rs1_val", "rs2", "rs2_rdy", "rs2_val", "imm");
	for(int i=0; i<IQ_SIZE; i++) {
		iq_entry_t* iqe = &iq[i];
		printf("%-9i %-9i %-9s %-9i %-9i %-9i %-9i %-9i %-9i %-9i\n", i, iqe->taken, iqe->opcode, iqe->u_rs1, iqe->u_rs1_ready, iqe->u_rs1_val, iqe->u_rs2, iqe->u_rs2_ready, iqe->u_rs2_val, iqe->imm);	
	}
}

void print_lsq(lsq_t* lsq) {
	printf("---Load Store Queue---\n");
	printf("%-9s %-9s\n", "head_ptr", "tail_ptr");
	printf("%-9i %-9i\n", lsq->head_ptr, lsq->tail_ptr);

	printf("%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s\n", "index", "taken", "opcode", "valid", "mem_addr", "rd", "rs2_rdy", "rs2", "rs2_val");
	for(int i=0; i<LSQ_SIZE; i++) {
		lsq_entry_t* l = &lsq->entries[i];
		printf("%-9i %-9i %-9s %-9i %-9i %-9i %-9i %-9i %-9i\n", i, l->taken, l->opcode, l->mem_addr_valid, l->mem_addr, l->u_rd, l->u_rs2_ready, l->u_rs2, l->u_rs2_val);	
	}
}

void print_rob(rob_t* rob) {
	printf("---Reorder Buffer---\n");
	printf("%-9s %-9s\n", "head_ptr", "tail_ptr");
	printf("%-9i %-9i\n", rob->head_ptr, rob->tail_ptr);

	printf("%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s\n", "index", "taken", "valid", "opcode", "pc", "rd", "val", "lsq_idx");
	for(int i=0; i<ROB_SIZE; i++) {
		rob_entry_t* r = &rob->entries[i];
		printf("%-9i %-9i %-9i %-9s %-9i %-9i %-9i %-9i\n", i, r->taken, r->valid, r->opcode, r->pc, r->u_rd, r->u_rd_val, r->lsq_idx);	
	}
}

void print_unified_regs(ureg_t* regs) {
	printf("---Unified Registers---\n");
	printf("%-9s %-9s %-9s %-9s %-9s\n", "reg", "taken", "valid", "zero", "value");
	for(int i=0; i<NUM_UNIFIED_REGS; i++) {
		printf("U%-9i %-9i %-9i %-9i %-9i\n", i, regs[i].taken, regs[i].valid, regs[i].zero_flag, regs[i].val);
	}	
}

void print_arch_regs(areg_t* regs) {
	printf("---Architectural Registers---\n");
	printf("%-9s %-9s %-9s\n", "reg", "valid", "u_rd");
	for(int i=0; i<NUM_ARCH_REGS; i++) {
		printf("R%-9i %-9i %-9i\n", i, regs[i].valid, regs[i].u_rd);
	}		
}

void print_all_FU(cpu_t* cpu) {

	fu_t* intFU = &cpu->intFU;
	fu_t* multFU = &cpu->multFU;
	fu_t* memFU = &cpu->memFU;

	printf("---EX Functional Units---\n");
	printf("%-9s %-9s %-9s\n", "field", "intFU", "multFU");	
	printf("%-9s %-9i %-9i\n", "rob_idx", intFU->rob_idx, multFU->rob_idx);	
	printf("%-9s %-9i %-9i\n", "busy", intFU->busy, multFU->busy);	

	printf("---MEM Function Unit---\n");
	printf("%-9s %-9s\n", "field", "memFU");	
	printf("%-9s %-9i\n", "rob_idx", memFU->rob_idx);	
	printf("%-9s %-9i\n", "busy", memFU->busy);	

}

void print_cpu(cpu_t* cpu) {
	print_arch_regs(cpu->arch_regs);
	print_unified_regs(cpu->unified_regs);
	
	print_rename_table(cpu);

	print_rob(&cpu->rob);
	//print_lsq(&cpu->lsq);
	print_iq(cpu->iq);
	print_all_FU(cpu);
}

void update_print_stack(char* name, cpu_t* cpu, stage_t* stage) {
	
	stage_t* new_stage = &cpu->print_stack[cpu->print_stack_ptr];
	strcpy(new_stage->name, name);
	new_stage->pc = stage->pc;
	strcpy(new_stage->opcode, stage->opcode);

	new_stage->rd = stage->rd;
	new_stage->rs1 = stage->rs1;
	new_stage->rs2 = stage->rs2;
	new_stage->imm = stage->imm;	

	//new_stage->u_rd = stage->u_rd;
	//new_stage->u_rs1 = stage->u_rs1;
	//new_stage->u_rs2 = stage->u_rs2;

	//new_stage->rs1_val = stage->rs1_val;
	//new_stage->rs2_val = stage->rs2_val;	
	//new_stage->val = stage->val;
	//new_stage->mem_addr = stage->mem_addr;

	// new_stage->zero_flag = stage->zero_flag;
	new_stage->stalled = stage->stalled;
	new_stage->busy = stage->busy;

	cpu->print_stack_ptr++;
}
