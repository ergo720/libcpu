/*
 * libcpu: x86_disasm.cpp
 *
 * disassembler (att syntax)
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "libcpu.h"
#include "x86_isa.h"
#include "x86_decode.h"

static const char *mnemo[] = {
#define DECLARE_INSTR(name,str) str,
#include "x86_instr.h"
#undef DECLARE_INSTR
};

static const char *to_mnemonic(struct x86_instr *instr)
{
	return mnemo[instr->type];
}

static const char *byte_reg_names[] = {
	"%al",
	"%cl",
	"%dl",
	"%bl",
	"%ah",
	"%ch",
	"%dh",
	"%bh",
};

static const char *word_reg_names[] = {
	"%ax",
	"%cx",
	"%dx",
	"%bx",
	"%sp",
	"%bp",
	"%si",
	"%di",
};

static const char *full_reg_names[] = {
	"%eax",
	"%ecx",
	"%edx",
	"%ebx",
	"%esp",
	"%ebp",
	"%esi",
	"%edi",
};

static const char *seg_reg_names[] = {
	"%es",
	"%cs",
	"%ss",
	"%ds",
	"%fs",
	"%gs",
};

static const char *cr_reg_names[] = {
	"%cr0",
	"",
	"%cr2",
	"%cr3",
	"%cr4",
	"",
	"",
	"",
};

static const char *dbg_reg_names[] = {
	"%db0",
	"%db1",
	"%db2",
	"%db3",
	"",
	"",
	"%db6",
	"%db7",
};

static const char *mem_reg_names_16[] = {
	"%bx,%si",
	"%bx,%di",
	"%bp,%si",
	"%bp,%di",
	"%si",
	"%di",
	"%bp",
	"%bx",
};

static const char *mem_reg_names_32[] = {
	"%eax",
	"%ecx",
	"%edx",
	"%ebx",
	"",
	"%ebp",
	"%esi",
	"%edi",
};

static const char *sib_reg_base_names[] = {
	"%eax",
	"%ecx",
	"%edx",
	"%ebx",
	"%esp",
	"",
	"%esi",
	"%edi",
};

static const char *sib_reg_idx_names[] = {
	"%eax",
	"%ecx",
	"%edx",
	"%ebx",
	"%eiz",
	"%ebp",
	"%esi",
	"%edi",
};

static const char *sib_scale_names[] = {
	"1",
	"2",
	"4",
	"8",
};

static const char *seg_override_names[] = {
	"",
	"%es:",
	"%cs:",
	"%ss:",
	"%ds:",
	"%fs:",
	"%gs:"
};

static const char *lock_names[] = {
	"",
	"lock ",
};

static const char *prefix_names[] = {
	"",
	"repnz ",
	"rep ",
};

static const char *sign_to_str(int n)
{
	if (n >= 0)
		return "";

	return "-";
}

static const char *to_reg_name(struct x86_instr *instr, unsigned int reg_num)
{
	if (instr->flags & WIDTH_BYTE)
		return byte_reg_names[reg_num];

	if (instr->flags & WIDTH_WORD)
		return word_reg_names[reg_num];

	return full_reg_names[reg_num];
}

static const char *to_mem_reg_name(struct x86_instr *instr, unsigned int reg_num)
{
	if (instr->addr_size_override)
		return mem_reg_names_16[reg_num];

	return mem_reg_names_32[reg_num];
}

static const char *to_sib_base_name(struct x86_instr *instr)
{
	if (instr->mod == 0 && instr->base == 5)
		return "";

	return sib_reg_base_names[instr->base];
}

static const char *to_seg_reg_name(struct x86_instr *instr, unsigned int reg_num)
{
	return seg_reg_names[reg_num];
}

static const char *to_cr_reg_name(struct x86_instr *instr, unsigned int reg_num)
{
	return cr_reg_names[reg_num];
}

static const char *to_dbg_reg_name(struct x86_instr *instr, unsigned int reg_num)
{
	return dbg_reg_names[reg_num];
}

static bool check_suffix_override(struct x86_instr *instr)
{
	switch (instr->type) {
	case INSTR_ARPL:
	case INSTR_RET:
	case INSTR_ENTER:
	case INSTR_LRET:
	case INSTR_INT:
	case INSTR_AAM:
	case INSTR_AAD:
	case INSTR_LOOPNE:
	case INSTR_LOOPE:
	case INSTR_LOOP:
	case INSTR_JECXZ:
		if (instr->is_two_byte_instr == 0)
			return true;
		break;
	case INSTR_JO:
	case INSTR_JNO:
	case INSTR_JB:
	case INSTR_JNB:
	case INSTR_JZ:
	case INSTR_JNE:
	case INSTR_JBE:
	case INSTR_JA:
	case INSTR_JS:
	case INSTR_JNS:
	case INSTR_JPE:
	case INSTR_JPO:
	case INSTR_JL:
	case INSTR_JGE:
	case INSTR_JLE:
	case INSTR_JG:
		return true;
	case INSTR_LMSW:
	case INSTR_INVLPG:
	case INSTR_LLDT:
	case INSTR_LTR:
	case INSTR_VERR:
	case INSTR_VERW:
	case INSTR_SETO:
	case INSTR_SETNO:
	case INSTR_SETB:
	case INSTR_SETNB:
	case INSTR_SETZ:
	case INSTR_SETNE:
	case INSTR_SETBE:
	case INSTR_SETA:
	case INSTR_SETS:
	case INSTR_SETNS:
	case INSTR_SETPE:
	case INSTR_SETPO:
	case INSTR_SETL:
	case INSTR_SETGE:
	case INSTR_SETLE:
	case INSTR_SETG:
	case INSTR_CMPXCHG8B:
	case INSTR_BSWAP:
		if (instr->is_two_byte_instr == 1)
			return true;
		break;
	default:
		switch (instr->opcode) {
		case 0xEB: // jmp
			if (instr->is_two_byte_instr == 0)
				return true;
		default:
			break;
		}
	}

	return false;
}

static const char *add_instr_suffix(struct x86_instr *instr)
{
	if (check_suffix_override(instr) ||
		!(instr->flags & (WIDTH_BYTE | WIDTH_WORD | WIDTH_DWORD | WIDTH_QWORD)))
		return "";

	if (instr->flags & WIDTH_BYTE)
		return "b";

	if (instr->flags & WIDTH_WORD)
		return "w";

	if (instr->flags & WIDTH_DWORD)
		return "l";

	return "q";
}

static int
print_operand(addr_t pc, char *operands, size_t size, struct x86_instr *instr, struct x86_operand *operand)
{
	int ret = 0;

	switch (operand->type) {
	case OPTYPE_IMM:
		ret = snprintf(operands, size, "$0x%x", operand->imm);
		break;
	case OPTYPE_FAR_PTR:
		ret = snprintf(operands, size, "$0x%x,$0x%x", operand->seg_sel, operand->imm);
		break;
	case OPTYPE_REL:
		ret = snprintf(operands, size, "0x%x", (unsigned int)((long)pc + instr->nr_bytes + operand->rel));
		break;
	case OPTYPE_REG:
		ret = snprintf(operands, size, "%s", to_reg_name(instr, operand->reg));
		break;
	case OPTYPE_REG8:
		ret = snprintf(operands, size, "%s", byte_reg_names[operand->reg]);
		break;
	case OPTYPE_SEG_REG:
		ret = snprintf(operands, size, "%s", to_seg_reg_name(instr, operand->reg));
		break;
	case OPTYPE_CR_REG:
		ret = snprintf(operands, size, "%s", to_cr_reg_name(instr, operand->reg));
		break;
	case OPTYPE_DBG_REG:
		ret = snprintf(operands, size, "%s", to_dbg_reg_name(instr, operand->reg));
		break;
	case OPTYPE_MOFFSET:
		ret = snprintf(operands, size, "%s0x%x", sign_to_str(operand->disp), abs(operand->disp));
		break;
	case OPTYPE_MEM:
		ret = snprintf(operands, size, "%s(%s)", seg_override_names[instr->seg_override], to_mem_reg_name(instr, operand->reg));
		break;
	case OPTYPE_MEM_DISP:
		ret = snprintf(operands, size, "%s%s0x%x", seg_override_names[instr->seg_override], sign_to_str(operand->disp), abs(operand->disp));
		switch ((instr->addr_size_override << 16) | (instr->mod << 8) | instr->rm) {
		case 5:     // 0, 0, 5
		case 65542: // 1, 0, 6
			break;
		default:
			ret += snprintf(operands+ret, size-ret, "(%s)", to_mem_reg_name(instr, operand->reg));
		}
		break;
	case OPTYPE_SIB_MEM:
		ret = snprintf(operands, size, "(%s,%s,%s)", sib_reg_base_names[instr->base], sib_reg_idx_names[instr->idx], sib_scale_names[instr->scale]);
		break;
	case OPTYPE_SIB_DISP:
		ret = snprintf(operands, size, "%s0x%x(%s,%s,%s)", sign_to_str(operand->disp), abs(operand->disp), to_sib_base_name(instr), sib_reg_idx_names[instr->idx], sib_scale_names[instr->scale]);
		break;
	default:
		break;
	}
	return ret;
}

int
arch_x86_disasm_instr_att(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line)
{
	struct x86_instr instr;
	char operands[32];
	int len = 0;

	assert(((cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT) == 0);
	if (arch_x86_decode_instr(&instr, cpu->RAM, pc, (cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT) != 0) {
		fprintf(stderr, "error: unable to decode opcode %x\n", instr.opcode);
		exit(1);
	}

	operands[0] = '\0';

	/* AT&T syntax operands */
	if (!(instr.flags & OP3_NONE))
		len += print_operand(pc, operands+len, sizeof(operands)-len, &instr, &instr.operand[OPNUM_THIRD]);

	if (!(instr.flags & SRC_NONE) && !(instr.flags & OP3_NONE))
		len += snprintf(operands+len, sizeof(operands)-len, ",");

	if (!(instr.flags & SRC_NONE))
		len += print_operand(pc, operands+len, sizeof(operands)-len, &instr, &instr.operand[OPNUM_SRC]);

	if (!(instr.flags & SRC_NONE) && !(instr.flags & DST_NONE))
		len += snprintf(operands+len, sizeof(operands)-len, ",");

	if (!(instr.flags & DST_NONE))
		len += print_operand(pc, operands+len, sizeof(operands)-len, &instr, &instr.operand[OPNUM_DST]);

    snprintf(line, max_line, "%s%s%-*s%s %s", lock_names[instr.lock_prefix], prefix_names[instr.rep_prefix], strlen(to_mnemonic(&instr)), to_mnemonic(&instr), add_instr_suffix(&instr), operands);

    return arch_x86_instr_length(&instr);
}
