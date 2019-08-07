#include "llvm/IR/Instructions.h"

#include "libcpu.h"
#include "libcpu_llvm.h"
#include "m88k_internal.h"
#include "frontend.h"
#include "m88k_types.h"

#define ptr_PSR		ptr_xr[0]
#define ptr_TRAPNO	ptr_xr[1]
#define ptr_C		(cpu->feptr)

static cpu_register_layout_t arch_m88k_reg_layout[] = {
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
	{ 0, 32, 0, 0, 0, "R16" },
	{ 0, 32, 0, 0, 0, "R17" },
	{ 0, 32, 0, 0, 0, "R18" },
	{ 0, 32, 0, 0, 0, "R19" },
	{ 0, 32, 0, 0, 0, "R20" },
	{ 0, 32, 0, 0, 0, "R21" },
	{ 0, 32, 0, 0, 0, "R22" },
	{ 0, 32, 0, 0, 0, "R23" },
	{ 0, 32, 0, 0, 0, "R24" },
	{ 0, 32, 0, 0, 0, "R25" },
	{ 0, 32, 0, 0, 0, "R26" },
	{ 0, 32, 0, 0, 0, "R27" },
	{ 0, 32, 0, 0, 0, "R28" },
	{ 0, 32, 0, 0, 0, "R29" },
	{ 0, 32, 0, 0, 0, "R30" },
	{ 0, 32, 0, 0, 0, "R31" },
	{ 0, 32, 0, 0, 0, "PSR" },
	{ 0, 32, 0, 0, 0, "TRAPNO" },
	{ 0, 80, 0, 0, 0, "X0" },
	{ 0, 80, 0, 0, 0, "X1" },
	{ 0, 80, 0, 0, 0, "X2" },
	{ 0, 80, 0, 0, 0, "X3" },
	{ 0, 80, 0, 0, 0, "X4" },
	{ 0, 80, 0, 0, 0, "X5" },
	{ 0, 80, 0, 0, 0, "X6" },
	{ 0, 80, 0, 0, 0, "X7" },
	{ 0, 80, 0, 0, 0, "X8" },
	{ 0, 80, 0, 0, 0, "X9" },
	{ 0, 80, 0, 0, 0, "X10" },
	{ 0, 80, 0, 0, 0, "X11" },
	{ 0, 80, 0, 0, 0, "X12" },
	{ 0, 80, 0, 0, 0, "X13" },
	{ 0, 80, 0, 0, 0, "X14" },
	{ 0, 80, 0, 0, 0, "X15" },
	{ 0, 80, 0, 0, 0, "X16" },
	{ 0, 80, 0, 0, 0, "X17" },
	{ 0, 80, 0, 0, 0, "X18" },
	{ 0, 80, 0, 0, 0, "X19" },
	{ 0, 80, 0, 0, 0, "X20" },
	{ 0, 80, 0, 0, 0, "X21" },
	{ 0, 80, 0, 0, 0, "X22" },
	{ 0, 80, 0, 0, 0, "X23" },
	{ 0, 80, 0, 0, 0, "X24" },
	{ 0, 80, 0, 0, 0, "X25" },
	{ 0, 80, 0, 0, 0, "X26" },
	{ 0, 80, 0, 0, 0, "X27" },
	{ 0, 80, 0, 0, 0, "X28" },
	{ 0, 80, 0, 0, 0, "X29" },
	{ 0, 80, 0, 0, 0, "X30" },
	{ 0, 80, 0, 0, 0, "X31" },
};

static void
arch_m88k_init(cpu_t *cpu, cpu_archinfo_t *info, cpu_archrf_t *rf)
{
	m88k_grf_t *reg;
	m88k_xrf_t *fp_reg;

	// Basic Information
	info->name = "m88k";
	info->full_name = "Motorola 88110";

	// This architecture is biendian, accept whatever the
	// client wants, override other flags.
	info->common_flags &= CPU_FLAG_ENDIAN_MASK;
	// Both r0 and x0 are hardwired to zero.
	info->common_flags |= CPU_FLAG_HARDWIRE_GPR0;
	info->common_flags |= CPU_FLAG_HARDWIRE_FPR0;
	// This architecture supports delay slots (w/o annihilation)
	// with 1 instruction.
	info->common_flags |= CPU_FLAG_DELAY_SLOT;
	info->delay_slots = 1;
	// The byte size is 8bits.
	// The word size is 32bits.
	// The float size is 80bits.
	// The address size is 32bits.
	info->byte_size = 8;
	info->word_size = 32;
	info->float_size = 80;
	info->address_size = 32;
	// Page size is just 4K.
	info->min_page_size = 4096;
	info->max_page_size = 4096;
	info->default_page_size = 4096;
	// There are 32 32-bit GPRs and 32 80-bit FPRs.
	info->register_count[CPU_REG_GPR] = 32;
	info->register_count[CPU_REG_FPR] = 32;
	// There are also 2 extra registers to handle
	// PSR and TRAPNO.
	info->register_count[CPU_REG_XR] = 2;
	info->register_layout = arch_m88k_reg_layout;

	// Setup the register files
	reg = (m88k_grf_t *)malloc(sizeof(m88k_grf_t));
	fp_reg = (m88k_xrf_t *)malloc(sizeof(m88k_xrf_t));
	for (int i = 0; i < 32; i++) {
		reg->r[i] = 0;
		fp_reg->x[i].i.hi = 0;
		fp_reg->x[i].i.lo = 0;
	}

	reg->sxip = 0;
	reg->psr = 0;
	reg->trapno = 0;

	// Architecture Register File
	rf->pc = &reg->sxip;
	rf->grf = reg;
	rf->frf = fp_reg;
	rf->vrf = NULL;

	LOG("Motorola 88110 initialized.\n");
}

static void
arch_m88k_done(cpu_t *cpu)
{
	free(cpu->rf.frf);
	free(cpu->rf.grf);
}

static addr_t
arch_m88k_get_pc(cpu_t *, void *reg)
{
	return ((m88k_grf_t *)reg)->sxip;
}

#define C_SHIFT 28

static Value *
arch_m88k_flags_encode(cpu_t *cpu, BasicBlock *bb)
{
	Value *flags = ConstantInt::get(getIntegerType(32), 0);

	flags = arch_encode_bit(flags, (Value *)ptr_C, C_SHIFT, 32, bb);

	return flags;
}

static void
arch_m88k_flags_decode(cpu_t *cpu, Value *flags, BasicBlock *bb)
{
	arch_decode_bit(flags, (Value *)ptr_C, C_SHIFT, 32, bb);
}

static void
arch_m88k_emit_decode_reg(cpu_t *cpu, BasicBlock *bb)
{
	// declare flags
	ptr_C = new AllocaInst(getIntegerType(1), "C", bb);

	// decode PSR
	Value *flags = new LoadInst(cpu->ptr_PSR, "", false, bb);
	arch_m88k_flags_decode(cpu, flags, bb);
}

static void
arch_m88k_spill_reg_state(cpu_t *cpu, BasicBlock *bb)
{
	Value *flags = arch_m88k_flags_encode(cpu, bb);
	new StoreInst(flags, cpu->ptr_PSR, false, bb);
}

static uint64_t
arch_m88k_get_psr(cpu_t *, void *regs)
{
	return ((m88k_grf_t *)regs)->psr;
}

static int
arch_m88k_get_reg(cpu_t *cpu, void *regs, unsigned reg_no, uint64_t *value)
{
	if (reg_no > 31)
		return (-1);

	*value = ((m88k_grf_t *)regs)->r[reg_no];
	return (0);
}

static int
arch_m88k_get_fp_reg(cpu_t *cpu, void *regs, unsigned reg_no, void *value)
{
	if (reg_no > 31)
		return (-1);

	*(fp80_reg_t *)value = ((m88k_xrf_t *)regs)->x[reg_no];
	return (0);
}

arch_func_t arch_func_m88k = {
	arch_m88k_init,
	arch_m88k_done,
	arch_m88k_get_pc,
	arch_m88k_emit_decode_reg,
	arch_m88k_spill_reg_state,
	arch_m88k_tag_instr,
	arch_m88k_disasm_instr,
	arch_m88k_translate_cond,
	arch_m88k_translate_instr,
	// idbg support
	arch_m88k_get_psr,
	arch_m88k_get_reg,
	arch_m88k_get_fp_reg
};
