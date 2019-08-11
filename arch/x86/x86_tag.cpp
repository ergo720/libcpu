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

	// TODO : Is INTEL_SYNTAX needed during tagging? If not, less opcodes would need to be checked for below.
	if (arch_x86_decode_instr(&instr, cpu->RAM, pc, (cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT) != 0)
		return -1;

	len = arch_x86_instr_length(&instr);

	switch (instr.opcode) {
	case X86_OPC_JO:
	case X86_OPC_JNO:
	case X86_OPC_JB:
	case X86_OPC_JNB:
	case X86_OPC_JZ:
	case X86_OPC_JNE:
	case X86_OPC_JBE:
	case X86_OPC_JA:
	case X86_OPC_JS:
	case X86_OPC_JNS:
	case X86_OPC_JPE:
	case X86_OPC_JPO:
	case X86_OPC_JL:
	case X86_OPC_JGE:
	case X86_OPC_JLE:
	case X86_OPC_JG:
	case X86_OPC_LOOP:
	case X86_OPC_LOOPE:
	case X86_OPC_LOOPNE:
		*tag = TAG_COND_BRANCH;
		*new_pc = pc + len + instr.rel_data[0];
		break;
	case X86_OPC_JMP:
	case X86_OPC_LJMP:
		*tag = TAG_BRANCH;
		switch (instr.opcode_byte) {
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
		break;
	case X86_OPC_CALL: // Intel syntax specific
	case X86_OPC_LCALL: // AT&T syntax specific
	case X86_OPC_SYSENTER:
		*tag = TAG_CALL;
		switch (instr.opcode_byte) {
		case 0x34: // X86_OPC_SYSENTER is in decode_table_two[0x34] (nowhere else)
			*new_pc = NEW_PC_NONE;
			break;
		case 0x9A: // X86_OPC_CALL or X86_OPC_LCALL is selected via X86_OPC_DIFF_SYNTAX in decode_table_one[0x9A]
			*new_pc = NEW_PC_NONE;
			break;
		case 0xE8: // Only decode_table_one has X86_OPC_CALL in index 0xE8
			*new_pc = pc + len + instr.rel_data[0];
			break;
		case 0xFF: // decode_table_one[0xFF] = GROUP_5, which forwards to grp5_decode_table, which contains X86_OPC_CALL
			*new_pc = NEW_PC_NONE;
			break;
		default:
			break;
		}
		break;
	case X86_OPC_RET:
	case X86_OPC_LRET:
	case X86_OPC_RETF:
	case X86_OPC_IRET:
	case X86_OPC_SYSEXIT:
		*tag = TAG_RET;
		break;
	case X86_OPC_INT:
	case X86_OPC_INTO:
	case X86_OPC_INT3:
	case X86_OPC_BOUND:
		*tag = TAG_TRAP;
		break;
	default:
		*tag = TAG_CONTINUE;
		break;
	}

	*next_pc = pc + len;

	return len;
}
