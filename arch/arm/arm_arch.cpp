/*
 * libcpu: arm_arch.cpp
 *
 * init code
 */

#include <assert.h>

#include "libcpu.h"
#include "arm_types.h"
#include "arm_internal.h"
#include "frontend.h"

static cpu_register_layout_t arch_arm_register_layout[] = {
	{ 0, 32, 0, 0, 0, "R0" },
	{ 0, 32, 0, 0, 0, "R1" },
	{ 0, 32, 0, 0, 0, "R2" },
	{ 0, 32, 0, 0, 0, "R3" },
	{ 0, 32, 0, 0, 0, "R4" },
	{ 0, 32, 0, 0, 0, "R5" },
	{ 0, 32, 0, 0, 0, "R6" },
	{ 0, 32, 0, 0, 0, "R7" },
	{ 0, 32, 0, 0, 0, "R8" },
	{ 0, 32, 0, 0, 0, "R9" },
	{ 0, 32, 0, 0, 0, "R10" },
	{ 0, 32, 0, 0, 0, "R11" },
	{ 0, 32, 0, 0, 0, "R12" },
	{ 0, 32, 0, 0, 0, "R13" },
	{ 0, 32, 0, 0, 0, "R14" },
	{ 0, 32, 0, 0, 0, "R15" },
	{ 0, 32, 0, 0, 0, "CPSR" },
};

static void
arch_arm_init(cpu_t *cpu, cpu_archinfo_t *info, cpu_archrf_t *rf)
{
	// Basic Information
	info->name = "arm";
	info->full_name = "ARM v6";

	// This architecture is biendian, accept whatever the
	// client wants, override other flags.
	info->common_flags &= CPU_FLAG_ENDIAN_MASK;

	info->delay_slots = 0;
	// The byte size is 8bits.
	// The word size is 32bits.
	// The float size is 64bits.
	// The address size is 32bits.
	info->byte_size = 8;
	info->word_size = 32;
	info->float_size = 64;
	info->address_size = 32;
	// There are 16 32-bit GPRs
	info->regclass_count[CPU_REGCLASS_GPR] = 16;
	// There is also 1 extra register to handle PSR.
	info->regclass_count[CPU_REGCLASS_XR] = 1;
	info->register_layout = arch_arm_register_layout;

	reg_arm_t *reg;
	reg = (reg_arm_t*)malloc(sizeof(reg_arm_t));
	for (int i=0; i<17; i++) /* this includes pc */
		reg->r[i] = 0;

	cpu->rf.pc = &reg->r[15];
	cpu->rf.grf = reg;

	// allocate space for CC flags.
	cpu->feptr = malloc(sizeof(ccarm_t));
	assert(cpu->feptr != NULL);
}

static void
arch_arm_done(cpu_t *cpu)
{
	free(cpu->feptr);
	free(cpu->rf.grf);
}

static addr_t
arch_arm_get_pc(cpu_t *, void *reg)
{
	return ((reg_arm_t *)reg)->pc;
}

static uint64_t
arch_arm_get_psr(cpu_t *, void *reg)
{
	return ((reg_arm_t *)reg)->cpsr;
}

static int
arch_arm_get_reg(cpu_t *cpu, void *reg, unsigned reg_no, uint64_t *value)
{
	if (reg_no > 15)
		return (-1);

	*value = ((reg_arm_t *)reg)->r[reg_no];
	return (0);
}

arch_func_t arch_func_arm = {
	arch_arm_init,
	arch_arm_done,
	arch_arm_get_pc,
	arch_arm_emit_decode_reg,
	arch_arm_spill_reg_state,
	arch_arm_tag_instr,
	arch_arm_disasm_instr,
	arch_arm_translate_cond,
	arch_arm_translate_instr,
	// idbg support
	arch_arm_get_psr,
	arch_arm_get_reg,
	NULL
};
