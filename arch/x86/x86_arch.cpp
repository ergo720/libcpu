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

	info->register_count[CPU_REG_GPR] = 8;
	info->register_size[CPU_REG_GPR] = info->word_size;

	info->register_count[CPU_REG_FPR] = 8;
	info->register_size[CPU_REG_FPR] = info->float_size;

	info->register_count[CPU_REG_XR] = 1;
	info->register_size[CPU_REG_XR] = info->word_size;

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
	switch (reg_no) {
	case 0:
		*value = ((reg_x86_t*)reg)->eax;
		break;
	case 1:
		*value = ((reg_x86_t*)reg)->ecx;
		break;
	case 2:
		*value = ((reg_x86_t*)reg)->edx;
		break;
	case 3:
		*value = ((reg_x86_t*)reg)->ebx;
		break;
	case 4:
		*value = ((reg_x86_t*)reg)->esp;
		break;
	case 5:
		*value = ((reg_x86_t*)reg)->ebp;
		break;
	case 6:
		*value = ((reg_x86_t*)reg)->esi;
		break;
	case 7:
		*value = ((reg_x86_t*)reg)->edi;
		break;
	default:
		return -1;
	}
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
