/*
 * libcpu: x86_translate.cpp
 *
 * main translation code
 */

#include <assert.h>

#include "llvm/IR/Instructions.h"

#include "libcpu.h"
#include "libcpu_llvm.h"
#include "x86_isa.h"
#include "x86_cc.h"
#include "frontend.h"
#include "x86_decode.h"

#define BAD fprintf(stderr, "frontend error: unimplemented instruction encountered at line %d\n", __LINE__); exit(1)

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7
#define ES  0
#define CS  1
#define SS  2
#define DS  3
#define FS  4
#define GS  5
#define CR0 0
#define CR1 1
#define CR2 2
#define CR3 3
#define CR4 4
#define DR0 0
#define DR1 1
#define DR2 2
#define DR3 3
#define DR4 4
#define DR5 5
#define DR6 6
#define DR7 7

#define SEG_REG_BASE 9
#define CR_REG_BASE  15
#define DBG_REG_BASE 20

static Value *
arch_x86_get_operand(cpu_t *cpu, struct x86_instr *instr, BasicBlock *bb, unsigned opnum)
{
	if (opnum >= OPNUM_COUNT) {
		assert(0 && "Invalid operand number specified\n");
		return NULL;
	}

	struct x86_operand *operand = &instr->operand[opnum];

	switch (operand->type) {
	case OPTYPE_IMM:
		return CONSTs(32, operand->imm);
	case OPTYPE_MEM:
		if (instr->addr_size_override == 1) {
			Value *reg;
			switch (operand->reg) {
			case EAX:
				reg = ADD(R16(EBX), R16(ESI));
				break;
			case ECX:
				reg = ADD(R16(EBX), R16(EDI));
				break;
			case EDX:
				reg = ADD(R16(EBP), R16(ESI));
				break;
			case EBX:
				reg = ADD(R16(EBP), R16(EDI));
				break;
			case ESP:
				reg = R16(ESI);
				break;
			case EBP:
				reg = R16(EDI);
				break;
			case ESI:
				assert(0 && "operand->reg specifies OPTYPE_MEM_DISP with OPTYPE_MEM!\n");
				return NULL;
			case EDI:
				reg = R16(EBX);
				break;
			default:
				assert(0 && "Unknown reg index in OPTYPE_MEM\n");
				return NULL;
			}
			return LOADMEM16(reg);
		}
		else {
			Value *reg;
			switch (operand->reg) {
			case EAX: // fallthrough
			case ECX: // fallthrough
			case EDX: // fallthrough
			case EBX: // fallthrough
			case ESI: // fallthrough
			case EDI:
				reg = GPR(operand->reg);
				break;
			case ESP:
				assert(0 && "operand->reg specifies SIB with OPTYPE_MEM!\n");
				return NULL;
			case EBP:
				assert(0 && "operand->reg specifies OPTYPE_MEM_DISP with OPTYPE_MEM!\n");
				return NULL;
			default:
				assert(0 && "Unknown reg index in OPTYPE_MEM\n");
				return NULL;
			}
			return LOADMEM32(reg);
		}
	case OPTYPE_MOFFSET:
		return CONSTs(32, operand->disp);
	case OPTYPE_MEM_DISP:
		if (instr->addr_size_override == 1) {
			Value *reg;
			switch (instr->mod) {
			case 0:
				if (instr->rm == 6) {
					reg = CONSTs(16, operand->disp);
				}
				else {
					assert(0 && "instr->mod == 0 but instr->rm != 6 in OPTYPE_MEM_DISP!\n");
					return NULL;
				}
				break;
			case 1:
				switch (instr->rm) {
				case EAX:
					reg = ADD(ADD(R16(EBX), R16(ESI)), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case ECX:
					reg = ADD(ADD(R16(EBX), R16(EDI)), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case EDX:
					reg = ADD(ADD(R16(EBP), R16(ESI)), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case EBX:
					reg = ADD(ADD(R16(EBP), R16(EDI)), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case ESP:
					reg = ADD(R16(ESI), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case EBP:
					reg = ADD(R16(EDI), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case ESI:
					reg = ADD(R16(EBP), SEXT(16, CONSTs(8, operand->disp)));
					break;
				case EDI:
					reg = ADD(R16(EBX), SEXT(16, CONSTs(8, operand->disp)));
					break;
				default:
					assert(0 && "Unknown rm index in OPTYPE_MEM_DISP\n");
					return NULL;
				}
				break;
			case 2:
				switch (instr->rm) {
				case EAX:
					reg = ADD(ADD(R16(EBX), R16(ESI)), CONSTs(32, operand->disp));
					break;
				case ECX:
					reg = ADD(ADD(R16(EBX), R16(EDI)), CONSTs(32, operand->disp));
					break;
				case EDX:
					reg = ADD(ADD(R16(EBP), R16(ESI)), CONSTs(32, operand->disp));
					break;
				case EBX:
					reg = ADD(ADD(R16(EBP), R16(EDI)), CONSTs(32, operand->disp));
					break;
				case ESP:
					reg = ADD(R16(ESI), CONSTs(32, operand->disp));
					break;
				case EBP:
					reg = ADD(R16(EDI), CONSTs(32, operand->disp));
					break;
				case ESI:
					reg = ADD(R16(EBP), CONSTs(32, operand->disp));
					break;
				case EDI:
					reg = ADD(R16(EBX), CONSTs(32, operand->disp));
					break;
				default:
					assert(0 && "Unknown rm index in OPTYPE_MEM_DISP\n");
					return NULL;
				}
				break;
			case 3:
				assert(0 && "instr->rm specifies OPTYPE_REG with OPTYPE_MEM_DISP!\n");
				return NULL;
			default:
				assert(0 && "Unknown rm index in OPTYPE_MEM_DISP\n");
				return NULL;
			}
			return LOADMEM16(reg);
		}
		else {
			Value *reg;
			switch (instr->mod) {
			case 0:
				if (instr->rm == 5) {
					reg = CONSTs(32, operand->disp);
				}
				else {
					assert(0 && "instr->mod == 0 but instr->rm != 5 in OPTYPE_MEM_DISP!\n");
					return NULL;
				}
				break;
			case 1:
				switch (instr->rm) {
				case EAX: // fallthrough
				case ECX: // fallthrough
				case EDX: // fallthrough
				case EBX: // fallthrough
				case EBP: // fallthrough
				case ESI: // fallthrough
				case EDI:
					reg = ADD(GPR(instr->rm), SEXT(32, CONSTs(8, operand->disp)));
					break;
				case ESP:
					assert(0 && "instr->rm specifies OPTYPE_SIB_DISP with OPTYPE_MEM_DISP!\n");
					return NULL;
				default:
					assert(0 && "Unknown rm index in OPTYPE_MEM_DISP\n");
					return NULL;
				}
				break;
			case 2:
				switch (instr->rm) {
				case EAX: // fallthrough
				case ECX: // fallthrough
				case EDX: // fallthrough
				case EBX: // fallthrough
				case EBP: // fallthrough
				case ESI: // fallthrough
				case EDI:
					reg = ADD(GPR(instr->rm), CONSTs(32, operand->disp));
					break;
				case ESP:
					assert(0 && "instr->rm specifies OPTYPE_SIB_DISP with OPTYPE_MEM_DISP!\n");
					return NULL;
				default:
					assert(0 && "Unknown rm index in OPTYPE_MEM_DISP\n");
					return NULL;
				}
				break;
			case 3:
				assert(0 && "instr->rm specifies OPTYPE_REG with OPTYPE_MEM_DISP!\n");
				return NULL;
			default:
				assert(0 && "Unknown rm index in OPTYPE_MEM_DISP\n");
				return NULL;
			}
			return LOADMEM32(reg);
		}
	case OPTYPE_REG:
	case OPTYPE_REG8:
		if (operand->reg > EDI) {
			assert(0 && "Unknown reg index in OPTYPE_REG(8)\n");
			return NULL;
		}
		if (instr->flags & WIDTH_BYTE || operand->type == OPTYPE_REG8) {
			if (operand->reg < ESP) {
				return R8(operand->reg);
			}
			else {
				return R8H(operand->reg - 4);
			}
		}
		else if (instr->flags & WIDTH_WORD) {
			return R16(operand->reg);
		}
		else {
			return R32(operand->reg);
		}
	case OPTYPE_SEG_REG:
		switch (operand->reg) {
		case ES: // fallthrough
		case CS: // fallthrough
		case SS: // fallthrough
		case DS: // fallthrough
		case FS: // fallthrough
		case GS:
			return R16(SEG_REG_BASE + operand->reg);
		case 6:
		case 7:
			assert(0 && "operand->reg specifies a reserved segment register!\n");
			return NULL;
		default:
			assert(0 && "Unknown reg index in OPTYPE_SEG_REG\n");
			return NULL;
		}
	case OPTYPE_CR_REG:
		switch (operand->reg) {
		case CR0: // fallthrough
		case CR2: // fallthrough
		case CR3: // fallthrough
		case CR4:
			return R32(CR_REG_BASE + operand->reg);
		case CR1:
		case 6:
		case 7:
			assert(0 && "operand->reg specifies a reserved control register!\n");
			return NULL;
		default:
			assert(0 && "Unknown reg index in OPTYPE_CR_REG\n");
			return NULL;
		}
	case OPTYPE_DBG_REG:
		switch (operand->reg) {
		case DR0: // fallthrough
		case DR1: // fallthrough
		case DR2: // fallthrough
		case DR3: // fallthrough
		case DR6: // fallthrough
		case DR7:
			return R32(DBG_REG_BASE + operand->reg);
		case DR4:
		case DR5:
			assert(0 && "operand->reg specifies a reserved debug register!\n");
			return NULL;
		default:
			assert(0 && "Unknown reg index in OPTYPE_DBG_REG\n");
			return NULL;
		}
	case OPTYPE_REL:
		return CONSTs(32, operand->rel);
	case OPTYPE_FAR_PTR:
		if (instr->flags & WIDTH_DWORD) {
			return CONSTs(48, ((uint64_t)operand->seg_sel << 32) | operand->imm);
		}
		else {
			return CONSTs(32, ((uint32_t)operand->seg_sel << 16) | operand->imm);
		}
	case OPTYPE_SIB_MEM:
	case OPTYPE_SIB_DISP:
		assert((instr->mod == 0 || instr->mod == 1 || instr->mod == 2) && instr->rm == 4);
		Value *scale, *idx, *base;
		if (instr->scale < 4) {
			scale = CONSTs(32, 1ULL << instr->scale);
		}
		else {
			assert(0 && "Invalid sib scale specified\n");
			return NULL;
		}
		switch (instr->idx) {
		case EAX: // fallthrough
		case ECX: // fallthrough
		case EDX: // fallthrough
		case EBX: // fallthrough
		case EBP: // fallthrough
		case ESI: // fallthrough
		case EDI:
			idx = GPR(instr->idx);
			break;
		case ESP:
			idx = CONSTs(32, 0);
			break;
		default:
			assert(0 && "Unknown sib index specified\n");
			return NULL;
		}
		switch (instr->base) {
		case EAX: // fallthrough
		case ECX: // fallthrough
		case EDX: // fallthrough
		case EBX: // fallthrough
		case ESP: // fallthrough
		case ESI: // fallthrough
		case EDI:
			base = GPR(instr->base);
			break;
		case EBP:
			switch (instr->mod) {
			case 0:
				return ADD(MUL(idx, scale), CONSTs(32, instr->disp));
			case 1:
				return ADD(ADD(MUL(idx, scale), SEXT(32, CONSTs(8, operand->disp))), GPR(EBP));
			case 2:
				return ADD(ADD(MUL(idx, scale), CONSTs(32, operand->disp)), GPR(EBP));
			case 3:
				assert(0 && "instr->mod specifies OPTYPE_REG with sib addressing mode!\n");
				return NULL;
			default:
				assert(0 && "Unknown instr->mod specified with instr->base == 5\n");
				return NULL;
			}
		default:
			assert(0 && "Unknown sib base specified\n");
			return NULL;
		}
		return ADD(base, MUL(idx, scale));
	default:
		assert(0 && "Unknown operand type specified\n");
		return NULL;
	}
}

Value *
arch_x86_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb)
{
	return NULL;
}

int
arch_x86_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb)
{
	struct x86_instr instr;

	if (arch_x86_decode_instr(&instr, cpu->RAM, pc, (cpu->flags_debug & CPU_DEBUG_INTEL_SYNTAX) >> CPU_DEBUG_INTEL_SYNTAX_SHIFT) != 0)
		return -1;

	switch (instr.type) {
    case INSTR_AAA:         BAD;
    case INSTR_AAD:         BAD;
    case INSTR_AAM:         BAD;
    case INSTR_AAS:         BAD;
    case INSTR_ADC:         BAD;
    case INSTR_ADD:         BAD;
    case INSTR_AND:         BAD;
    case INSTR_ARPL:        BAD;
    case INSTR_BOUND:       BAD;
    case INSTR_BSF:         BAD;
    case INSTR_BSR:         BAD;
    case INSTR_BSWAP:       BAD;
    case INSTR_BT:          BAD;
    case INSTR_BTC:         BAD;
    case INSTR_BTR:         BAD;
    case INSTR_BTS:         BAD;
    case INSTR_CALL:        BAD;
    case INSTR_CBW:         BAD;
    case INSTR_CBTV:        BAD;
    case INSTR_CDQ:         BAD;
    case INSTR_CLC:         BAD;
    case INSTR_CLD:         BAD;
    case INSTR_CLI:         BAD;
    case INSTR_CLTD:        BAD;
    case INSTR_CLTS:        BAD;
    case INSTR_CMC:         BAD;
    case INSTR_CMOVA:       BAD;
    case INSTR_CMOVB:       BAD;
    case INSTR_CMOVBE:      BAD;
    case INSTR_CMOVG:       BAD;
    case INSTR_CMOVGE:      BAD;
    case INSTR_CMOVL:       BAD;
    case INSTR_CMOVLE:      BAD;
    case INSTR_CMOVNB:      BAD;
    case INSTR_CMOVNE:      BAD;
    case INSTR_CMOVNO:      BAD;
    case INSTR_CMOVNS:      BAD;
    case INSTR_CMOVO:       BAD;
    case INSTR_CMOVPE:      BAD;
    case INSTR_CMOVPO:      BAD;
    case INSTR_CMOVS:       BAD;
    case INSTR_CMOVZ:       BAD;
    case INSTR_CMP:         BAD;
    case INSTR_CMPS:        BAD;
    case INSTR_CMPXCHG8B:   BAD;
    case INSTR_CMPXCHG:     BAD;
    case INSTR_CPUID:       BAD;
    case INSTR_CWD:         BAD;
    case INSTR_CWDE:        BAD;
    case INSTR_CWTD:        BAD;
    case INSTR_CWTL:        BAD;
    case INSTR_DAA:         BAD;
    case INSTR_DAS:         BAD;
    case INSTR_DEC:         BAD;
    case INSTR_DIV:         BAD;
    case INSTR_ENTER:       BAD;
    case INSTR_HLT:         BAD;
    case INSTR_IDIV:        BAD;
    case INSTR_IMUL:        BAD;
    case INSTR_IN:          BAD;
    case INSTR_INC:         BAD;
    case INSTR_INS:         BAD;
    case INSTR_INT3:        BAD;
    case INSTR_INT:         BAD;
    case INSTR_INTO:        BAD;
    case INSTR_INVD:        BAD;
    case INSTR_INVLPG:      BAD;
    case INSTR_IRET:        BAD;
    case INSTR_JA:          BAD;
    case INSTR_JAE:         BAD;
    case INSTR_JB:          BAD;
    case INSTR_JBE:         BAD;
    case INSTR_JC:          BAD;
    case INSTR_JCXZ:        BAD;
    case INSTR_JE:          BAD;
    case INSTR_JECXZ:       BAD;
    case INSTR_JG:          BAD;
    case INSTR_JGE:         BAD;
    case INSTR_JL:          BAD;
    case INSTR_JLE:         BAD;
    case INSTR_JMP:         BAD;
    case INSTR_JNA:         BAD;
    case INSTR_JNAE:        BAD;
    case INSTR_JNB:         BAD;
    case INSTR_JNBE:        BAD;
    case INSTR_JNC:         BAD;
    case INSTR_JNE:         BAD;
    case INSTR_JNG:         BAD;
    case INSTR_JNGE:        BAD;
    case INSTR_JNL:         BAD;
    case INSTR_JNLE:        BAD;
    case INSTR_JNO:         BAD;
    case INSTR_JNP:         BAD;
    case INSTR_JNS:         BAD;
    case INSTR_JNZ:         BAD;
    case INSTR_JO:          BAD;
    case INSTR_JP:          BAD;
    case INSTR_JPE:         BAD;
    case INSTR_JPO:         BAD;
    case INSTR_JS:          BAD;
    case INSTR_JZ:          BAD;
    case INSTR_LAHF:        BAD;
    case INSTR_LAR:         BAD;
    case INSTR_LCALL:       BAD;
    case INSTR_LDS:         BAD;
    case INSTR_LEA:         BAD;
    case INSTR_LEAVE:       BAD;
    case INSTR_LES:         BAD;
    case INSTR_LFS:         BAD;
    case INSTR_LGDTD:       BAD;
    case INSTR_LGDTL:       BAD;
    case INSTR_LGDTW:       BAD;
    case INSTR_LGS:         BAD;
    case INSTR_LIDTD:       BAD;
    case INSTR_LIDTL:       BAD;
    case INSTR_LIDTW:       BAD;
    case INSTR_LJMP:        BAD;
    case INSTR_LLDT:        BAD;
    case INSTR_LMSW:        BAD;
    case INSTR_LODS:        BAD;
    case INSTR_LOOP:        BAD;
    case INSTR_LOOPE:       BAD;
    case INSTR_LOOPNE:      BAD;
    case INSTR_LOOPNZ:      BAD;
    case INSTR_LOOPZ:       BAD;
    case INSTR_LRET:        BAD;
    case INSTR_LSL:         BAD;
    case INSTR_LSS:         BAD;
    case INSTR_LTR:         BAD;
    case INSTR_MOV:         BAD;
    case INSTR_MOVS:        BAD;
    case INSTR_MOVSX:       BAD;
    case INSTR_MOVSXB:      BAD;
    case INSTR_MOVSXW:      BAD;
    case INSTR_MOVZX:       BAD;
    case INSTR_MOVZXB:      BAD;
    case INSTR_MOVZXW:      BAD;
    case INSTR_MUL:         BAD;
    case INSTR_NEG:         BAD;
    case INSTR_NOP:         BAD;
    case INSTR_NOT:         BAD;
    case INSTR_OR:          BAD;
    case INSTR_OUT:         BAD;
    case INSTR_OUTS:        BAD;
    case INSTR_POP:         BAD;
    case INSTR_POPA:        BAD;
    case INSTR_POPF:        BAD;
    case INSTR_PUSH:        BAD;
    case INSTR_PUSHA:       BAD;
    case INSTR_PUSHF:       BAD;
    case INSTR_RCL:         BAD;
    case INSTR_RCR:         BAD;
    case INSTR_RDMSR:       BAD;
    case INSTR_RDPMC:       BAD;
    case INSTR_RDTSC:       BAD;
    case INSTR_REP:         BAD;
    case INSTR_REPE:        BAD;
    case INSTR_REPNE:       BAD;
    case INSTR_REPNZ:       BAD;
    case INSTR_REPZ:        BAD;
    case INSTR_RET:         BAD;
    case INSTR_RETF:        BAD;
    case INSTR_ROL:         BAD;
    case INSTR_ROR:         BAD;
    case INSTR_RSM:         BAD;
    case INSTR_SAHF:        BAD;
    case INSTR_SAL:         BAD;
    case INSTR_SAR:         BAD;
    case INSTR_SBB:         BAD;
    case INSTR_SCAS:        BAD;
    case INSTR_SETA:        BAD;
    case INSTR_SETB:        BAD;
    case INSTR_SETBE:       BAD;
    case INSTR_SETG:        BAD;
    case INSTR_SETGE:       BAD;
    case INSTR_SETL:        BAD;
    case INSTR_SETLE:       BAD;
    case INSTR_SETNB:       BAD;
    case INSTR_SETNE:       BAD;
    case INSTR_SETNO:       BAD;
    case INSTR_SETNS:       BAD;
    case INSTR_SETO:        BAD;
    case INSTR_SETPE:       BAD;
    case INSTR_SETPO:       BAD;
    case INSTR_SETS:        BAD;
    case INSTR_SETZ:        BAD;
    case INSTR_SGDTD:       BAD;
    case INSTR_SGDTL:       BAD;
    case INSTR_SGDTW:       BAD;
    case INSTR_SHL:         BAD;
    case INSTR_SHLD:        BAD;
    case INSTR_SHR:         BAD;
    case INSTR_SHRD:        BAD;
    case INSTR_SIDTD:       BAD;
    case INSTR_SIDTL:       BAD;
    case INSTR_SIDTW:       BAD;
    case INSTR_SLDT:        BAD;
    case INSTR_SMSW:        BAD;
    case INSTR_STC:         BAD;
    case INSTR_STD:         BAD;
    case INSTR_STI:         BAD;
    case INSTR_STOS:        BAD;
    case INSTR_STR:         BAD;
    case INSTR_SUB:         BAD;
    case INSTR_SYSENTER:    BAD;
    case INSTR_SYSEXIT:     BAD;
    case INSTR_TEST:        BAD;
    case INSTR_UD1:         BAD;
    case INSTR_UD2:         BAD;
    case INSTR_VERR:        BAD;
    case INSTR_VERW:        BAD;
    case INSTR_WBINVD:      BAD;
    case INSTR_WRMSR:       BAD;
    case INSTR_XADD:        BAD;
    case INSTR_XCHG:        BAD;
    case INSTR_XLATB:       BAD;
    case INSTR_XOR:         BAD;
	default:
		fprintf(stderr, "INVALID %s:%d\n", __func__, __LINE__);
		exit(1);
	}

	return 0;
}
