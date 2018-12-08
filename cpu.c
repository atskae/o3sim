#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // INT_MAX
#include <assert.h>

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
	for(int i=0; i<NUM_UNIFIED_REGS; i++) {
		cpu->unified_regs[i].valid = 1;
	}		
	
	memset(cpu->front_rename_table, -1, NUM_ARCH_REGS * sizeof(int));	
	memset(cpu->back_rename_table, -1, NUM_ARCH_REGS * sizeof(int));	

	memset(&cpu->rob, 0, sizeof(rob_t));	
	memset(cpu->iq, 0, sizeof(iq_entry_t));	
	memset(&cpu->lsq, 0, sizeof(lsq_t));	

	memset(cpu->stage, 0, sizeof(stage_t) * NUM_STAGES);
	memset(cpu->memory, 0, sizeof(int) * MEM_SIZE);
		
	cpu->cfid = -1;
	memset(cpu->cfid_freelist, 0, CFQ_SIZE);
	cpu->cfq_head_ptr = 0;
	cpu->cfq_tail_ptr = 0;
	
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
	
	cpu->mulFU.busy = 0;
	cpu->mulFU.print_idx = cpu->code_size;
	
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

char is_controlflow(char* opcode) {
	if( strcmp(opcode, "BZ") 	== 0 ||
		strcmp(opcode, "BNZ") 	== 0 ||
		strcmp(opcode, "JUMP") 	== 0 ){
		return 1;
	}
	else return 0;
}

char is_halt(char* opcode) {
	return strcmp(opcode, "HALT") == 0;
}

char is_nop(char* opcode) {
	return strcmp(opcode, "NOP") == 0;
}

char is_intFU(char* opcode) {
	if( strcmp(opcode, "ADD") 	== 0 ||
		strcmp(opcode, "SUB") 	== 0 ||
		strcmp(opcode, "AND") 	== 0 ||
		strcmp(opcode, "OR") 	== 0 ||
		strcmp(opcode, "XOR")   == 0 ||
		strcmp(opcode, "MOVC") 	== 0 ||
		strcmp(opcode, "ADDL") 	== 0 ||
		strcmp(opcode, "SUBL") 	== 0 ||
		strcmp(opcode, "LOAD") 	== 0 || // addr calculation
		strcmp(opcode, "STORE") == 0 || // addr calculation
		strcmp(opcode, "JUMP") 	== 0 ){ // addr calculation

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
			stage->pc = -1;
			cpu->stage[DRF] = cpu->stage[F];	
			cpu->stage[F].stalled = 1;
			update_print_stack("Fetch", cpu, cpu->code_size + F);
			return 0;	
		}
		if(is_nop(stage->opcode)) return 0;

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

// rename instruction and obtain ready operands
int decode(cpu_t* cpu) {
	stage_t* stage = &cpu->stage[DRF];
	if(!stage->busy && !stage->stalled) {
		
		if(!is_valid_insn(stage->opcode)) {
			stage->pc = -1;
			cpu->stage[DP] = cpu->stage[DRF]; 	
			cpu->stage[DRF].stalled = 1;
			update_print_stack("Decode", cpu, cpu->code_size + DRF);
			return 0;	
		}

		if(is_halt(stage->opcode)) {
			cpu->stage[DP] = cpu->stage[DRF]; // move the halt to the next stage
			cpu->stage[DRF].stalled = 1;
			update_print_stack("Decode", cpu, get_code_index(stage->pc));
			return 0;
		}

		// rename the source registers ; if no source, renamed register is simply -1
		stage->u_rs1 = cpu->front_rename_table[stage->rs1];
		stage->u_rs2 = cpu->front_rename_table[stage->rs2];	
	
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
			
			cpu->unified_regs[stage->u_rd].valid = 0;	
		}
		
		// update print info
		stage_t* p = &cpu->print_info[get_code_index(stage->pc)];
		p->u_rd = stage->u_rd;
		p->u_rs1 = stage->u_rs1;
		p->u_rs2 = stage->u_rs2;

		cpu->stage[DP] = cpu->stage[DRF]; // move to dispatch

	}
	
	if(!is_valid_insn(stage->opcode)) update_print_stack("Decode", cpu, cpu->code_size + DRF);
	else update_print_stack("Decode", cpu, get_code_index(stage->pc));
	
	return 0;
}

// create LSQ, IQ, and ROB entries
int dispatch(cpu_t* cpu) {
	
	stage_t* stage = &cpu->stage[DP];
	if(!stage->busy && !stage->stalled) {	
		
		if(!is_valid_insn(stage->opcode)) {
			stage->pc = -1;	
			cpu->stage[DP].stalled = 1;
			update_print_stack("Dispatch", cpu, cpu->code_size + DP);
			return 0;	
		}
		if(is_nop(stage->opcode)) return 0;
	
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
			
				lsqe->done = 0;	
				lsqe->pc = stage->pc;
	
				lsqe->taken = 1;
				strcpy(lsqe->opcode, stage->opcode); // load or store
				lsqe->mem_addr_valid = 0;	
				lsqe->cfid = cpu->cfid; // control-flow insn

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
			strcpy(robe->opcode, stage->opcode);
			robe->pc = stage->pc;
			robe->rd = stage->rd;
			robe->u_rd = stage->u_rd;
			robe->lsq_idx = lsq_idx;		
			robe->cfid = cpu->cfid;	// control-flow id

			if(is_mem(stage->opcode) && lsq_idx != -1) cpu->lsq.entries[lsq_idx].rob_idx = rob_idx;
			if(is_halt(stage->opcode)) {
				robe->valid = 1;
			} if(is_controlflow(stage->opcode)) { // BZ, BNZ, JUMP
				// look for a free cfid
				char found = 0;
				for(int i=0; i<CFQ_SIZE; i++) {
					if(!cpu->cfid_freelist[i]) {
						cpu->cfid = i;
						found = 1;
						break;	
					}
				}
				if(!found) {
					printf("No more cfids. Insert logic here...\n");
				}

				// add new cfid to cfq
				cpu->cfq[cpu->cfq_tail_ptr] = cpu->cfid;
				cpu->cfq_tail_ptr = (cpu->cfq_tail_ptr + 1) % CFQ_SIZE;

				// save the state of URF and rename table ; in case branch-taken, must restore URF and rename table
				memcpy(cpu->saved_state[cpu->cfid].unified_regs, cpu->unified_regs, NUM_UNIFIED_REGS * sizeof(ureg_t));
				memcpy(cpu->saved_state[cpu->cfid].front_rename_table, cpu->front_rename_table, NUM_ARCH_REGS * sizeof(int));

				// re-assign new cfid to this branch insn
				robe->cfid = cpu->cfid;	
			}
	
			rob->tail_ptr = (rob->tail_ptr + 1) % ROB_SIZE;	
		} // create ROB entry ; end
	
		// create IQ entry	
		int iq_idx = -1;	
		if(!is_halt(stage->opcode)) {
			iq_entry_t* iq = cpu->iq;
			// scan IQ and look for a free entry
			for(int i=0; i<IQ_SIZE; i++) {
				iq_entry_t* iqe = &iq[i];
				if(!iqe->taken) {
					iq_idx = i;
					iqe->taken = 1;
					iqe->cycle_dispatched = cpu->clock;
	
					iqe->pc = stage->pc; // just for printing
					iqe->rob_idx = rob_idx;			
					iqe->lsq_idx = lsq_idx;
	
					strcpy(iqe->opcode, stage->opcode);
					iqe->imm = stage->imm;
					
					iqe->u_rs1 = stage->u_rs1;
					iqe->u_rs1_ready = 0;
	
					iqe->u_rs2 = stage->u_rs2;
					iqe->u_rs2_ready = 0;
	
					// control-flow id
					iqe->cfid = cpu->cfid;	
	
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
	
					// check if any source registers are ready
					if(cpu->unified_regs[iqe->u_rs1].valid) {
						iqe->u_rs1_ready = 1;
						iqe->u_rs1_val = cpu->unified_regs[iqe->u_rs1].val;
					}
					if(cpu->unified_regs[iqe->u_rs2].valid) {
						iqe->u_rs2_ready = 1;
						if(strcmp(iqe->opcode, "STORE") == 0) {
							lsq_entry_t* lsqe = &cpu->lsq.entries[iqe->lsq_idx];
							lsqe->u_rs2_ready = 1;
							lsqe->u_rs2_val = cpu->unified_regs[iqe->u_rs2].val;	
						}
						iqe->u_rs2_val = cpu->unified_regs[iqe->u_rs2].val;
					}
				
					break;
				}
			} // create IQ entry ; end
			if(iq_idx < 0) {
				printf("IQ full... insert logic here...\n");
			}	
		} // !is_halt() ; end
	
		// update print info
		stage_t* p = &cpu->print_info[get_code_index(stage->pc)];
		p->rob_idx = rob_idx;
		p->iq_idx = iq_idx;
		p->lsq_idx = lsq_idx;
		p->cfid = cpu->cfid;
	}
	
	if(!is_valid_insn(stage->opcode)) update_print_stack("Dispatch", cpu, cpu->code_size + DP);
	else update_print_stack("Dispatch", cpu, get_code_index(stage->pc));
	
	return 0;
}

int issue(cpu_t* cpu) {
			
	// look for the earliest dispatched insn with all operands ready and send to FU	
	iq_entry_t* iq = cpu->iq;
	int earliest_cycle_intFU = INT_MAX;
	int earliest_intFU = INT_MAX;
	
	int earliest_cycle_mulFU = INT_MAX;
	int earliest_mulFU = INT_MAX;

	for(int i=0; i<IQ_SIZE; i++) {
		iq_entry_t* iqe = &iq[i];
		if(!iqe->taken)	continue;
	
		// check if any source registers are ready
		if(!iqe->u_rs1_ready && cpu->unified_regs[iqe->u_rs1].valid) {
			iqe->u_rs1_ready = 1;
			iqe->u_rs1_val = cpu->unified_regs[iqe->u_rs1].val;
		}
		if(!iqe->u_rs2_ready && cpu->unified_regs[iqe->u_rs2].valid) {
			iqe->u_rs2_ready = 1;
			iqe->u_rs2_val = cpu->unified_regs[iqe->u_rs2].val;
		}

		// search for the earliest ready instruction for each FU
	
		if(cpu->intFU.busy <= 0 && is_intFU(iqe->opcode)) { // if this FU is free and this insn goes to intFU
			if(iqe->u_rs1_ready && iqe->u_rs2_ready) {	
				if(iqe->cycle_dispatched < earliest_cycle_intFU) {
					earliest_intFU = i;
					earliest_cycle_intFU = iqe->cycle_dispatched; 
				}
			}
		}
		if(cpu->mulFU.busy <= 0 && (strcmp(iqe->opcode, "MUL") == 0)) {	
			if(iqe->u_rs1_ready && iqe->u_rs2_ready) {
				if(iqe->cycle_dispatched < earliest_cycle_mulFU) {
					earliest_mulFU = i;
					earliest_cycle_mulFU = iqe->cycle_dispatched; 
				}
			}
		}
	}	
	
	// issue for intFU
	// send ready insn to intFU
	if(earliest_intFU != INT_MAX) { // means intFU is free and found a ready insn
		iq_entry_t* iqe = &cpu->iq[earliest_intFU];
		iqe->taken = 0; // free this IQ entry
		
		// send this insn to intFU
		fu_t* intFU = &cpu->intFU;
		rob_entry_t* robe = &cpu->rob.entries[iqe->rob_idx];
		intFU->rob_idx = iqe->rob_idx;
		strcpy(intFU->opcode, iqe->opcode);
		intFU->u_rd = robe->u_rd; // target register
		intFU->cfid = robe->cfid; // control-flow id

		intFU->imm = iqe->imm;
		intFU->u_rs1_val = iqe->u_rs1_val;
		intFU->u_rs2_val = iqe->u_rs2_val;
			
		intFU->busy = INT_FU_LAT; // latency + issue latency

		// printing stuff
		intFU->print_idx = get_code_index(iqe->pc);
		//update_print_stack("Issue", cpu, intFU->print_idx);
	}

	// send ready insn to mulFU
	if(earliest_mulFU != INT_MAX) { // means mulFU is free and found a ready insn
		iq_entry_t* iqe = &cpu->iq[earliest_mulFU];
		iqe->taken = 0; // free this IQ entry
		
		// send this insn to mulFU
		fu_t* mulFU = &cpu->mulFU;
		rob_entry_t* robe = &cpu->rob.entries[iqe->rob_idx];
		mulFU->rob_idx = iqe->rob_idx;
		strcpy(mulFU->opcode, iqe->opcode);
		mulFU->u_rd = robe->u_rd; // target register
		mulFU->cfid = robe->cfid; // control-flow id

		mulFU->imm = iqe->imm;
		mulFU->u_rs1_val = iqe->u_rs1_val;
		mulFU->u_rs2_val = iqe->u_rs2_val;
			
		mulFU->busy = MUL_FU_LAT; // latency + issue latency

		// printing stuff
		mulFU->print_idx = get_code_index(iqe->pc);
		//update_print_stack("Issue", cpu, mulFU->print_idx);
	}
	
	return 0;
	
}

// broadcast calculated value to waiting insn in IQ
void broadcast(cpu_t* cpu, int u_rd, int u_rd_val) {

	iq_entry_t* iq = cpu->iq;
	for(int i=0; i<IQ_SIZE; i++) {
		iq_entry_t* iqe = &iq[i];
		if(iqe->taken) {
			if(u_rd == iqe->u_rs1) {
				iqe->u_rs1_ready = 1;
				iqe->u_rs1_val = u_rd_val;	
			}
			if(u_rd == iqe->u_rs2) {
				iqe->u_rs2_ready = 1;
				iqe->u_rs2_val = u_rd_val;	
			}
		} // if iqe->taken ; end
	}

	lsq_entry_t* lsq = cpu->lsq.entries;	
	for(int i=0; i<LSQ_SIZE; i++) {
		lsq_entry_t* lsqe = &lsq[i];
		if(lsqe->taken) {
			if(u_rd == lsqe->u_rs2) {
				lsqe->u_rs2_val = u_rd_val;
				lsqe->u_rs2_ready = 1;	
			}
		}
	}

}

int execute(cpu_t* cpu) {

	issue(cpu); // selects an instruction that is ready

	// check each FU

	// intFU	
	fu_t* intFU = &cpu->intFU;
	intFU->busy--;
	if(!intFU->busy) {

		rob_entry_t* robe = &cpu->rob.entries[intFU->rob_idx];	
		ureg_t* u_rd = &cpu->unified_regs[robe->u_rd];
		// perform computation
		if(strcmp(intFU->opcode, "LOAD") == 0 || strcmp(intFU->opcode, "STORE") == 0) { // memory address computation
			lsq_entry_t* lsqe = &cpu->lsq.entries[robe->lsq_idx];
			lsqe->mem_addr = intFU->u_rs1_val + intFU->imm;
			lsqe->mem_addr_valid = 1;
		} else { // arithmetic insn	
			if(strcmp(intFU->opcode, "MOVC") == 0) u_rd->val = intFU->imm + 0;	
			else if(strcmp(intFU->opcode, "ADD") == 0) u_rd->val = intFU->u_rs1_val + intFU->u_rs2_val;
			else if(strcmp(intFU->opcode, "SUB") == 0) u_rd->val = intFU->u_rs1_val - intFU->u_rs2_val;
			else if(strcmp(intFU->opcode, "AND") == 0) u_rd->val = intFU->u_rs1_val & intFU->u_rs2_val;
			else if(strcmp(intFU->opcode, "XOR") == 0) u_rd->val = intFU->u_rs1_val ^ intFU->u_rs2_val;	
			else if(strcmp(intFU->opcode, "ADDL") == 0) u_rd->val = intFU->u_rs1_val + intFU->imm;
			else if(strcmp(intFU->opcode, "SUBL") == 0) u_rd->val = intFU->u_rs1_val - intFU->imm;		
				
			else if(strcmp(intFU->opcode, "JUMP") == 0) {
				cpu->pc = intFU->u_rs1_val + intFU->imm;

				// jump is always taken ; restore URF and rename table	
				memcpy(cpu->unified_regs, cpu->saved_state[cpu->cfid].unified_regs, NUM_UNIFIED_REGS * sizeof(ureg_t));
				memcpy(cpu->front_rename_table, cpu->saved_state[cpu->cfid].front_rename_table, NUM_ARCH_REGS * sizeof(int));

				// search where this cfid starts in the cfq
				int temp_head_ptr = 0;
				for(int i=cpu->cfq_head_ptr; i<CFQ_SIZE; i++) {
					if(cpu->cfq[i] == intFU->cfid) {
						temp_head_ptr = i;
						break;
					}
				}

				// flush all instructions with cfids from temp_head_ptr to cfq_tail_ptr
				int new_tail_ptr = temp_head_ptr; // save the value
				while(temp_head_ptr != cpu->cfq_tail_ptr) {	
					// convert all insn with matching cfid to NOPs
					int cfid = cpu->cfq[temp_head_ptr];
					// search iq
					iq_entry_t* iq = cpu->iq;
					for(int i=0; i<IQ_SIZE; i++) {
						iq_entry_t* iqe = &iq[i];
						if(!iqe->taken) continue;
			
						if(iqe->cfid == cfid) {
							iqe->taken = 0;	// deallocate entry
						}	
					}
					// search rob
					rob_entry_t* rob = cpu->rob.entries;
					for(int i=0; i<ROB_SIZE; i++) {
						rob_entry_t* robe = &rob[i];
						if(robe->taken && robe->cfid == cfid) {
							strcpy(robe->opcode, "NOP");
							robe->valid = 1; // no need to wait for sources
						}
					}

					// search lsq
					lsq_entry_t* lsq = cpu->lsq.entries;
					for(int i=0; i<LSQ_SIZE; i++) {
						lsq_entry_t* lsqe = &lsq[i];
						if(lsqe->taken && lsqe->cfid == cfid) {
							strcpy(lsqe->opcode, "NOP");
							lsqe->done= 1; // no need to wait for sources
						}
					}

					// check FUs
					if(cpu->mulFU.cfid == cfid) {
						cpu->mulFU.busy = -1; // free resource
						strcpy(cpu->mulFU.opcode, "NOP");	
					}
					if(cpu->memFU.cfid == cfid) {
						cpu->memFU.busy = -1; // free resource
						strcpy(cpu->memFU.opcode, "NOP");	
					}
				
					// free this cfid
					cpu->cfid_freelist[cfid] = 0;	
					temp_head_ptr = (temp_head_ptr + 1) % CFQ_SIZE;
				
				}
				cpu->cfq_tail_ptr = new_tail_ptr;
	
				update_print_stack("Execute", cpu, intFU->print_idx);		
				robe->valid = 1;	
				return 0;
			} 

			if(u_rd->val == 0) u_rd->zero_flag = 1;
			u_rd->valid = 1;

			// broadcast ready value to IQ and LSQ
			broadcast(cpu, robe->u_rd, u_rd->val);
			robe->valid = 1;	
		}
				
		update_print_stack("Execute", cpu, intFU->print_idx);		
	} 
	// mulFU
	fu_t* mulFU = &cpu->mulFU;
	mulFU->busy--;
	if(!mulFU->busy) {
		rob_entry_t* robe = &cpu->rob.entries[mulFU->rob_idx];	
		
		ureg_t* u_rd = &cpu->unified_regs[robe->u_rd];
		u_rd->val = mulFU->u_rs1_val * mulFU->u_rs2_val; // the value is written directly to URF
		u_rd->valid = 1;
		
		// broadcast ready value to IQ
		broadcast(cpu, robe->u_rd, u_rd->val);
		robe->valid = 1;

		update_print_stack("Execute", cpu, mulFU->print_idx);		
	}	

	return 0;
}

int memory(cpu_t* cpu) {
	
	fu_t* memFU = &cpu->memFU;
	memFU->busy--;
	if(memFU->busy < 0) { // unit is free ; put a memory instruction here
		
		// check head of LSQ for ready instruction 
		int head_ptr = cpu->lsq.head_ptr;
		lsq_entry_t* lsqe = &cpu->lsq.entries[head_ptr];
		if(lsqe->done) return 0; // this memory instruction completed ; just needs to be commited

		head_ptr = cpu->rob.head_ptr;
		rob_entry_t* robe = &cpu->rob.entries[head_ptr];

		// check if source is ready (for store only)
		if(cpu->unified_regs[lsqe->u_rs2].valid) {
			lsqe->u_rs2_val = cpu->unified_regs[lsqe->u_rs2].val;
			lsqe->u_rs2_ready = 1;	
		}

		if(lsqe->taken && lsqe->mem_addr_valid && lsqe->pc == robe->pc) {
			char ready = 0;
			if(strcmp(lsqe->opcode, "LOAD") == 0) ready = 1;
			else if(strcmp(lsqe->opcode, "STORE") == 0 && lsqe->u_rs2_ready) ready = 1;

			if(ready) { // send to memFU	
				strcpy(memFU->opcode, lsqe->opcode);
				memFU->mem_addr = lsqe->mem_addr;
				memFU->u_rs2_val = lsqe->u_rs2_val;	// only used by stores
				memFU->rob_idx = lsqe->rob_idx;
				memFU->busy = MEM_FU_LAT - 1; // this cycle also counts toward the latency count, hence -1
				// print info
				memFU->print_idx = get_code_index(lsqe->pc);
				
				update_print_stack("Memory", cpu, memFU->print_idx);
			}
		}
	
	} else if(!memFU->busy) { // mem operations complete in this cycle

		int head_ptr = cpu->lsq.head_ptr;
		lsq_entry_t* lsqe = &cpu->lsq.entries[head_ptr];

		head_ptr = cpu->rob.head_ptr;
		rob_entry_t* robe = &cpu->rob.entries[head_ptr];
		ureg_t* u_rd = &cpu->unified_regs[robe->u_rd];
		assert(robe->pc == lsqe->pc);

		if(strcmp(memFU->opcode, "LOAD") == 0) {
			u_rd->val = cpu->memory[memFU->mem_addr];
			// broadcast ready value to IQ
			broadcast(cpu, robe->u_rd, u_rd->val);
		}
		else if(strcmp(memFU->opcode, "STORE") == 0) {
			cpu->memory[memFU->mem_addr] = memFU->u_rs2_val;
		}
		robe->valid = 1;		
		lsqe->done = 1;
	
		update_print_stack("Memory", cpu, memFU->print_idx);
	}

	return 0;
}

int commit(cpu_t* cpu) {
	
	for(int i=0; i<MAX_COMMIT_NUM; i++) {		
		int ptr = cpu->rob.head_ptr;
		// release ROB entry
		// get ROB entry at the head of ROB
		rob_entry_t* robe = &cpu->rob.entries[ptr];
		if(robe->valid) { // insn has wrote to URF 
			// set arch reg mapping to URF
			cpu->arch_regs[robe->rd].u_rd = robe->u_rd;
			
			// update backend rename table
			int old_u_rd = cpu->back_rename_table[robe->rd];
			if(old_u_rd != -1 && old_u_rd != robe->u_rd) cpu->unified_regs[old_u_rd].taken = 0; // free old mapping
			cpu->back_rename_table[robe->rd] = robe->u_rd;

			robe->taken = 0;
			//robe->valid = 0;
			//robe->pc = 0;
			//u_rd->val = 0;

			if(is_mem(robe->opcode)) {
				cpu->lsq.entries[cpu->lsq.head_ptr].taken = 0;
				cpu->lsq.head_ptr = (cpu->lsq.head_ptr + 1) % LSQ_SIZE; // update lsq head_ptr	
			}
			cpu->rob.head_ptr = (cpu->rob.head_ptr + 1) % ROB_SIZE; // update rob head_ptr		
		
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
		memory(cpu);
		execute(cpu);
		dispatch(cpu);
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
			if(cpu->done) printf("sim> No more instructions to simulate. Completed at %i cycles.\n", cpu->clock);
			break;
		}

	}
		
	return 0;
}
