/*
 * libcpu: x86_disasm.cpp
 *
 * disassembler
 */

#include <stdlib.h>
#include <stdio.h>

#include "libcpu.h"
#include "x86_isa.h"
#include "x86_decode.h"

static const char* mnemo[] = {
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

static int
print_operand(addr_t pc, char *operands, size_t size, struct x86_instr *instr, struct x86_operand *operand)
{
	int ret = 0;

	switch (operand->type) {
	case OP_IMM:
		ret = snprintf(operands, size, "$0x%x", operand->imm);
		break;
	case OP_FAR_PTR:
		ret = snprintf(operands, size, "$0x%x,$0x%x", operand->seg_sel, operand->imm);
		break;
	case OP_REL:
		ret = snprintf(operands, size, "0x%x", (unsigned int)((long)pc + instr->nr_bytes + operand->rel));
		break;
	case OP_REG:
		ret = snprintf(operands, size, "%s", to_reg_name(instr, operand->reg));
		break;
	case OP_SEG_REG:
		ret = snprintf(operands, size, "%s", to_seg_reg_name(instr, operand->reg));
		break;
	case OP_MOFFSET:
		ret = snprintf(operands, size, "%s0x%x", sign_to_str(operand->disp), abs(operand->disp));
		break;
	case OP_MEM:
		ret = snprintf(operands, size, "(%s)", to_mem_reg_name(instr, operand->reg));
		break;
	case OP_MEM_DISP:
		ret = snprintf(operands, size, "%s%s0x%x", seg_override_names[instr->seg_override], sign_to_str(operand->disp), abs(operand->disp));
		switch ((instr->addr_size_override << 16) | (instr->mod << 8) | instr->rm) {
		case 5:     // 0, 0, 5
		case 65542: // 1, 0, 6
			break;
		default:
			ret += snprintf(operands+ret, size-ret, "(%s)", to_mem_reg_name(instr, operand->reg));
		}
		break;
	case OP_SIB_MEM:
		ret = snprintf(operands, size, "(%s,%s,%s)", sib_reg_base_names[instr->base], sib_reg_idx_names[instr->idx], sib_scale_names[instr->scale]);
		break;
	case OP_SIB_DISP:
		ret = snprintf(operands, size, "%s0x%x(%s,%s,%s)", sign_to_str(operand->disp), abs(operand->disp), to_sib_base_name(instr), sib_reg_idx_names[instr->idx], sib_scale_names[instr->scale]);
		break;
	default:
		break;
	}
	return ret;
}

int
arch_x86_disasm_instr(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line)
{
	struct x86_instr instr;
	char operands[32];
	int len = 0;

	if (arch_x86_decode_instr(&instr, cpu->RAM, pc) != 0) {
		fprintf(stderr, "error: unable to decode opcode %x\n", instr.opcode);
		exit(1);
	}

	operands[0] = '\0';

	/* AT&T syntax operands */
	if (!(instr.flags & OP3_NONE))
		len += print_operand(pc, operands+len, sizeof(operands)-len, &instr, &instr.third);

	if (!(instr.flags & SRC_NONE) && !(instr.flags & OP3_NONE))
		len += snprintf(operands+len, sizeof(operands)-len, ",");

	if (!(instr.flags & SRC_NONE))
		len += print_operand(pc, operands+len, sizeof(operands)-len, &instr, &instr.src);

	if (!(instr.flags & SRC_NONE) && !(instr.flags & DST_NONE))
		len += snprintf(operands+len, sizeof(operands)-len, ",");

	if (!(instr.flags & DST_NONE))
		len += print_operand(pc, operands+len, sizeof(operands)-len, &instr, &instr.dst);

    snprintf(line, max_line, "%s%s%-8s%s", lock_names[instr.lock_prefix], prefix_names[instr.rep_prefix], to_mnemonic(&instr), operands);

    return arch_x86_instr_length(&instr);
}
