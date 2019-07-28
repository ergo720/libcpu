/*
 * libcpu: x86_translate.cpp
 *
 * main translation code
 */

#include "libcpu.h"
#include "x86_isa.h"
#include "x86_cc.h"
#include "frontend.h"
#include "x86_internal.h"
#include <assert.h>

Value *
arch_8086_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb)
{
	return NULL;
}

static int
arch_8086_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb)
{
	return 0;
}

static void
arch_x86_init(cpu_t *cpu, cpu_archinfo_t *info, cpu_archrf_t *rf)
{
	info->name = "X86";
	info->full_name = "Intel Pentium III";

	info->common_flags = CPU_FLAG_ENDIAN_LITTLE;

	info->byte_size		= 8;
	info->word_size		= 32;
	info->float_size	= 80;
	info->address_size	= 32;

	info->register_count[CPU_REG_GPR]	= 8;
	info->register_size[CPU_REG_GPR]	= info->word_size;

	info->register_count[CPU_REG_FPR] = 8;
	info->register_size[CPU_REG_FPR] = info->float_size;

	info->register_count[CPU_REG_XR]	= 0;
	info->register_size[CPU_REG_XR]		= 0;

	reg_x86_t *reg;
	reg = (reg_x86_t*)calloc(1, sizeof(reg_x86_t));
	assert(reg != NULL);

	rf->pc	= &reg->eip;
	rf->grf	= reg;
}

static void
arch_8086_done(cpu_t *cpu)
{
	free(cpu->feptr);
	free(cpu->rf.grf);
}

static void
arch_8086_emit_decode_reg(cpu_t *cpu, BasicBlock *bb)
{
}

static void
arch_8086_spill_reg_state(cpu_t *cpu, BasicBlock *bb)
{
}

static addr_t
arch_8086_get_pc(cpu_t *, void *reg)
{
	return ((reg_x86_t*)reg)->eip;
}

static uint64_t
arch_8086_get_psr(cpu_t *, void *reg)
{
	return 0xdeadbeefdeadbeefULL;
}

static int
arch_8086_get_reg(cpu_t *cpu, void *reg, unsigned reg_no, uint64_t *value)
{
	return -1;
}

arch_func_t arch_func_x86 = {
	arch_x86_init,
	arch_8086_done,
	arch_8086_get_pc,
	arch_8086_emit_decode_reg,
	arch_8086_spill_reg_state,
	arch_x86_tag_instr,
	arch_x86_disasm_instr,
	arch_8086_translate_cond,
	arch_8086_translate_instr,
	// idbg support
	arch_8086_get_psr,
	arch_8086_get_reg,
	NULL
};
