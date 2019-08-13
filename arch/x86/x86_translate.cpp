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
arch_x86_get_operand(cpu_t *cpu, struct x86_instr *instr, BasicBlock *bb, unsigned opnum, size_t *opsize)
{
	if (opnum >= OPNUM_COUNT) {
		assert(0 && "Invalid operand number specified\n");
		return NULL;
	}

	struct x86_operand *operand = &instr->operand[opnum];

	switch (operand->type) {
	case OPTYPE_IMM:
		if (instr->flags & (SRC_IMM8 | OP3_IMM8)) {
			*opsize = 8;
			return CONSTs(*opsize, operand->imm);
		}
		else if (instr->flags & DST_IMM16) {
			*opsize = 16;
			return CONSTs(*opsize, operand->imm);
		}
		switch (instr->flags & WIDTH_MASK) {
		case WIDTH_BYTE:
			*opsize = 8;
			break;
		case WIDTH_WORD:
			*opsize = 16;
			break;
		case WIDTH_DWORD:
			*opsize = 32;
			break;
		default:
			fprintf(stderr, "Missing operand size in OPTYPE_IMM (calling %s on an instruction without operands?)\n", __func__);
			*opsize = 0;
			return NULL;
		}
		return CONSTs(*opsize, operand->imm);
	case OPTYPE_MEM:
		*opsize = 0;
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
			*opsize = 16;
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
			*opsize = 32;
			return LOADMEM32(reg);
		}
	case OPTYPE_MOFFSET:
		switch (instr->flags & WIDTH_MASK) {
		case WIDTH_BYTE:
			*opsize = 8;
			break;
		case WIDTH_WORD:
			*opsize = 16;
			break;
		case WIDTH_DWORD:
			*opsize = 32;
			break;
		default:
			fprintf(stderr, "Missing operand size in OPTYPE_MOFFSET (calling %s on an instruction without operands?)\n", __func__);
			*opsize = 0;
			return NULL;
		}
		return CONSTs(*opsize, operand->disp);
	case OPTYPE_MEM_DISP:
		*opsize = 0;
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
			*opsize = 16;
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
			*opsize = 32;
			return LOADMEM32(reg);
		}
	case OPTYPE_REG:
	case OPTYPE_REG8:
		*opsize = 0;
		if (operand->reg > EDI) {
			assert(0 && "Unknown reg index in OPTYPE_REG(8)\n");
			return NULL;
		}
		if (instr->flags & WIDTH_BYTE || operand->type == OPTYPE_REG8) {
			*opsize = 8;
			if (operand->reg < ESP) {
				return R8(operand->reg);
			}
			else {
				return R8H(operand->reg - 4);
			}
		}
		else if (instr->flags & WIDTH_WORD) {
			*opsize = 16;
			return R16(operand->reg);
		}
		else {
			*opsize = 32;
			return R32(operand->reg);
		}
	case OPTYPE_SEG_REG:
		*opsize = 0;
		switch (operand->reg) {
		case ES: // fallthrough
		case CS: // fallthrough
		case SS: // fallthrough
		case DS: // fallthrough
		case FS: // fallthrough
		case GS:
			*opsize = 16;
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
		*opsize = 0;
		switch (operand->reg) {
		case CR0: // fallthrough
		case CR2: // fallthrough
		case CR3: // fallthrough
		case CR4:
			*opsize = 32;
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
		*opsize = 0;
		switch (operand->reg) {
		case DR0: // fallthrough
		case DR1: // fallthrough
		case DR2: // fallthrough
		case DR3: // fallthrough
		case DR6: // fallthrough
		case DR7:
			*opsize = 32;
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
		switch (instr->flags &WIDTH_MASK) {
		case WIDTH_BYTE:
			*opsize = 8;
			break;
		case WIDTH_WORD:
			*opsize = 16;
			break;
		case WIDTH_DWORD:
			*opsize = 32;
			break;
		default:
			fprintf(stderr, "Missing operand size in OPTYPE_REL (calling %s on an instruction without operands?)\n", __func__);
			*opsize = 0;
			return NULL;
		}
		return CONSTs(*opsize, operand->rel);
	case OPTYPE_FAR_PTR:
		if (instr->flags & WIDTH_DWORD) {
			*opsize = 48;
			return CONSTs(48, ((uint64_t)operand->seg_sel << 32) | operand->imm);
		}
		else {
			*opsize = 32;
			return CONSTs(32, ((uint32_t)operand->seg_sel << 16) | operand->imm);
		}
	case OPTYPE_SIB_MEM:
	case OPTYPE_SIB_DISP:
		assert((instr->mod == 0 || instr->mod == 1 || instr->mod == 2) && instr->rm == 4);
		*opsize = 0;
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
				*opsize = 32;
				return ADD(MUL(idx, scale), CONSTs(32, instr->disp));
			case 1:
				*opsize = 32;
				return ADD(ADD(MUL(idx, scale), SEXT(32, CONSTs(8, operand->disp))), GPR(EBP));
			case 2:
				*opsize = 32;
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
		*opsize = 32;
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
	case X86_OPC_AAA:         BAD;
	case X86_OPC_AAD:         BAD;
	case X86_OPC_AAM:         BAD;
	case X86_OPC_AAS:         BAD;
	case X86_OPC_ADC:         BAD;
	case X86_OPC_ADD:         BAD;
	case X86_OPC_AND:         BAD;
	case X86_OPC_ARPL:        BAD;
	case X86_OPC_BOUND:       BAD;
	case X86_OPC_BSF:         BAD;
	case X86_OPC_BSR:         BAD;
	case X86_OPC_BSWAP:       BAD;
	case X86_OPC_BT:          BAD;
	case X86_OPC_BTC:         BAD;
	case X86_OPC_BTR:         BAD;
	case X86_OPC_BTS:         BAD;
	case X86_OPC_CALL:        BAD;
	case X86_OPC_CBW:         BAD;
	case X86_OPC_CBTV:        BAD;
	case X86_OPC_CDQ:         BAD;
	case X86_OPC_CLC:         BAD;
	case X86_OPC_CLD:         BAD;
	case X86_OPC_CLI:         BAD;
	case X86_OPC_CLTD:        BAD;
	case X86_OPC_CLTS:        BAD;
	case X86_OPC_CMC:         BAD;
	case X86_OPC_CMOVA:       BAD;
	case X86_OPC_CMOVB:       BAD;
	case X86_OPC_CMOVBE:      BAD;
	case X86_OPC_CMOVG:       BAD;
	case X86_OPC_CMOVGE:      BAD;
	case X86_OPC_CMOVL:       BAD;
	case X86_OPC_CMOVLE:      BAD;
	case X86_OPC_CMOVNB:      BAD;
	case X86_OPC_CMOVNE:      BAD;
	case X86_OPC_CMOVNO:      BAD;
	case X86_OPC_CMOVNS:      BAD;
	case X86_OPC_CMOVO:       BAD;
	case X86_OPC_CMOVPE:      BAD;
	case X86_OPC_CMOVPO:      BAD;
	case X86_OPC_CMOVS:       BAD;
	case X86_OPC_CMOVZ:       BAD;
	case X86_OPC_CMP:         BAD;
	case X86_OPC_CMPS:        BAD;
	case X86_OPC_CMPXCHG8B:   BAD;
	case X86_OPC_CMPXCHG:     BAD;
	case X86_OPC_CPUID:       BAD;
	case X86_OPC_CWD:         BAD;
	case X86_OPC_CWDE:        BAD;
	case X86_OPC_CWTD:        BAD;
	case X86_OPC_CWTL:        BAD;
	case X86_OPC_DAA:         BAD;
	case X86_OPC_DAS:         BAD;
	case X86_OPC_DEC:         BAD;
	case X86_OPC_DIV:         BAD;
	case X86_OPC_ENTER:       BAD;
	case X86_OPC_HLT:         BAD;
	case X86_OPC_IDIV:        BAD;
	case X86_OPC_IMUL:        BAD;
	case X86_OPC_IN:          BAD;
	case X86_OPC_INC:         BAD;
	case X86_OPC_INS:         BAD;
	case X86_OPC_INT3:        BAD;
	case X86_OPC_INT:         BAD;
	case X86_OPC_INTO:        BAD;
	case X86_OPC_INVD:        BAD;
	case X86_OPC_INVLPG:      BAD;
	case X86_OPC_IRET:        BAD;
	case X86_OPC_JA:          BAD;
	case X86_OPC_JAE:         BAD;
	case X86_OPC_JB:          BAD;
	case X86_OPC_JBE:         BAD;
	case X86_OPC_JC:          BAD;
	case X86_OPC_JCXZ:        BAD;
	case X86_OPC_JE:          BAD;
	case X86_OPC_JECXZ:       BAD;
	case X86_OPC_JG:          BAD;
	case X86_OPC_JGE:         BAD;
	case X86_OPC_JL:          BAD;
	case X86_OPC_JLE:         BAD;
	case X86_OPC_JMP:         BAD;
	case X86_OPC_JNA:         BAD;
	case X86_OPC_JNAE:        BAD;
	case X86_OPC_JNB:         BAD;
	case X86_OPC_JNBE:        BAD;
	case X86_OPC_JNC:         BAD;
	case X86_OPC_JNE:         BAD;
	case X86_OPC_JNG:         BAD;
	case X86_OPC_JNGE:        BAD;
	case X86_OPC_JNL:         BAD;
	case X86_OPC_JNLE:        BAD;
	case X86_OPC_JNO:         BAD;
	case X86_OPC_JNP:         BAD;
	case X86_OPC_JNS:         BAD;
	case X86_OPC_JNZ:         BAD;
	case X86_OPC_JO:          BAD;
	case X86_OPC_JP:          BAD;
	case X86_OPC_JPE:         BAD;
	case X86_OPC_JPO:         BAD;
	case X86_OPC_JS:          BAD;
	case X86_OPC_JZ:          BAD;
	case X86_OPC_LAHF:        BAD;
	case X86_OPC_LAR:         BAD;
	case X86_OPC_LCALL:       BAD;
	case X86_OPC_LDS:         BAD;
	case X86_OPC_LEA:         BAD;
	case X86_OPC_LEAVE:       BAD;
	case X86_OPC_LES:         BAD;
	case X86_OPC_LFS:         BAD;
	case X86_OPC_LGDTD:       BAD;
	case X86_OPC_LGDTL:       BAD;
	case X86_OPC_LGDTW:       BAD;
	case X86_OPC_LGS:         BAD;
	case X86_OPC_LIDTD:       BAD;
	case X86_OPC_LIDTL:       BAD;
	case X86_OPC_LIDTW:       BAD;
	case X86_OPC_LJMP:        BAD;
	case X86_OPC_LLDT:        BAD;
	case X86_OPC_LMSW:        BAD;
	case X86_OPC_LODS:        BAD;
	case X86_OPC_LOOP:        BAD;
	case X86_OPC_LOOPE:       BAD;
	case X86_OPC_LOOPNE:      BAD;
	case X86_OPC_LOOPNZ:      BAD;
	case X86_OPC_LOOPZ:       BAD;
	case X86_OPC_LRET:        BAD;
	case X86_OPC_LSL:         BAD;
	case X86_OPC_LSS:         BAD;
	case X86_OPC_LTR:         BAD;
	case X86_OPC_MOV:         BAD;
	case X86_OPC_MOVS:        BAD;
	case X86_OPC_MOVSX:       BAD;
	case X86_OPC_MOVSXB:      BAD;
	case X86_OPC_MOVSXW:      BAD;
	case X86_OPC_MOVZX:       BAD;
	case X86_OPC_MOVZXB:      BAD;
	case X86_OPC_MOVZXW:      BAD;
	case X86_OPC_MUL:         BAD;
	case X86_OPC_NEG:         BAD;
	case X86_OPC_NOP:         BAD;
	case X86_OPC_NOT:         BAD;
	case X86_OPC_OR:          BAD;
	case X86_OPC_OUT:         BAD;
	case X86_OPC_OUTS:        BAD;
	case X86_OPC_POP:         BAD;
	case X86_OPC_POPA:        BAD;
	case X86_OPC_POPF:        BAD;
	case X86_OPC_PUSH:        BAD;
	case X86_OPC_PUSHA:       BAD;
	case X86_OPC_PUSHF:       BAD;
	case X86_OPC_RCL:         BAD;
	case X86_OPC_RCR:         BAD;
	case X86_OPC_RDMSR:       BAD;
	case X86_OPC_RDPMC:       BAD;
	case X86_OPC_RDTSC:       BAD;
	case X86_OPC_REP:         BAD;
	case X86_OPC_REPE:        BAD;
	case X86_OPC_REPNE:       BAD;
	case X86_OPC_REPNZ:       BAD;
	case X86_OPC_REPZ:        BAD;
	case X86_OPC_RET:         BAD;
	case X86_OPC_RETF:        BAD;
	case X86_OPC_ROL:         BAD;
	case X86_OPC_ROR:         BAD;
	case X86_OPC_RSM:         BAD;
	case X86_OPC_SAHF:        BAD;
	case X86_OPC_SAL:         BAD;
	case X86_OPC_SAR:         BAD;
	case X86_OPC_SBB:         BAD;
	case X86_OPC_SCAS:        BAD;
	case X86_OPC_SETA:        BAD;
	case X86_OPC_SETB:        BAD;
	case X86_OPC_SETBE:       BAD;
	case X86_OPC_SETG:        BAD;
	case X86_OPC_SETGE:       BAD;
	case X86_OPC_SETL:        BAD;
	case X86_OPC_SETLE:       BAD;
	case X86_OPC_SETNB:       BAD;
	case X86_OPC_SETNE:       BAD;
	case X86_OPC_SETNO:       BAD;
	case X86_OPC_SETNS:       BAD;
	case X86_OPC_SETO:        BAD;
	case X86_OPC_SETPE:       BAD;
	case X86_OPC_SETPO:       BAD;
	case X86_OPC_SETS:        BAD;
	case X86_OPC_SETZ:        BAD;
	case X86_OPC_SGDTD:       BAD;
	case X86_OPC_SGDTL:       BAD;
	case X86_OPC_SGDTW:       BAD;
	case X86_OPC_SHL:         BAD;
	case X86_OPC_SHLD:        BAD;
	case X86_OPC_SHR:         BAD;
	case X86_OPC_SHRD:        BAD;
	case X86_OPC_SIDTD:       BAD;
	case X86_OPC_SIDTL:       BAD;
	case X86_OPC_SIDTW:       BAD;
	case X86_OPC_SLDT:        BAD;
	case X86_OPC_SMSW:        BAD;
	case X86_OPC_STC:         BAD;
	case X86_OPC_STD:         BAD;
	case X86_OPC_STI:         BAD;
	case X86_OPC_STOS:        BAD;
	case X86_OPC_STR:         BAD;
	case X86_OPC_SUB:         BAD;
	case X86_OPC_SYSENTER:    BAD;
	case X86_OPC_SYSEXIT:     BAD;
	case X86_OPC_TEST:        BAD;
	case X86_OPC_UD1:         BAD;
	case X86_OPC_UD2:         BAD;
	case X86_OPC_VERR:        BAD;
	case X86_OPC_VERW:        BAD;
	case X86_OPC_WBINVD:      BAD;
	case X86_OPC_WRMSR:       BAD;
	case X86_OPC_XADD:        BAD;
	case X86_OPC_XCHG:        BAD;
	case X86_OPC_XLATB:       BAD;
	case X86_OPC_XOR:         BAD;
	default:
		fprintf(stderr, "INVALID %s:%d\n", __func__, __LINE__);
		exit(1);
	}

	return 0;
}
