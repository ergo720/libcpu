/*
 * libcpu: x86_arch.cpp
 *
 * init code
 */

#include <assert.h>

#include "libcpu.h"
#include "x86_isa.h"
#include "frontend.h"
#include "x86_internal.h"

static cpu_register_layout_t arch_x86_register_layout[] = {
	{ 0, 32, 0, 0, 0, "EAX" },
	{ 0, 32, 0, 0, 0, "ECX" },
	{ 0, 32, 0, 0, 0, "EDX" },
	{ 0, 32, 0, 0, 0, "EBX" },
	{ 0, 32, 0, 0, 0, "ESP" },
	{ 0, 32, 0, 0, 0, "EBP" },
	{ 0, 32, 0, 0, 0, "ESI" },
	{ 0, 32, 0, 0, 0, "EDI" },
	{ 0, 32, 0, 0, 0, "EFLAGS" },
	{ 0, 16, 0, 0, 0, "ES" },
	{ 0, 16, 0, 0, 0, "CS" },
	{ 0, 16, 0, 0, 0, "SS" },
	{ 0, 16, 0, 0, 0, "DS" },
	{ 0, 16, 0, 0, 0, "FS" },
	{ 0, 16, 0, 0, 0, "GS" },
	{ 0, 32, 0, 0, 0, "CR0" },
	{ 0, 32, 0, 0, 0, "CR1" },
	{ 0, 32, 0, 0, 0, "CR2" },
	{ 0, 32, 0, 0, 0, "CR3" },
	{ 0, 32, 0, 0, 0, "CR4" },
	{ 0, 32, 0, 0, 0, "DR0" },
	{ 0, 32, 0, 0, 0, "DR1" },
	{ 0, 32, 0, 0, 0, "DR2" },
	{ 0, 32, 0, 0, 0, "DR3" },
	{ 0, 32, 0, 0, 0, "DR4" },
	{ 0, 32, 0, 0, 0, "DR5" },
	{ 0, 32, 0, 0, 0, "DR6" },
	{ 0, 32, 0, 0, 0, "DR7" },
	{ 0, 80, 0, 0, 0, "ST0" },
	{ 0, 80, 0, 0, 0, "ST1" },
	{ 0, 80, 0, 0, 0, "ST2" },
	{ 0, 80, 0, 0, 0, "ST3" },
	{ 0, 80, 0, 0, 0, "ST4" },
	{ 0, 80, 0, 0, 0, "ST5" },
	{ 0, 80, 0, 0, 0, "ST6" },
	{ 0, 80, 0, 0, 0, "ST7" },
};

static cpu_flags_layout_t arch_x86_flags_layout[] = {
	{ ID_SHIFT, 0, "ID" }, /* cpuid supported */
	{ VIP_SHIFT, 0, "VIP" }, /* virtual interrupt pending */
	{ VIF_SHIFT, 0, "VIF" }, /* virtual interrupt flag */
	{ AC_SHIFT, 0, "AC" }, /* alignment check */
	{ VM_SHIFT, 0, "VM" }, /* virtual 8086 mode */
	{ RF_SHIFT, 0, "RF" }, /* resume flag */
	{ NT_SHIFT, 0, "NT" }, /* nested task flag */
	{ IOPLH_SHIFT, 0, "IOPLH" }, /* I/O privilege level (high bit) */
	{ IOPLL_SHIFT, 0, "IOPLL" }, /* I/O privilege level (low bit) */
	{ OF_SHIFT, CPU_FLAGTYPE_OVERFLOW, "OF" }, /* overflow flag */
	{ DF_SHIFT, 0, "DF" }, /* direction flag */
	{ IF_SHIFT, 0, "IF" }, /* interrupt enable flag */
	{ TF_SHIFT, 0, "TF" }, /* trap flag */
	{ SF_SHIFT, CPU_FLAGTYPE_NEGATIVE, "SF" }, /* sign flag */
	{ ZF_SHIFT, CPU_FLAGTYPE_ZERO, "ZF" }, /* zero flag */
	{ AF_SHIFT, 0, "AF" }, /* adjust flag */
	{ PF_SHIFT, CPU_FLAGTYPE_PARITY, "PF" }, /* parity flag */
	{ CF_SHIFT, CPU_FLAGTYPE_CARRY, "CF" }, /* carry flag */
	{ -1, 0, NULL }
};

static void
arch_x86_init(cpu_t *cpu, cpu_archinfo_t *info, cpu_archrf_t *rf)
{
	reg_x86_t *reg;
	fpr_x86_t *fp_reg;

	info->name = "X86";
	info->full_name = "Intel Pentium III";

	info->common_flags = CPU_FLAG_ENDIAN_LITTLE;

	info->byte_size = 8;
	info->word_size = 32;
	info->float_size = 80;
	info->address_size = 32;
	info->psr_size = 32;

	info->regclass_count[CPU_REGCLASS_GPR] = 8;
	info->regclass_count[CPU_REGCLASS_XR] = 20;
	info->regclass_count[CPU_REGCLASS_FPR] = 8;
	info->register_layout = arch_x86_register_layout;

	info->flags_count = 18;
	info->flags_layout = arch_x86_flags_layout;

	reg = (reg_x86_t *)calloc(1, sizeof(reg_x86_t));
	fp_reg = (fpr_x86_t *)calloc(1, sizeof(fpr_x86_t));
	assert(reg != NULL);
	assert(fp_reg != NULL);
	reg->eflags = 0x2U;

	rf->pc = &reg->eip;
	rf->grf = reg;
	rf->frf = fp_reg;
}

static void
arch_x86_done(cpu_t *cpu)
{
	free(cpu->rf.grf);
	free(cpu->rf.frf);
}

static void
arch_x86_emit_decode_reg(cpu_t *cpu, BasicBlock *bb)
{
}

static void
arch_x86_spill_reg_state(cpu_t *cpu, BasicBlock *bb)
{
}

static addr_t
arch_x86_get_pc(cpu_t *, void *reg)
{
	return ((reg_x86_t*)reg)->eip;
}

static uint64_t
arch_x86_get_psr(cpu_t *, void *reg)
{
	return ((reg_x86_t*)reg)->eflags;
}

static int
arch_x86_get_reg(cpu_t *cpu, void *reg, unsigned reg_no, uint64_t *value)
{
	if (reg_no > 7)
		return -1;

	*value = ((reg_x86_t*)reg)->gpr[reg_no];
	return 0;
}

arch_func_t arch_func_x86 = {
	arch_x86_init,
	arch_x86_done,
	arch_x86_get_pc,
	arch_x86_emit_decode_reg,
	arch_x86_spill_reg_state,
	arch_x86_tag_instr,
	arch_x86_disasm_instr,
	arch_x86_translate_cond,
	arch_x86_translate_instr,
	// idbg support
	arch_x86_get_psr,
	arch_x86_get_reg,
	NULL
};
