#include "libcpu.h"
#include "fapra_internal.h"
#include "fapra_interface.h"
#include "frontend.h"

static cpu_register_layout_t arch_fapra_register_layout[] = {
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
};

static libcpu_status
arch_fapra_init(cpu_t *cpu, cpu_archinfo_t *info, cpu_archrf_t *rf)
{
	// Basic Information
	info->name = "fapra";
	info->full_name = "FAPRA";

	// This architecture is little-endian.
	info->common_flags = CPU_FLAG_ENDIAN_LITTLE;
	// The byte size is 8bits.
	info->byte_size = 8;
	// The word size is 32bits.
	// The address size is 32bits.
	info->word_size = 32;
	info->address_size = 32;
	// Page size is 4K or 16M
	info->min_page_size = 4096;
	info->max_page_size = 16777216;
	info->default_page_size = 4096;
	// There are 32 32-bit GPRs 
	info->regclass_count[CPU_REGCLASS_GPR] = 32;
	info->register_layout = arch_fapra_register_layout;

	reg_fapra32_t *reg;
	reg = (reg_fapra32_t *) malloc(sizeof(reg_fapra32_t));
	if (reg == nullptr) {
		return LIBCPU_NO_MEMORY;
	}
	for (int i = 0; i < 32; i++)
		reg->r[i] = 0;
	reg->pc = 0;

	cpu->rf.pc = &reg->pc;
	cpu->rf.grf = reg;

	LOG("%d bit FAPRA initialized.\n", info->word_size);

	return LIBCPU_SUCCESS;
}

static void
arch_fapra_done(cpu_t *cpu)
{
	free(cpu->rf.grf);
}

static addr_t
arch_fapra_get_pc(cpu_t *cpu, void *reg)
{
	return ((reg_fapra32_t*)reg)->pc;
}

static uint64_t
arch_fapra_get_psr(cpu_t *, void *)
{
	return 0;
}

static int
arch_fapra_get_reg(cpu_t *cpu, void *reg, unsigned reg_no, uint64_t *value)
{
	if (reg_no > 31)
		return (-1);

	*value = ((reg_fapra32_t*)reg)->r[reg_no];

	return (0);
}

arch_func_t arch_func_fapra = {
	arch_fapra_init,
	arch_fapra_done,
	arch_fapra_get_pc,
	nullptr, /* emit_decode_reg */
	nullptr, /* spill_reg_state */
	arch_fapra_tag_instr,
	arch_fapra_disasm_instr,
	arch_fapra_translate_cond,
	arch_fapra_translate_instr,
	// idbg support
	arch_fapra_get_psr,
	arch_fapra_get_reg,
	nullptr
};
