/* All the print functions in simulator */

#include <stdio.h>
#include <string.h>

#include "print.h"
#include "cpu.h"

void print_insn(stage_t* stage) {

	char rename = !(strcmp(stage->name, "Fetch") == 0);

	// no operand insn
	if(strcmp(stage->opcode, "NOP") == 0 || strcmp(stage->opcode, "HALT") == 0) printf("%s ", stage->opcode);

	// insn with only literal
	if(strcmp(stage->opcode, "MOVC") == 0) {
		printf("%s,R%d,#%d ", stage->opcode, stage->rd, stage->imm);
		if(rename) printf("(%s,U%d,#%d) ", stage->opcode, stage->u_rd, stage->imm);
	}
	if(strcmp(stage->opcode, "BZ") == 0 || strcmp(stage->opcode, "BNZ") == 0) {
		printf("%s,R%d,#%d ", stage->opcode, stage->rs1, stage->imm);
		if(rename) printf("(%s,U%d,#%d) ", stage->opcode, stage->u_rs1, stage->imm);
	}

	// insn with register and literal	
	if(strcmp(stage->opcode, "LOAD") == 0 || strcmp(stage->opcode, "ADDL") == 0 ||strcmp(stage->opcode, "SUBL") == 0) {
		printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rd, stage->rs1, stage->imm);
		if(rename) printf("(%s,U%d,U%d,#%d) ", stage->opcode, stage->u_rd, stage->u_rs1, stage->imm);
	}
	if(strcmp(stage->opcode, "STORE") == 0 || strcmp(stage->opcode, "JAL") == 0) {
		printf("%s,R%d,R%d,#%d ", stage->opcode, stage->rs2, stage->rs1, stage->imm);
		if(rename) printf("(%s,U%d,U%d,#%d) ", stage->opcode, stage->u_rs2, stage->u_rs1, stage->imm);
	}	
	if(strcmp(stage->opcode, "JUMP") == 0) {
		printf("%s,R%d,#%d ", stage->opcode, stage->rs1, stage->imm);
		if(rename) printf("(%s,U%d,#%d) ", stage->opcode, stage->u_rs1, stage->imm);
	}	

	// insn with only registers
	if(	strcmp(stage->opcode, "ADD") == 0 ||
		strcmp(stage->opcode, "SUB") == 0 ||
		strcmp(stage->opcode, "AND") == 0 ||
		strcmp(stage->opcode, "OR" ) == 0 ||
		strcmp(stage->opcode, "XOR") == 0 ||
		strcmp(stage->opcode, "MUL") == 0 ){
		
		printf("%s,R%d,R%d,R%d ", stage->opcode, stage->rd, stage->rs1, stage->rs2);
		if(rename) printf("(%s,U%d,U%d,U%d) ", stage->opcode, stage->u_rd, stage->u_rs1, stage->u_rs2);
	}

}

void print_stage_content(stage_t* stage) {
	printf("%-15s: pc(%d) ", stage->name, stage->pc);
	print_insn(stage);	
	if(strcmp(stage->name, "intFU") == 0 || strcmp(stage->name, "mulFU") == 0 || strcmp(stage->name, "memFU") == 0 ) {
		if(stage->busy > 0) {
			int lat;
			if(strcmp(stage->name, "intFU") == 0) lat = INT_FU_LAT;
			else if(strcmp(stage->name, "mulFU") == 0) lat = MUL_FU_LAT;
			else lat = MEM_FU_LAT;
			
			if(stage->busy == lat) printf("*issued ");
			else printf("busy %i ", stage->busy);
		}
	}	
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

	printf("%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s\n", "index", "taken", "dispatch", "pc", "opcode", "rs1", "rs1_rdy", "rs1_val", "rs2", "rs2_rdy", "rs2_val", "imm");
	for(int i=0; i<IQ_SIZE; i++) {
		iq_entry_t* iqe = &iq[i];
		printf("%-9i %-9i %-9i %-9i %-9s %-9i %-9i %-9i %-9i %-9i %-9i %-9i\n", i, iqe->taken, iqe->cycle_dispatched, iqe->pc, iqe->opcode, iqe->u_rs1, iqe->u_rs1_ready, iqe->u_rs1_val, iqe->u_rs2, iqe->u_rs2_ready, iqe->u_rs2_val, iqe->imm);	
	}
}

void print_memory(cpu_t* cpu) {
	printf("---Data memory---\n");
	int bytes_per_line = 64;	
	for(int i=0; i<4000; i+=bytes_per_line) {
		printf("%-4i: ", i);
		for(int j=0; j<bytes_per_line; j++) { // print 4 bytes per line
			printf("%i ", cpu->memory[i + j]);
		}
		printf("\n");	
	}
}

void print_lsq(lsq_t* lsq) {
	printf("---Load Store Queue---\n");
	printf("%-9s %-9s\n", "head_ptr", "tail_ptr");
	printf("%-9i %-9i\n", lsq->head_ptr, lsq->tail_ptr);

	printf("%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s\n", "index", "taken", "pc", "opcode", "valid", "mem_addr", "rd", "rs2_rdy", "rs2", "rs2_val");
	for(int i=0; i<LSQ_SIZE; i++) {
		lsq_entry_t* l = &lsq->entries[i];
		printf("%-9i %-9i %-9i %-9s %-9i %-9i %-9i %-9i %-9i %-9i\n", i, l->taken, l->pc, l->opcode, l->mem_addr_valid, l->mem_addr, l->u_rd, l->u_rs2_ready, l->u_rs2, l->u_rs2_val);	
	}
}

void print_rob(rob_t* rob) {
	printf("---Reorder Buffer---\n");
	printf("%-9s %-9s\n", "head_ptr", "tail_ptr");
	printf("%-9i %-9i\n", rob->head_ptr, rob->tail_ptr);

	printf("%-9s %-9s %-9s %-9s %-9s %-9s %-9s %-9s\n", "index", "taken", "valid", "pc", "opcode", "rd", "val", "lsq_idx");
	for(int i=0; i<ROB_SIZE; i++) {
		rob_entry_t* r = &rob->entries[i];
		printf("%-9i %-9i %-9i %-9i %-9s %-9i %-9i %-9i\n", i, r->taken, r->valid, r->pc, r->opcode, r->u_rd, r->u_rd_val, r->lsq_idx);	
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
	fu_t* mulFU = &cpu->mulFU;
	fu_t* memFU = &cpu->memFU;

	printf("---Functional Units---\n");
	stage_t* stage = &cpu->print_info[intFU->print_idx];
	stage->busy = intFU->busy;
	if(stage->busy < 0) stage = &cpu->print_info[cpu->code_size]; // NOP	
	strcpy(stage->name, "intFU");
	print_stage_content(stage);

	stage = &cpu->print_info[mulFU->print_idx];
	stage->busy = mulFU->busy;
	if(stage->busy < 0) stage = &cpu->print_info[cpu->code_size]; // NOP	
	strcpy(stage->name, "mulFU");
	print_stage_content(stage);
	
	printf("---Memory Function Unit---\n");
	stage = &cpu->print_info[memFU->print_idx];
	stage->busy = memFU->busy;
	if(stage->busy < 0) stage = &cpu->print_info[cpu->code_size]; // NOP	
	strcpy(stage->name, "memFU");
	print_stage_content(stage);

}

void print_cpu(cpu_t* cpu) {
	print_unified_regs(cpu->unified_regs);	
	print_rename_table(cpu);
	
	print_rob(&cpu->rob);
	print_lsq(&cpu->lsq);
	print_memory(cpu);
	print_iq(cpu->iq);
	
	print_all_FU(cpu);	
	print_arch_regs(cpu->arch_regs);
}

void update_print_stack(char* name, cpu_t* cpu, int idx) {		
	strcpy(cpu->print_info[idx].name, name); // update stage name
	cpu->print_stack[cpu->print_stack_ptr] = idx;
	cpu->print_stack_ptr++;
}
