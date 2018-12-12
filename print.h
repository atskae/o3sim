#ifndef PRINT_H
#define PRINT_H

#include "cpu.h"

void print_insn(stage_t* stage, char rename);
void print_stage_content(char* name, stage_t* stage);
void print_rename_table(cpu_t* cpu);

void print_iq(iq_entry_t* iq);
void print_memory(cpu_t* cpu);
void print_lsq(lsq_t* lsq);
void print_rob(rob_t* rob);

void print_unified_regs(ureg_t* regs);
void print_arch_regs(areg_t* regs);
void print_all_FU(cpu_t* cpu);

void print_cpu(cpu_t* cpu);
void print_code(cpu_t* cpu);
void display(cpu_t* cpu);
void update_print_stack(char* name, cpu_t* cpu, int idx); // index into cpu->print_info

#endif // PRINT_H
