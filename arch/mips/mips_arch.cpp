#include "libcpu.h"
#include "mips_internal.h"
#include "mips_interface.h"
#include "frontend.h"

#if CPU_MIPS_IS_64BIT
static cpu_register_layout_t arch_mips_register_layout[] = {
	{ 0, 64, 0, 0, 0, "R0" },
	{ 0, 64, 0, 0, 0, "R1" },
	{ 0, 64, 0, 0, 0, "R2" },
	{ 0, 64, 0, 0, 0, "R3" },
	{ 0, 64, 0, 0, 0, "R4" },
	{ 0, 64, 0, 0, 0, "R5" },
	{ 0, 64, 0, 0, 0, "R6" },
	{ 0, 64, 0, 0, 0, "R7" },
	{ 0, 64, 0, 0, 0, "R8" },
	{ 0, 64, 0, 0, 0, "R9" },
	{ 0, 64, 0, 0, 0, "R10" },
	{ 0, 64, 0, 0, 0, "R11" },
	{ 0, 64, 0, 0, 0, "R12" },
	{ 0, 64, 0, 0, 0, "R13" },
	{ 0, 64, 0, 0, 0, "R14" },
	{ 0, 64, 0, 0, 0, "R15" },
	{ 0, 64, 0, 0, 0, "R16" },
	{ 0, 64, 0, 0, 0, "R17" },
	{ 0, 64, 0, 0, 0, "R18" },
	{ 0, 64, 0, 0, 0, "R19" },
	{ 0, 64, 0, 0, 0, "R20" },
	{ 0, 64, 0, 0, 0, "R21" },
	{ 0, 64, 0, 0, 0, "R22" },
	{ 0, 64, 0, 0, 0, "R23" },
	{ 0, 64, 0, 0, 0, "R24" },
	{ 0, 64, 0, 0, 0, "R25" },
	{ 0, 64, 0, 0, 0, "R26" },
	{ 0, 64, 0, 0, 0, "R27" },
	{ 0, 64, 0, 0, 0, "R28" },
	{ 0, 64, 0, 0, 0, "R29" },
	{ 0, 64, 0, 0, 0, "R30" },
	{ 0, 64, 0, 0, 0, "R31" },
	{ 0, 64, 0, 0, 0, "HI" },
	{ 0, 64, 0, 0, 0, "LO" },
};
#else
static cpu_register_layout_t arch_mips_register_layout[] = {
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
	{ 0, 32, 0, 0, 0, "HI" },
	{ 0, 32, 0, 0, 0, "LO" },
};
#endif

static libcpu_status
arch_mips_init(cpu_t *cpu, cpu_archinfo_t *info, cpu_archrf_t *rf)
{
	// Basic Information
	info->name = "mips";
	info->full_name = "MIPS R4000";

	// This architecture is biendian, accept whatever the
	// client wants, override other flags.
	info->common_flags &= CPU_FLAG_ENDIAN_MASK;
	// Both r0 and x0 are hardwired to zero.
	info->common_flags |= CPU_FLAG_HARDWIRE_GPR0;
	info->common_flags |= CPU_FLAG_HARDWIRE_FPR0;
	// The byte size is 8bits.
	// The float size is 64bits.
	info->byte_size = 8;
	info->float_size = 80;
	if (info->arch_flags & CPU_MIPS_IS_64BIT) {
  		// The word size is 64bits.
		// The address size is 64bits.
		info->word_size = 64;
		info->address_size = 64; //XXX actually it's 32!
	} else {
  		// The word size is 32bits.
		// The address size is 32bits.
		info->word_size = 32;
		info->address_size = 32;
	}
	// Page size is 4K or 16M
	info->min_page_size = 4096;
	info->max_page_size = 16777216;
	info->default_page_size = 4096;
	// There are 32 32-bit GPRs 
	info->regclass_count[CPU_REGCLASS_GPR] = 32;
	// There are 2 extra registers, HI/LO for MUL/DIV insn.
	info->regclass_count[CPU_REGCLASS_XR] = 2;
	info->register_layout = arch_mips_register_layout;

	if (info->arch_flags & CPU_MIPS_IS_64BIT) {
		reg_mips64_t *reg;
		reg = (reg_mips64_t*)malloc(sizeof(reg_mips64_t));
		if (reg == nullptr) {
			return LIBCPU_NO_MEMORY;
		}
		for (int i=0; i<32; i++) 
			reg->r[i] = 0;
		reg->pc = 0;

		cpu->rf.pc = &reg->pc;
		cpu->rf.grf = reg;
	} else {
		reg_mips32_t *reg;
		reg = (reg_mips32_t*)malloc(sizeof(reg_mips32_t));
		if (reg == nullptr) {
			return LIBCPU_NO_MEMORY;
		}
		for (int i=0; i<32; i++) 
			reg->r[i] = 0;
		reg->pc = 0;

		cpu->rf.pc = &reg->pc;
		cpu->rf.grf = reg;
	}

	LOG("%d bit MIPS initialized.\n", info->word_size);

	return LIBCPU_SUCCESS;
}

static void
arch_mips_done(cpu_t *cpu)
{
	free(cpu->rf.grf);
}

static addr_t
arch_mips_get_pc(cpu_t *cpu, void *reg)
{
	if (cpu->info.arch_flags & CPU_MIPS_IS_64BIT)
		return ((reg_mips64_t*)reg)->pc;
	else
		return ((reg_mips32_t*)reg)->pc;
}

static uint64_t
arch_mips_get_psr(cpu_t *, void *)
{
	return 0;
}

static int
arch_mips_get_reg(cpu_t *cpu, void *reg, unsigned reg_no, uint64_t *value)
{
	if (reg_no > 31)
		return (-1);

	if (cpu->info.arch_flags & CPU_MIPS_IS_64BIT)
		*value = ((reg_mips64_t*)reg)->r[reg_no];
	else
		*value = ((reg_mips32_t*)reg)->r[reg_no];
	return (0);
}

arch_func_t arch_func_mips = {
	arch_mips_init,
	arch_mips_done,
	arch_mips_get_pc,
	nullptr, /* emit_decode_reg */
	nullptr, /* spill_reg_state */
	arch_mips_tag_instr,
	arch_mips_disasm_instr,
	arch_mips_translate_cond,
	arch_mips_translate_instr,
	// idbg support
	arch_mips_get_psr,
	arch_mips_get_reg,
	nullptr
};
