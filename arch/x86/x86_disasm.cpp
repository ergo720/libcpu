/*
 * libcpu: x86_disasm.cpp
 *
 * disassembler
 */

#include "libcpu.h"

extern int arch_x86_disasm_instr_att(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);
extern int arch_x86_disasm_instr_intel(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);

typedef int(*fp_x86_disasm_instr)(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);
fp_x86_disasm_instr arch_x86_disasm_func[] = {
	arch_x86_disasm_instr_att,
	arch_x86_disasm_instr_intel,
};

int
arch_x86_disasm_instr(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line)
{
	return (*arch_x86_disasm_func[(cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT])(cpu, pc, line, max_line);
}
