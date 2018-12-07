#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "print.h" // all printing functions

#define PRINT 1

cpu_t* cpu_init(const char* filename) {
	
	if(!filename) return NULL;
	
	cpu_t* cpu = malloc(sizeof(cpu_t));
	if(!cpu) return NULL;

	cpu->clock = 0;	
	cpu->pc = CODE_START_ADDR;
	memset(cpu->arch_regs, 0, sizeof(areg_t) * NUM_ARCH_REGS);
	memset(cpu->unified_regs, 0, sizeof(ureg_t) * NUM_UNIFIED_REGS);
	
	memset(cpu->front_rename_table, -1, NUM_ARCH_REGS * sizeof(int));	
	memset(cpu->back_rename_table, -1, NUM_ARCH_REGS * sizeof(int));	

	memset(&cpu->rob, 0, sizeof(rob_t));	
	memset(cpu->iq, 0, sizeof(iq_entry_t));	
	memset(&cpu->lsq, 0, sizeof(lsq_t));	

	memset(cpu->stage, 0, sizeof(stage_t) * NUM_STAGES);
	memset(cpu->memory, 0, sizeof(int) * MEM_SIZE);
			
	// obtain instructions from .asm file	
	cpu->code = create_code(filename, &cpu->code_size);
	
	if(!cpu->code) {
		free(cpu);
		return NULL;
	}
	
	if(PRINT) {
		fprintf(stderr, "sim> CPU initialized ; %i instructions\n", cpu->code_size);
		printf("%-9s %-9s %-9s %-9s %-9s %-9s\n", "pc", "opcode", "rd", "rs1", "rs2", "imm");
		
		for (int i = 0; i < cpu->code_size; ++i) {
			printf("%-9d %-9s %-9d %-9d %-9d %-9d\n",
			CODE_START_ADDR + i*4,
			cpu->code[i].opcode,
			cpu->code[i].rd,
			cpu->code[i].rs1,
			cpu->code[i].rs2,
			cpu->code[i].imm);
		}
	}

	// solely for printing purposes
	cpu->print_info = (stage_t*) malloc((cpu->code_size + NUM_STAGES) * sizeof(stage_t));
	memset(cpu->print_info, 0,(cpu->code_size + NUM_STAGES) * sizeof(stage_t));
	for(int i=0; i<NUM_STAGES; i++) {
		stage_t* nop = &cpu->print_info[cpu->code_size + i];	
		strcpy(nop->opcode, "NOP");
		nop->pc = 0;
	}
	
	cpu->intFU.busy = 0;
	cpu->intFU.print_idx = cpu->code_size;
	
	cpu->multFU.busy = 0;
	cpu->multFU.print_idx = cpu->code_size;
	
	cpu->memFU.busy = 0;
	cpu->memFU.print_idx = cpu->code_size;
	
	for(int i=1; i<NUM_STAGES; i++) {
		cpu->stage[i].stalled = 1;
	}
	
	return cpu;
}

void cpu_stop(cpu_t* cpu) {
	free(cpu->code);
	free(cpu);
}

int get_code_index(int pc) {
	return (pc - CODE_START_ADDR) / 4;
}

char is_intFU(char* opcode) {
	if( strcmp(opcode, "ADD") 	== 0 ||
		strcmp(opcode, "SUB") 	== 0 ||
		strcmp(opcode, "AND") 	== 0 ||
		strcmp(opcode, "OR") 	== 0 ||
		strcmp(opcode, "XOR")   == 0 ||
		strcmp(opcode, "MOVC") 	== 0 ||
		strcmp(opcode, "ADDL") 	== 0 ||
		strcmp(opcode, "SUBL") 	== 0 ){
		return 1;
	}
	else return 0;
}

char is_mem(char* opcode) {
	if( strcmp(opcode, "LOAD") == 0 || strcmp(opcode, "STORE") == 0 ) return 1;
	else return 0;
}

// checks if this insn writes to a register
char has_rd(char* opcode) {
	if( strcmp(opcode, "ADD") 	== 0 ||
		strcmp(opcode, "SUB") 	== 0 ||
		strcmp(opcode, "AND") 	== 0 ||
		strcmp(opcode, "OR") 	== 0 ||
		strcmp(opcode, "XOR")   == 0 ||
		strcmp(opcode, "MUL") 	== 0 ||
		strcmp(opcode, "MOVC") 	== 0 ||
		strcmp(opcode, "LOAD") 	== 0 ||
		strcmp(opcode, "ADDL") 	== 0 ||
		strcmp(opcode, "SUBL") 	== 0 ){	
		return 1;
	}
	else return 0;
}

char is_valid_insn(char* opcode) {
	if( strcmp(opcode, "ADD") 	== 0 ||
		strcmp(opcode, "SUB") 	== 0 ||
		strcmp(opcode, "AND") 	== 0 ||
		strcmp(opcode, "OR") 	== 0 ||
		strcmp(opcode, "XOR")   == 0 ||
		strcmp(opcode, "MUL") 	== 0 ||
		strcmp(opcode, "MOVC")  == 0 ||
		strcmp(opcode, "LOAD")  == 0 ||
		strcmp(opcode, "STORE") == 0 ||
		strcmp(opcode, "BZ")    == 0 ||
		strcmp(opcode, "BNZ")   == 0 ||
		strcmp(opcode, "JUMP")  == 0 ||
		strcmp(opcode, "NOP")  	== 0 ||
		strcmp(opcode, "HALT")  == 0 ||
		// new insn
		strcmp(opcode, "ADDL")  == 0 ||
		strcmp(opcode, "SUBL")  == 0 ||
		strcmp(opcode, "JAL")   == 0 ){
		return 1;
	}	
	
	return 0;
}

int fetch(cpu_t* cpu) {
	
	stage_t* stage = &cpu->stage[F];
	if(!stage->busy && !stage->stalled) {  
			
		// get insn from code mem ; copy values to stage latch	
		stage->pc = cpu->pc;

		insn_t* insn = &cpu->code[get_code_index(cpu->pc)];
		strcpy(stage->opcode, insn->opcode);	
		if(!is_valid_insn(stage->opcode)) {
			stage->stalled = 1;
			stage->pc = -1;
			cpu->stage[DRF] = cpu->stage[F];
			update_print_stack("Fetch", cpu, cpu->code_size + F);
			return 0;	
		}

		stage->rd = insn->rd;
		stage->rs1 = insn->rs1;
		stage->rs2 = insn->rs2;
		stage->imm = insn->imm;
		stage->rd = insn->rd;
		
		// update pc for next insn
		cpu->pc += 4;
	
		// create print_info for this insn
		stage_t* p = &cpu->print_info[get_code_index(stage->pc)];
		p->pc = stage->pc;
		strcpy(p->opcode, insn->opcode);
		p->rd = insn->rd;
		p->rs1 = insn->rs1;
		p->rs2 = insn->rs2;
		p->imm = insn->imm;
		p->rd = insn->rd;

		cpu->stage[DRF] = cpu->stage[F];
		
	}

	if(!is_valid_insn(stage->opcode)) update_print_stack("Fetch", cpu, cpu->code_size + F);
	else update_print_stack("Fetch", cpu, get_code_index(stage->pc));

	return 0;
}

int decode(cpu_t* cpu) {
	stage_t* stage = &cpu->stage[DRF];
	if (!stage->busy && !stage->stalled) {
		
		if(!is_valid_insn(stage->opcode)) {
			stage->stalled = 1;
			stage->pc = -1;
			update_print_stack("Decode", cpu, cpu->code_size + DRF);
			return 0;	
		}

		/*
			Rename instruction
		*/
		// allocate a unified register if this instruction writes to a register	
		if(has_rd(stage->opcode)) {
			
			// scan URF for free unified register
			for(int i=0; i<NUM_UNIFIED_REGS; i++) {	
				ureg_t* r = &cpu->unified_regs[i];
				if(!r->taken) {
					stage->u_rd = i;
					r->taken = 1; // allocate unified reg
					r->valid = 0;
					r->zero_flag = 0;
					break;
				}
			}
			
			if(stage->u_rd == -1) { // no more free unified registers ; stall
				printf("No more unified registers. Insert logic here...\n");	
			}

			// rename arch_reg -> unified_reg
			stage->u_rd = stage->u_rd;
			
			// update frontend rename table
			cpu->front_rename_table[stage->rd] = stage->u_rd;
		
		}
		// rename the source registers ; if no source, renamed register is simply -1
		stage->u_rs1 = cpu->front_rename_table[stage->rs1];
		stage->u_rs2 = cpu->front_rename_table[stage->rs2];
	
		/*
			Create LSQ (if mem), IQ, and ROB entries
		*/		
		// create an LSQ entry if this is a memory operation	
		int lsq_idx = -1; // IQ entry needs this value
		if(is_mem(stage->opcode)) {
			lsq_t* lsq = &cpu->lsq;		
			// if LSQ is full
			if(lsq->entries[lsq->tail_ptr].taken) {
				printf("LSQ full... Insert logic here.\n");
			} else { // create an LSQ entry
				lsq_idx = lsq->tail_ptr;
				lsq_entry_t* lsqe = &lsq->entries[lsq_idx];
		
				lsqe->pc = stage->pc; // just for printing
	
				lsqe->taken = 1;
				strcpy(lsqe->opcode, stage->opcode); // load or store
				lsqe->mem_addr_valid = 0;	
				 // only for loads	
				lsqe->u_rd = stage->u_rd;
				// only for stores
				lsqe->u_rs2_ready = 0;
				lsqe->u_rs2 = stage->u_rs2;							
	
				lsq->tail_ptr = (lsq->tail_ptr + 1) % LSQ_SIZE;
			}
		} // create LSQ entry ; end

	
		// create ROB entry
		rob_t* rob = &cpu->rob;
		int rob_idx = -1;
		if(rob->entries[rob->tail_ptr].taken) {
			printf("ROB full. Insert logic here...\n");
		} else {
			rob_idx = rob->tail_ptr;
			rob_entry_t* robe = &rob->entries[rob_idx];
			robe->taken = 1;
			robe->valid = 0;
			robe->commit_ready= 0;
			strcpy(robe->opcode, stage->opcode);
			robe->pc = stage->pc;
			robe->u_rd = stage->u_rd;
			robe->lsq_idx = lsq_idx;		

			rob->tail_ptr = (rob->tail_ptr + 1) % ROB_SIZE;
	
		}

		// create IQ entry
		iq_entry_t* iq = cpu->iq;
		int iq_idx = -1;	
		// scan IQ and look for a free entry
		for(int i=0; i<IQ_SIZE; i++) {
			iq_entry_t* iqe = &iq[i];
			if(!iqe->taken) {
				iq_idx = i;
				iqe->taken = 1;
				iqe->cycle_issued = cpu->clock;

				iqe->pc = stage->pc; // just for printing
				iqe->rob_idx = rob_idx;		
	
				strcpy(iqe->opcode, stage->opcode);
				iqe->imm = stage->imm;
				
				iqe->u_rs1 = stage->u_rs1;
				iqe->u_rs1_ready = 0;

				iqe->u_rs2 = stage->u_rs2;
				iqe->u_rs2_ready = 0;

				// check if insn do not need particular source registers ; set them to ready so they do not wait for them 
				
				// insn with only literal	
				if(strcmp(iqe->opcode, "MOVC") == 0) {
					iqe->u_rs1_ready = 1;
					iqe->u_rs2_ready = 1;
				}
				if(strcmp(iqe->opcode, "BZ") == 0 || strcmp(iqe->opcode, "BNZ") == 0) {
					iqe->u_rs1_ready = 1;
				}
				// insn with register and literal	
				if(strcmp(iqe->opcode, "LOAD") == 0 || strcmp(iqe->opcode, "ADDL") == 0 || strcmp(iqe->opcode, "SUBL") == 0) {
					iqe->u_rs2_ready = 1;
				}
				if(strcmp(iqe->opcode, "JUMP") == 0) {
					iqe->u_rs2_ready = 1;
				}	

				iqe->lsq_idx = lsq_idx;
			
				break;
			}
		}
		if(iq_idx < 0) {
			printf("IQ full... insert logic here...\n");
		}	
	
		// update print info
		stage_t* p = &cpu->print_info[get_code_index(stage->pc)];
		p->u_rd = stage->u_rd;
		p->u_rs1 = stage->u_rs1;
		p->u_rs2 = stage->u_rs2;
		p->rob_idx = rob_idx;
		p->iq_idx = iq_idx;
		p->lsq_idx = lsq_idx;

	}
	
	if(!is_valid_insn(stage->opcode)) update_print_stack("Decode", cpu, cpu->code_size + DRF);
	else update_print_stack("Decode", cpu, get_code_index(stage->pc));
	
	return 0;
}

int issue(cpu_t* cpu) {
			
	// look for the earliest dispatched insn with all operands ready and send to FU	
	iq_entry_t* iq = cpu->iq;
	for(int i=0; i<IQ_SIZE; i++) {
		iq_entry_t* iqe = &iq[i];
		if(cpu->intFU.busy < 0) { // if this FU is free
			if(iqe->taken && is_intFU(iqe->opcode) && iqe->u_rs1_ready && iqe->u_rs2_ready ) {	
				iqe->taken = 0; // free this IQ entry
				
				// send this insn to intFU
				fu_t* intFU = &cpu->intFU;
				rob_entry_t* robe = &cpu->rob.entries[iqe->rob_idx];
				intFU->rob_idx = iqe->rob_idx;
				strcpy(intFU->opcode, iqe->opcode);
				intFU->u_rd = robe->u_rd; // target register
			
				intFU->imm = iqe->imm;
				intFU->u_rs1_val = iqe->u_rs1_val;
				intFU->u_rs2_val = iqe->u_rs2_val;
					
				intFU->busy = INT_FU_LAT; // latency + issue latency

				// printing stuff
				intFU->print_idx = get_code_index(iqe->pc);
				update_print_stack("Issue", cpu, get_code_index(iqe->pc));
				break;	
			}
		}
		//else if( !sent_multFU && (strcmp(iqe->opcode, "MULT") == 0) && iqe->u_rs1_ready && iqe->u_rs2_ready ) {	
		//	sent_multFU = 1; 
		//	iqe->taken = 0; // free this IQ entry
		//	cpu->multFU.insn = cpu->stage[IS]; // send to EX stage to multFU
		//  cpu->busy = MULT_FU_LAT; // latency
		//}
		//else if( !sent_memFU && is_mem(iqe->opcode) && iqe->u_rs1_ready && iqe->u_rs2_ready ) {
		//	sent_memFU = 1; 
		//	iqe->taken = 0; // free this IQ entry
		//	cpu->memFU.insn = cpu->stage[IS]; // send to EX stage to memFU
		//  cpu->busy = MEM_FU_LAT; // latency
		//}
	}	
			
	return 0;
	
}

int execute(cpu_t* cpu) {

	// check each FU

	// intFU	
	fu_t* intFU = &cpu->intFU;
	intFU->busy--;
	if(intFU->busy >= 0) {

		rob_entry_t* robe = &cpu->rob.entries[intFU->rob_idx];	

		// perform computation
		if(strcmp(intFU->opcode, "MOVC") == 0) robe->u_rd_val = intFU->imm + 0;	
		else if(strcmp(intFU->opcode, "ADD") == 0) robe->u_rd_val = intFU->u_rs1_val + intFU->u_rs2_val;
		else if(strcmp(intFU->opcode, "SUB") == 0) robe->u_rd_val = intFU->u_rs1_val - intFU->u_rs2_val;
		else if(strcmp(intFU->opcode, "AND") == 0) robe->u_rd_val = intFU->u_rs1_val & intFU->u_rs2_val;
		else if(strcmp(intFU->opcode, "XOR") == 0) robe->u_rd_val = intFU->u_rs1_val ^ intFU->u_rs2_val;	
		else if(strcmp(intFU->opcode, "ADDL") == 0) robe->u_rd_val = intFU->u_rs1_val + intFU->imm;
		else if(strcmp(intFU->opcode, "SUBL") == 0) robe->u_rd_val = intFU->u_rs1_val - intFU->imm;	
		robe->valid = 1;
		
		// broadcast calculated value to waiting insn in IQ
		iq_entry_t* iq = cpu->iq;
		for(int i=0; i<IQ_SIZE; i++) {
			iq_entry_t* iqe = &iq[i];
			if(iqe->taken) {
				if(intFU->u_rd == iq->u_rs1) {
					iqe->u_rs1_ready = 1;
					iqe->u_rs1_val = robe->u_rd_val;	
				}
				if(intFU->u_rd == iq->u_rs2) {
					iqe->u_rs2_ready = 1;
					iqe->u_rs2_val = robe->u_rd_val;	
				}
			}
		} // broadcast ; end
		update_print_stack("Execute", cpu, intFU->print_idx);
		
	} 
	// multFU
	//fu_t* multFU = &cpu->multFU;
	//if(multFU.busy > 0) {
	//	multFU.busy--;
	//	if( !multFU->busy && !intFU->stalled) {
	//
	//	}	
	//}	

	return 0;
}

int memory(cpu_t* cpu) {
	
	//fu_t* memFU = &cpu->memFU;
	//if(memFU->busy > 0) memFU->busy--;
	//if( !memFU->busy && !memFU->stalled) {
	//
	//}

	return 0;
}

int writeback(cpu_t* cpu) {

	// write result to URF	
	int head_ptr = cpu->rob.head_ptr;
	// get ROB entry at the head of ROB
	rob_entry_t* robe = &cpu->rob.entries[head_ptr];
	if(robe->valid) { // insn has completed
		// write result to URF
		cpu->unified_regs[robe->u_rd].val = robe->u_rd_val;
		robe->commit_ready = 1;
		update_print_stack("Writeback", cpu, get_code_index(robe->pc));
	} 

	return 0;
}

int commit(cpu_t* cpu) {
	
	for(int i=0; i<MAX_COMMIT_NUM; i++) {		
		int ptr = cpu->rob.head_ptr;
		// release ROB entry
		// get ROB entry at the head of ROB
		rob_entry_t* robe = &cpu->rob.entries[ptr];
		if(robe->commit_ready) { // insn has wrote to URF 
			// set arch reg mapping to URF
			cpu->arch_regs[robe->rd].u_rd = robe->u_rd;
			robe->taken = 0;
			//robe->valid = 0;
			//robe->pc = 0;
			//robe->u_rd_val = 0;

			cpu->rob.head_ptr = (cpu->rob.head_ptr + 1) % ROB_SIZE; // update ROB head_ptr		
		
			update_print_stack("Commit", cpu, get_code_index(robe->pc));
		} else break; // can't commit further insn 
	}

	return 0;
}

char no_more_insn(cpu_t* cpu) {

	char rob_empty = 0;
	if(cpu->rob.head_ptr == cpu->rob.tail_ptr && !cpu->rob.entries[cpu->rob.head_ptr].taken) rob_empty = 1;

	char done = 1;
	int ptr = cpu->print_stack_ptr - 1;
	for(int i=ptr; i>=0; i--) {
		int idx = cpu->print_stack[i];
		stage_t* stage = &cpu->print_info[idx];
		if(strcmp(stage->name, "Commit") == 0) continue;
		if(is_valid_insn(stage->opcode) && strcmp(stage->opcode, "NOP")) {
			done = 0;
			break;
		}
	}

	return (rob_empty && done);
}

/* Main simulation loop */
int cpu_run(cpu_t* cpu) {
	
	while(1) {
	
		cpu->clock++;				
		cpu->print_stack_ptr = 0; // reset

		commit(cpu);
		writeback(cpu);
		memory(cpu);
		execute(cpu);
		issue(cpu);
		decode(cpu);
		fetch(cpu);
		
		if(PRINT) {
			printf("--------------------------------\n");
			printf("Clock Cycle # %d\n", cpu->clock);
			printf("--------------------------------\n");

			// print stage contents
			int ptr = cpu->print_stack_ptr - 1;	
			for(int i=ptr; i>=0; i--) {
				int idx = cpu->print_stack[i];
				stage_t* stage = &cpu->print_info[idx];
				print_stage_content(stage);
			}
		
			print_cpu(cpu); // prints reg files, rob, lsq, etc...
		}
		
		cpu->done = no_more_insn(cpu);
		if(cpu->clock == cpu->stop_cycle || cpu->done) {
			printf("sim> Reached %i cycles\n", cpu->clock);
			if(cpu->done) printf("sim> No more instructions to simulate.\n");
			break;
		}

	}
		
	return 0;
}
