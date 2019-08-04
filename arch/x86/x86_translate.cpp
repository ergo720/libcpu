/*
 * libcpu: x86_translate.cpp
 *
 * main translation code
 */

#include "libcpu.h"
#include "x86_isa.h"
#include "x86_cc.h"
#include "frontend.h"
#include "x86_decode.h"

const char *mnemo[] = {
#define DECLARE_INSTR(name,str) str,
#include "x86_instr.h"
#undef DECLARE_INSTR
};

Value*
arch_8086_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb)
{
	return NULL;
}

int
arch_x86_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb)
{
	struct x86_instr instr;

	if (arch_x86_decode_instr(&instr, cpu->RAM, pc, (cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT) != 0)
		return -1;

	switch (instr.type) {
	default:
		fprintf(stderr, "error: frontend does not implement %s instruction yet\n", mnemo[instr.type]);
		exit(1);
	}

	return 0;
}
