/*
 * libcpu: x86_tag.cpp
 *
 * tagging code
 */

#include "libcpu.h"
#include "x86_decode.h"
#include "x86_isa.h"
#include "tag.h"

int
arch_x86_tag_instr(cpu_t *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	struct x86_instr instr;
	int len;

	if (arch_x86_decode_instr(&instr, cpu->RAM, pc, (cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT) != 0)
		return -1;

	len = arch_x86_instr_length(&instr);

	switch (instr.type) {
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
		*new_pc = pc + len + instr.rel_data[0];
		*tag = TAG_COND_BRANCH;
		break;
	case INSTR_JMP:
	case INSTR_LJMP:
		switch (instr.opcode) {
		case 0xE9:
		case 0xEB:
			*new_pc = pc + len + instr.rel_data[0];
			break;
		case 0xEA:
			*new_pc = NEW_PC_NONE;
			break;
		case 0xFF:
			*new_pc = NEW_PC_NONE;
			break;
		default:
			break;
		}
		*tag = TAG_BRANCH;
		break;
	case INSTR_LOOP:
	case INSTR_LOOPE:
	case INSTR_LOOPNE:
		*new_pc = pc + len + instr.rel_data[0];
		*tag = TAG_COND_BRANCH;
		break;
	case INSTR_CALL:
	case INSTR_LCALL:
	case INSTR_SYSENTER:
		switch (instr.opcode) {
		case 0x34:
			*new_pc = NEW_PC_NONE;
			break;
		case 0x9A:
			*new_pc = NEW_PC_NONE;
			break;
		case 0xE8:
			*new_pc = pc + len + instr.rel_data[0];
			break;
		case 0xFF:
			*new_pc = NEW_PC_NONE;
			break;
		default:
			break;
		}
		*tag = TAG_CALL;
		break;
	case INSTR_RET:
	case INSTR_LRET:
	case INSTR_RETF:
	case INSTR_IRET:
	case INSTR_SYSEXIT:
		*tag = TAG_RET;
		break;
	case INSTR_INT:
	case INSTR_INTO:
	case INSTR_INT3:
	case INSTR_BOUND:
		*tag = TAG_TRAP;
		break;
	default:
		*tag = TAG_CONTINUE;
		break;
	}

	*next_pc = pc + len;

	return len;
}
