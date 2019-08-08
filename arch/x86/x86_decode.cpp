/*
 * libcpu: x86_decode.cpp
 *
 * instruction decoding
 */

#include "libcpu.h"
#include "x86_isa.h"
#include "x86_decode.h"

/*
 * First byte of an element in 'decode_table_one' is the instruction type.
 */
#define X86_INSTR_TYPE_MASK	0xFF

#define INSTR_UNDEFINED		0	// non-existent instr
#define INSTR_DIFF_SYNTAX	0	// instr has different syntax between intel and att
#define OP_SIZE_OVERRIDE	0x100
#define USE_INTEL			0x10000

#define Jb (ADDMODE_REL | WIDTH_BYTE)
#define Jv (ADDMODE_REL | WIDTH_DWORD)
#define Cv (ADDMODE_RM_REG | WIDTH_DWORD)
#define Sb (ADDMODE_RM | WIDTH_BYTE)

static const uint64_t decode_table_one[256] = {
	/*[0x00]*/	INSTR_ADD | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x01]*/	INSTR_ADD | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x02]*/	INSTR_ADD | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x03]*/	INSTR_ADD | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x04]*/	INSTR_ADD | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x05]*/	INSTR_ADD | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x06]*/	INSTR_PUSH | ADDMODE_SEG2_REG /* ES */ | WIDTH_DWORD,
	/*[0x07]*/	INSTR_POP | ADDMODE_SEG2_REG /* ES */ | WIDTH_DWORD,
	/*[0x08]*/	INSTR_OR | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x09]*/	INSTR_OR | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x0A]*/	INSTR_OR | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x0B]*/	INSTR_OR | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x0C]*/	INSTR_OR | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x0D]*/	INSTR_OR | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x0E]*/	INSTR_PUSH | ADDMODE_SEG2_REG /* CS */ | WIDTH_DWORD,
	/*[0x0F]*/	0 /* TWO BYTE OPCODE */,
	/*[0x10]*/	INSTR_ADC | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x11]*/	INSTR_ADC | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x12]*/	INSTR_ADC | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x13]*/	INSTR_ADC | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x14]*/	INSTR_ADC | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x15]*/	INSTR_ADC | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x16]*/	INSTR_PUSH | ADDMODE_SEG2_REG /* SS */ | WIDTH_DWORD,
	/*[0x17]*/	INSTR_POP | ADDMODE_SEG2_REG /* SS */ | WIDTH_DWORD,
	/*[0x18]*/	INSTR_SBB | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x19]*/	INSTR_SBB | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x1A]*/	INSTR_SBB | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x1B]*/	INSTR_SBB | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x1C]*/	INSTR_SBB | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x1D]*/	INSTR_SBB | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x1E]*/	INSTR_PUSH | ADDMODE_SEG2_REG /* DS */ | WIDTH_DWORD,
	/*[0x1F]*/	INSTR_POP | ADDMODE_SEG2_REG /* DS */ | WIDTH_DWORD,
	/*[0x20]*/	INSTR_AND | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x21]*/	INSTR_AND | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x22]*/	INSTR_AND | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x23]*/	INSTR_AND | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x24]*/	INSTR_AND | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x25]*/	INSTR_AND | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x26]*/	0 /* ES_OVERRIDE */,
	/*[0x27]*/	INSTR_DAA | ADDMODE_IMPLIED,
	/*[0x28]*/	INSTR_SUB | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x29]*/	INSTR_SUB | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x2A]*/	INSTR_SUB | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x2B]*/	INSTR_SUB | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x2C]*/	INSTR_SUB | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x2D]*/	INSTR_SUB | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x2E]*/	0 /* CS_OVERRIDE */,
	/*[0x2F]*/	INSTR_DAS | ADDMODE_IMPLIED,
	/*[0x30]*/	INSTR_XOR | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x31]*/	INSTR_XOR | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x32]*/	INSTR_XOR | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x33]*/	INSTR_XOR | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x34]*/	INSTR_XOR | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x35]*/	INSTR_XOR | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x36]*/	0 /* SS_OVERRIDE */,
	/*[0x37]*/	INSTR_AAA | ADDMODE_IMPLIED,
	/*[0x38]*/	INSTR_CMP | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x39]*/	INSTR_CMP | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x3A]*/	INSTR_CMP | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x3B]*/	INSTR_CMP | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x3C]*/	INSTR_CMP | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0x3D]*/	INSTR_CMP | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0x3E]*/	0 /* DS_OVERRIDE */,
	/*[0x3F]*/	INSTR_AAS | ADDMODE_IMPLIED,
	/*[0x40]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x41]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x42]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x43]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x44]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x45]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x46]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x47]*/	INSTR_INC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x48]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x49]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x4A]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x4B]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x4C]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x4D]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x4E]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x4F]*/	INSTR_DEC | ADDMODE_REG | WIDTH_DWORD,
	/*[0x50]*/	INSTR_PUSH | ADDMODE_REG /* AX */ | WIDTH_DWORD,
	/*[0x51]*/	INSTR_PUSH | ADDMODE_REG /* CX */ | WIDTH_DWORD,
	/*[0x52]*/	INSTR_PUSH | ADDMODE_REG /* DX */ | WIDTH_DWORD,
	/*[0x53]*/	INSTR_PUSH | ADDMODE_REG /* BX */ | WIDTH_DWORD,
	/*[0x54]*/	INSTR_PUSH | ADDMODE_REG /* SP */ | WIDTH_DWORD,
	/*[0x55]*/	INSTR_PUSH | ADDMODE_REG /* BP */ | WIDTH_DWORD,
	/*[0x56]*/	INSTR_PUSH | ADDMODE_REG /* SI */ | WIDTH_DWORD,
	/*[0x57]*/	INSTR_PUSH | ADDMODE_REG /* DI */ | WIDTH_DWORD,
	/*[0x58]*/	INSTR_POP | ADDMODE_REG /* AX */  | WIDTH_DWORD,
	/*[0x59]*/	INSTR_POP | ADDMODE_REG /* CX */  | WIDTH_DWORD,
	/*[0x5A]*/	INSTR_POP | ADDMODE_REG /* DX */  | WIDTH_DWORD,
	/*[0x5B]*/	INSTR_POP | ADDMODE_REG /* BX */  | WIDTH_DWORD,
	/*[0x5C]*/	INSTR_POP | ADDMODE_REG /* SP */  | WIDTH_DWORD,
	/*[0x5D]*/	INSTR_POP | ADDMODE_REG /* BP */  | WIDTH_DWORD,
	/*[0x5E]*/	INSTR_POP | ADDMODE_REG /* SI */  | WIDTH_DWORD,
	/*[0x5F]*/	INSTR_POP | ADDMODE_REG /* DI */  | WIDTH_DWORD,
	/*[0x60]*/	INSTR_PUSHA | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0x61]*/	INSTR_POPA | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0x62]*/	INSTR_DIFF_SYNTAX | ADDMODE_RM_REG,
	/*[0x63]*/	INSTR_ARPL | ADDMODE_REG_RM | WIDTH_WORD,
	/*[0x64]*/	0 /* FS_OVERRIDE */,
	/*[0x65]*/	0 /* GS_OVERRIDE */,
	/*[0x66]*/	0 /* OPERAND_SIZE_OVERRIDE */,
	/*[0x67]*/	0 /* ADDRESS_SIZE_OVERRIDE */,
	/*[0x68]*/	INSTR_PUSH | ADDMODE_IMM | WIDTH_DWORD,
	/*[0x69]*/	INSTR_IMUL | ADDMODE_RM_IMM_REG | WIDTH_DWORD,
	/*[0x6A]*/	INSTR_PUSH | ADDMODE_IMM | WIDTH_BYTE,
	/*[0x6B]*/	INSTR_IMUL | ADDMODE_RM_IMM8_REG | WIDTH_DWORD,
	/*[0x6C]*/	INSTR_INS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0x6D]*/	INSTR_INS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0x6E]*/	INSTR_OUTS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0x6F]*/	INSTR_OUTS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0x70]*/	INSTR_JO  | Jb,
	/*[0x71]*/	INSTR_JNO | Jb,
	/*[0x72]*/	INSTR_JB  | Jb,
	/*[0x73]*/	INSTR_JNB | Jb,
	/*[0x74]*/	INSTR_JZ  | Jb,
	/*[0x75]*/	INSTR_JNE | Jb,
	/*[0x76]*/	INSTR_JBE | Jb,
	/*[0x77]*/	INSTR_JA  | Jb,
	/*[0x78]*/	INSTR_JS  | Jb,
	/*[0x79]*/	INSTR_JNS | Jb,
	/*[0x7A]*/	INSTR_JPE | Jb,
	/*[0x7B]*/	INSTR_JPO | Jb,
	/*[0x7C]*/	INSTR_JL  | Jb,
	/*[0x7D]*/	INSTR_JGE | Jb,
	/*[0x7E]*/	INSTR_JLE | Jb,
	/*[0x7F]*/	INSTR_JG  | Jb,
	/*[0x80]*/	GROUP_1 | ADDMODE_IMM8_RM | WIDTH_BYTE,
	/*[0x81]*/	GROUP_1 | ADDMODE_IMM_RM | WIDTH_DWORD,
	/*[0x82]*/	GROUP_1 | ADDMODE_IMM8_RM | WIDTH_BYTE,
	/*[0x83]*/	GROUP_1 | ADDMODE_IMM8_RM | WIDTH_DWORD,
	/*[0x84]*/	INSTR_TEST | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x85]*/	INSTR_TEST | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x86]*/	INSTR_XCHG | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x87]*/	INSTR_XCHG | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x88]*/	INSTR_MOV | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0x89]*/	INSTR_MOV | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0x8A]*/	INSTR_MOV | ADDMODE_RM_REG | WIDTH_BYTE,
	/*[0x8B]*/	INSTR_MOV | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x8C]*/	INSTR_MOV | ADDMODE_SEG3_REG_RM | WIDTH_WORD,
	/*[0x8D]*/	INSTR_LEA | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x8E]*/	INSTR_MOV | ADDMODE_RM_SEG_REG | WIDTH_WORD,
	/*[0x8F]*/	INSTR_POP | ADDMODE_RM | WIDTH_DWORD,
	/*[0x90]*/	INSTR_NOP | ADDMODE_IMPLIED,	/* xchg eax, eax */
	/*[0x91]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x92]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x93]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x94]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x95]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x96]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x97]*/	INSTR_XCHG | ADDMODE_ACC_REG | WIDTH_DWORD,
	/*[0x98]*/	INSTR_DIFF_SYNTAX | ADDMODE_IMPLIED,
	/*[0x99]*/	INSTR_DIFF_SYNTAX | ADDMODE_IMPLIED,
	/*[0x9A]*/	INSTR_DIFF_SYNTAX | ADDMODE_FAR_PTR | WIDTH_DWORD,
	/*[0x9B]*/	INSTR_UNDEFINED, // fpu
	/*[0x9C]*/	INSTR_PUSHF | ADDMODE_IMPLIED,
	/*[0x9D]*/	INSTR_POPF | ADDMODE_IMPLIED,
	/*[0x9E]*/	INSTR_SAHF | ADDMODE_IMPLIED,
	/*[0x9F]*/	INSTR_LAHF | ADDMODE_IMPLIED,
	/*[0xA0]*/	INSTR_MOV | ADDMODE_MOFFSET_ACC | WIDTH_BYTE, /* load */
	/*[0xA1]*/	INSTR_MOV | ADDMODE_MOFFSET_ACC | WIDTH_DWORD, /* load */
	/*[0xA2]*/	INSTR_MOV | ADDMODE_ACC_MOFFSET | WIDTH_BYTE, /* store */
	/*[0xA3]*/	INSTR_MOV | ADDMODE_ACC_MOFFSET | WIDTH_DWORD, /* store */
	/*[0xA4]*/	INSTR_MOVS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xA5]*/	INSTR_MOVS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xA6]*/	INSTR_CMPS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xA7]*/	INSTR_CMPS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xA8]*/	INSTR_TEST | ADDMODE_IMM_ACC | WIDTH_BYTE,
	/*[0xA9]*/	INSTR_TEST | ADDMODE_IMM_ACC | WIDTH_DWORD,
	/*[0xAA]*/	INSTR_STOS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xAB]*/	INSTR_STOS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xAC]*/	INSTR_LODS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xAD]*/	INSTR_LODS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xAE]*/	INSTR_SCAS | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xAF]*/	INSTR_SCAS | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xB0]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB1]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB2]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB3]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB4]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB5]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB6]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB7]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_BYTE,
	/*[0xB8]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xB9]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xBA]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xBB]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xBC]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xBD]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xBE]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xBF]*/	INSTR_MOV | ADDMODE_IMM_REG | WIDTH_DWORD,
	/*[0xC0]*/	GROUP_2 | ADDMODE_IMM8_RM | WIDTH_BYTE,
	/*[0xC1]*/	GROUP_2 | ADDMODE_IMM8_RM | WIDTH_DWORD,
	/*[0xC2]*/	INSTR_RET | ADDMODE_IMM | WIDTH_WORD,
	/*[0xC3]*/	INSTR_RET | ADDMODE_IMPLIED,
	/*[0xC4]*/	INSTR_LES | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xC5]*/	INSTR_LDS | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xC6]*/	INSTR_MOV | ADDMODE_IMM8_RM | WIDTH_BYTE,
	/*[0xC7]*/	INSTR_MOV | ADDMODE_IMM_RM | WIDTH_DWORD,
	/*[0xC8]*/	INSTR_ENTER | ADDMODE_IMM8_IMM16 | WIDTH_WORD,
	/*[0xC9]*/	INSTR_LEAVE | ADDMODE_IMPLIED,
	/*[0xCA]*/	INSTR_DIFF_SYNTAX | ADDMODE_IMM | WIDTH_WORD,
	/*[0xCB]*/	INSTR_DIFF_SYNTAX | ADDMODE_IMPLIED,
	/*[0xCC]*/	INSTR_INT3 | ADDMODE_IMPLIED,
	/*[0xCD]*/	INSTR_INT | ADDMODE_IMM | WIDTH_BYTE,
	/*[0xCE]*/	INSTR_INTO | ADDMODE_IMPLIED,
	/*[0xCF]*/	INSTR_IRET | ADDMODE_IMPLIED,
	/*[0xD0]*/	GROUP_2 | ADDMODE_RM | WIDTH_BYTE,
	/*[0xD1]*/	GROUP_2 | ADDMODE_RM | WIDTH_DWORD,
	/*[0xD2]*/	GROUP_2 | ADDMODE_RM | WIDTH_BYTE,
	/*[0xD3]*/	GROUP_2 | ADDMODE_RM | WIDTH_DWORD,
	/*[0xD4]*/	INSTR_AAM | ADDMODE_IMM | WIDTH_BYTE,
	/*[0xD5]*/	INSTR_AAD | ADDMODE_IMM | WIDTH_BYTE,
	/*[0xD6]*/	INSTR_UNDEFINED, // SALC undocumented instr, should add this?
	/*[0xD7]*/	INSTR_XLATB | ADDMODE_IMPLIED,
	/*[0xD8]*/	INSTR_UNDEFINED, // fpu
	/*[0xD9]*/	INSTR_UNDEFINED, // fpu
	/*[0xDA]*/	INSTR_UNDEFINED, // fpu
	/*[0xDB]*/	INSTR_UNDEFINED, // fpu
	/*[0xDC]*/	INSTR_UNDEFINED, // fpu
	/*[0xDD]*/	INSTR_UNDEFINED, // fpu
	/*[0xDE]*/	INSTR_UNDEFINED, // fpu
	/*[0xDF]*/	INSTR_UNDEFINED, // fpu
	/*[0xE0]*/	INSTR_LOOPNE | ADDMODE_REL | WIDTH_BYTE,
	/*[0xE1]*/	INSTR_LOOPE | ADDMODE_REL | WIDTH_BYTE,
	/*[0xE2]*/	INSTR_LOOP | ADDMODE_REL | WIDTH_BYTE,
	/*[0xE3]*/	INSTR_JECXZ | Jb,
	/*[0xE4]*/	INSTR_IN | ADDMODE_IMM | WIDTH_BYTE,
	/*[0xE5]*/	INSTR_IN | ADDMODE_IMM8 | WIDTH_DWORD,
	/*[0xE6]*/	INSTR_OUT | ADDMODE_IMM | WIDTH_BYTE,
	/*[0xE7]*/	INSTR_OUT | ADDMODE_IMM8 | WIDTH_DWORD,
	/*[0xE8]*/	INSTR_CALL | Jv,
	/*[0xE9]*/	INSTR_JMP  | Jv,
	/*[0xEA]*/	INSTR_DIFF_SYNTAX | ADDMODE_FAR_PTR | WIDTH_DWORD,
	/*[0xEB]*/	INSTR_JMP  | Jb,
	/*[0xEC]*/	INSTR_IN | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xED]*/	INSTR_IN | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xEE]*/	INSTR_OUT | ADDMODE_IMPLIED | WIDTH_BYTE,
	/*[0xEF]*/	INSTR_OUT | ADDMODE_IMPLIED | WIDTH_DWORD,
	/*[0xF0]*/	0 /* LOCK */,
	/*[0xF1]*/	INSTR_UNDEFINED, // INT1 undocumented instr, should add this?
	/*[0xF2]*/	0 /* REPNZ_PREFIX */,
	/*[0xF3]*/	0 /* REPZ_PREFIX */,
	/*[0xF4]*/	INSTR_HLT | ADDMODE_IMPLIED,
	/*[0xF5]*/	INSTR_CMC | ADDMODE_IMPLIED,
	/*[0xF6]*/	GROUP_3 | ADDMODE_RM | WIDTH_BYTE,
	/*[0xF7]*/	GROUP_3 | ADDMODE_RM | WIDTH_DWORD,
	/*[0xF8]*/	INSTR_CLC | ADDMODE_IMPLIED,
	/*[0xF9]*/	INSTR_STC | ADDMODE_IMPLIED,
	/*[0xFA]*/	INSTR_CLI | ADDMODE_IMPLIED,
	/*[0xFB]*/	INSTR_STI | ADDMODE_IMPLIED,
	/*[0xFC]*/	INSTR_CLD | ADDMODE_IMPLIED,
	/*[0xFD]*/	INSTR_STD | ADDMODE_IMPLIED,
	/*[0xFE]*/	GROUP_4 | ADDMODE_RM | WIDTH_BYTE,
	/*[0xFF]*/	GROUP_5 | ADDMODE_RM | WIDTH_DWORD,
};

static const uint64_t decode_table_two[256] = {
	/*[0x00]*/	GROUP_6 | ADDMODE_RM | WIDTH_WORD,
	/*[0x01]*/	GROUP_7 | ADDMODE_RM | WIDTH_DWORD,
	/*[0x02]*/	INSTR_LAR | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x03]*/	INSTR_LSL | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0x04]*/	INSTR_UNDEFINED,
	/*[0x05]*/	INSTR_UNDEFINED,
	/*[0x06]*/	INSTR_CLTS | ADDMODE_IMPLIED,
	/*[0x07]*/	INSTR_UNDEFINED,
	/*[0x08]*/	INSTR_INVD | ADDMODE_IMPLIED,
	/*[0x09]*/	INSTR_WBINVD | ADDMODE_IMPLIED,
	/*[0x0A]*/	INSTR_UNDEFINED,
	/*[0x0B]*/	INSTR_UD2 | ADDMODE_IMPLIED,
	/*[0x0C]*/	INSTR_UNDEFINED,
	/*[0x0D]*/	INSTR_UNDEFINED,
	/*[0x0E]*/	INSTR_UNDEFINED,
	/*[0x0F]*/	INSTR_UNDEFINED,
	/*[0x10]*/	INSTR_UNDEFINED, // sse
	/*[0x11]*/	INSTR_UNDEFINED, // sse
	/*[0x12]*/	INSTR_UNDEFINED, // sse
	/*[0x13]*/	INSTR_UNDEFINED, // sse
	/*[0x14]*/	INSTR_UNDEFINED, // sse
	/*[0x15]*/	INSTR_UNDEFINED, // sse
	/*[0x16]*/	INSTR_UNDEFINED, // sse
	/*[0x17]*/	INSTR_UNDEFINED, // sse
	/*[0x18]*/	INSTR_UNDEFINED, // sse
	/*[0x19]*/	INSTR_UNDEFINED,
	/*[0x1A]*/	INSTR_UNDEFINED,
	/*[0x1B]*/	INSTR_UNDEFINED,
	/*[0x1C]*/	INSTR_UNDEFINED,
	/*[0x1D]*/	INSTR_UNDEFINED,
	/*[0x1E]*/	INSTR_UNDEFINED,
	/*[0x1F]*/	INSTR_UNDEFINED,
	/*[0x20]*/	INSTR_MOV | ADDMODE_CR_RM | WIDTH_DWORD,
	/*[0x21]*/	INSTR_MOV | ADDMODE_DBG_RM | WIDTH_DWORD,
	/*[0x22]*/	INSTR_MOV | ADDMODE_RM_CR | WIDTH_DWORD,
	/*[0x23]*/	INSTR_MOV | ADDMODE_RM_DBG | WIDTH_DWORD,
	/*[0x24]*/	INSTR_UNDEFINED,
	/*[0x25]*/	INSTR_UNDEFINED,
	/*[0x26]*/	INSTR_UNDEFINED,
	/*[0x27]*/	INSTR_UNDEFINED,
	/*[0x28]*/	INSTR_UNDEFINED, // sse
	/*[0x29]*/	INSTR_UNDEFINED, // sse
	/*[0x2A]*/	INSTR_UNDEFINED, // sse
	/*[0x2B]*/	INSTR_UNDEFINED, // sse
	/*[0x2C]*/	INSTR_UNDEFINED, // sse
	/*[0x2D]*/	INSTR_UNDEFINED, // sse
	/*[0x2E]*/	INSTR_UNDEFINED, // sse
	/*[0x2F]*/	INSTR_UNDEFINED, // sse
	/*[0x30]*/	INSTR_WRMSR | ADDMODE_IMPLIED,
	/*[0x31]*/	INSTR_RDTSC | ADDMODE_IMPLIED,
	/*[0x32]*/	INSTR_RDMSR | ADDMODE_IMPLIED,
	/*[0x33]*/	INSTR_RDPMC | ADDMODE_IMPLIED,
	/*[0x34]*/	INSTR_SYSENTER | ADDMODE_IMPLIED,
	/*[0x35]*/	INSTR_SYSEXIT | ADDMODE_IMPLIED,
	/*[0x36]*/	INSTR_UNDEFINED,
	/*[0x37]*/	INSTR_UNDEFINED,
	/*[0x38]*/	INSTR_UNDEFINED,
	/*[0x39]*/	INSTR_UNDEFINED,
	/*[0x3A]*/	INSTR_UNDEFINED,
	/*[0x3B]*/	INSTR_UNDEFINED,
	/*[0x3C]*/	INSTR_UNDEFINED,
	/*[0x3D]*/	INSTR_UNDEFINED,
	/*[0x3E]*/	INSTR_UNDEFINED,
	/*[0x3F]*/	INSTR_UNDEFINED,
	/*[0x40]*/	INSTR_CMOVO  | Cv,
	/*[0x41]*/	INSTR_CMOVNO | Cv,
	/*[0x42]*/	INSTR_CMOVB  | Cv,
	/*[0x43]*/	INSTR_CMOVNB | Cv,
	/*[0x44]*/	INSTR_CMOVZ  | Cv,
	/*[0x45]*/	INSTR_CMOVNE | Cv,
	/*[0x46]*/	INSTR_CMOVBE | Cv,
	/*[0x47]*/	INSTR_CMOVA  | Cv,
	/*[0x48]*/	INSTR_CMOVS  | Cv,
	/*[0x49]*/	INSTR_CMOVNS | Cv,
	/*[0x4A]*/	INSTR_CMOVPE | Cv,
	/*[0x4B]*/	INSTR_CMOVPO | Cv,
	/*[0x4C]*/	INSTR_CMOVL  | Cv,
	/*[0x4D]*/	INSTR_CMOVGE | Cv,
	/*[0x4E]*/	INSTR_CMOVLE | Cv,
	/*[0x4F]*/	INSTR_CMOVG  | Cv,
	/*[0x50]*/	INSTR_UNDEFINED, // sse
	/*[0x51]*/	INSTR_UNDEFINED, // sse
	/*[0x52]*/	INSTR_UNDEFINED, // sse
	/*[0x53]*/	INSTR_UNDEFINED, // sse
	/*[0x54]*/	INSTR_UNDEFINED, // sse
	/*[0x55]*/	INSTR_UNDEFINED, // sse
	/*[0x56]*/	INSTR_UNDEFINED, // sse
	/*[0x57]*/	INSTR_UNDEFINED, // sse
	/*[0x58]*/	INSTR_UNDEFINED, // sse
	/*[0x59]*/	INSTR_UNDEFINED, // sse
	/*[0x5A]*/	INSTR_UNDEFINED,
	/*[0x5B]*/	INSTR_UNDEFINED,
	/*[0x5C]*/	INSTR_UNDEFINED, // sse
	/*[0x5D]*/	INSTR_UNDEFINED, // sse
	/*[0x5E]*/	INSTR_UNDEFINED, // sse
	/*[0x5F]*/	INSTR_UNDEFINED, // sse
	/*[0x60]*/	INSTR_UNDEFINED, // mmx
	/*[0x61]*/	INSTR_UNDEFINED, // mmx
	/*[0x62]*/	INSTR_UNDEFINED, // mmx
	/*[0x63]*/	INSTR_UNDEFINED, // mmx
	/*[0x64]*/	INSTR_UNDEFINED, // mmx
	/*[0x65]*/	INSTR_UNDEFINED, // mmx
	/*[0x66]*/	INSTR_UNDEFINED, // mmx
	/*[0x67]*/	INSTR_UNDEFINED, // mmx
	/*[0x68]*/	INSTR_UNDEFINED, // mmx
	/*[0x69]*/	INSTR_UNDEFINED, // mmx
	/*[0x6A]*/	INSTR_UNDEFINED, // mmx
	/*[0x6B]*/	INSTR_UNDEFINED, // mmx
	/*[0x6C]*/	INSTR_UNDEFINED,
	/*[0x6D]*/	INSTR_UNDEFINED,
	/*[0x6E]*/	INSTR_UNDEFINED, // mmx
	/*[0x6F]*/	INSTR_UNDEFINED, // mmx
	/*[0x70]*/	INSTR_UNDEFINED, // sse
	/*[0x71]*/	INSTR_UNDEFINED, // mmx
	/*[0x72]*/	INSTR_UNDEFINED, // mmx
	/*[0x73]*/	INSTR_UNDEFINED, // mmx
	/*[0x74]*/	INSTR_UNDEFINED, // mmx
	/*[0x75]*/	INSTR_UNDEFINED, // mmx
	/*[0x76]*/	INSTR_UNDEFINED, // mmx
	/*[0x77]*/	INSTR_UNDEFINED, // mmx
	/*[0x78]*/	INSTR_UNDEFINED,
	/*[0x79]*/	INSTR_UNDEFINED,
	/*[0x7A]*/	INSTR_UNDEFINED,
	/*[0x7B]*/	INSTR_UNDEFINED,
	/*[0x7C]*/	INSTR_UNDEFINED,
	/*[0x7D]*/	INSTR_UNDEFINED,
	/*[0x7E]*/	INSTR_UNDEFINED, // mmx
	/*[0x7F]*/	INSTR_UNDEFINED, // mmx
	/*[0x80]*/	INSTR_JO  | Jv,
	/*[0x81]*/	INSTR_JNO | Jv,
	/*[0x82]*/	INSTR_JB  | Jv,
	/*[0x83]*/	INSTR_JNB | Jv,
	/*[0x84]*/	INSTR_JZ  | Jv,
	/*[0x85]*/	INSTR_JNE | Jv,
	/*[0x86]*/	INSTR_JBE | Jv,
	/*[0x87]*/	INSTR_JA  | Jv,
	/*[0x88]*/	INSTR_JS  | Jv,
	/*[0x89]*/	INSTR_JNS | Jv,
	/*[0x8A]*/	INSTR_JPE | Jv,
	/*[0x8B]*/	INSTR_JPO | Jv,
	/*[0x8C]*/	INSTR_JL  | Jv,
	/*[0x8D]*/	INSTR_JGE | Jv,
	/*[0x8E]*/	INSTR_JLE | Jv,
	/*[0x8F]*/	INSTR_JG  | Jv,
	/*[0x90]*/	INSTR_SETO  | Sb,
	/*[0x91]*/	INSTR_SETNO | Sb,
	/*[0x92]*/	INSTR_SETB  | Sb,
	/*[0x93]*/	INSTR_SETNB | Sb,
	/*[0x94]*/	INSTR_SETZ  | Sb,
	/*[0x95]*/	INSTR_SETNE | Sb,
	/*[0x96]*/	INSTR_SETBE | Sb,
	/*[0x97]*/	INSTR_SETA  | Sb,
	/*[0x98]*/	INSTR_SETS  | Sb,
	/*[0x99]*/	INSTR_SETNS | Sb,
	/*[0x9A]*/	INSTR_SETPE | Sb,
	/*[0x9B]*/	INSTR_SETPO | Sb,
	/*[0x9C]*/	INSTR_SETL  | Sb,
	/*[0x9D]*/	INSTR_SETGE | Sb,
	/*[0x9E]*/	INSTR_SETLE | Sb,
	/*[0x9F]*/	INSTR_SETG  | Sb,
	/*[0xA0]*/	INSTR_PUSH | ADDMODE_SEG3_REG /* FS */ | WIDTH_DWORD,
	/*[0xA1]*/	INSTR_POP | ADDMODE_SEG3_REG /* FS */ | WIDTH_DWORD,
	/*[0xA2]*/	INSTR_CPUID | ADDMODE_IMPLIED,
	/*[0xA3]*/	INSTR_BT | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0xA4]*/	INSTR_SHLD | ADDMODE_REG_IMM8_RM | WIDTH_DWORD,
	/*[0xA5]*/	INSTR_SHLD | ADDMODE_REG_CL_RM | WIDTH_DWORD,
	/*[0xA6]*/	INSTR_UNDEFINED,
	/*[0xA7]*/	INSTR_UNDEFINED,
	/*[0xA8]*/	INSTR_PUSH | ADDMODE_SEG3_REG /* GS */ | WIDTH_DWORD,
	/*[0xA9]*/	INSTR_POP | ADDMODE_SEG3_REG /* GS */ | WIDTH_DWORD,
	/*[0xAA]*/	INSTR_RSM | ADDMODE_IMPLIED,
	/*[0xAB]*/	INSTR_BTS | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0xAC]*/	INSTR_SHRD | ADDMODE_REG_IMM8_RM | WIDTH_DWORD,
	/*[0xAD]*/	INSTR_SHRD | ADDMODE_REG_CL_RM | WIDTH_DWORD,
	/*[0xAE]*/	INSTR_UNDEFINED, // fpu, sse
	/*[0xAF]*/	INSTR_IMUL | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xB0]*/	INSTR_CMPXCHG | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0xB1]*/	INSTR_CMPXCHG | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0xB2]*/	INSTR_LSS | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xB3]*/	INSTR_BTR | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0xB4]*/	INSTR_LFS | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xB5]*/	INSTR_LGS | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xB6]*/	INSTR_DIFF_SYNTAX | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xB7]*/	INSTR_DIFF_SYNTAX | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xB8]*/	INSTR_UNDEFINED,
	/*[0xB9]*/	INSTR_UD1 | ADDMODE_IMPLIED,
	/*[0xBA]*/	GROUP_8 | ADDMODE_IMM8_RM | WIDTH_DWORD,
	/*[0xBB]*/	INSTR_BTC | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0xBC]*/	INSTR_BSF | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xBD]*/	INSTR_BSR | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xBE]*/	INSTR_DIFF_SYNTAX | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xBF]*/	INSTR_DIFF_SYNTAX | ADDMODE_RM_REG | WIDTH_DWORD,
	/*[0xC0]*/	INSTR_XADD | ADDMODE_REG_RM | WIDTH_BYTE,
	/*[0xC1]*/	INSTR_XADD | ADDMODE_REG_RM | WIDTH_DWORD,
	/*[0xC2]*/	INSTR_UNDEFINED, // sse
	/*[0xC3]*/	INSTR_UNDEFINED,
	/*[0xC4]*/	INSTR_UNDEFINED, // sse
	/*[0xC5]*/	INSTR_UNDEFINED, // sse
	/*[0xC6]*/	INSTR_UNDEFINED, // sse
	/*[0xC7]*/	GROUP_9 | ADDMODE_RM | WIDTH_QWORD,
	/*[0xC8]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xC9]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xCA]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xCB]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xCC]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xCD]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xCE]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xCF]*/	INSTR_BSWAP | ADDMODE_REG | WIDTH_DWORD,
	/*[0xD0]*/	INSTR_UNDEFINED,
	/*[0xD1]*/	INSTR_UNDEFINED, // mmx
	/*[0xD2]*/	INSTR_UNDEFINED, // mmx
	/*[0xD3]*/	INSTR_UNDEFINED, // mmx
	/*[0xD4]*/	INSTR_UNDEFINED,
	/*[0xD5]*/	INSTR_UNDEFINED, // mmx
	/*[0xD6]*/	INSTR_UNDEFINED,
	/*[0xD7]*/	INSTR_UNDEFINED, // sse
	/*[0xD8]*/	INSTR_UNDEFINED, // mmx
	/*[0xD9]*/	INSTR_UNDEFINED, // mmx
	/*[0xDA]*/	INSTR_UNDEFINED, // sse
	/*[0xDB]*/	INSTR_UNDEFINED, // mmx
	/*[0xDC]*/	INSTR_UNDEFINED, // mmx
	/*[0xDD]*/	INSTR_UNDEFINED, // mmx
	/*[0xDE]*/	INSTR_UNDEFINED, // sse
	/*[0xDF]*/	INSTR_UNDEFINED, // mmx
	/*[0xE0]*/	INSTR_UNDEFINED, // sse
	/*[0xE1]*/	INSTR_UNDEFINED, // mmx
	/*[0xE2]*/	INSTR_UNDEFINED, // mmx
	/*[0xE3]*/	INSTR_UNDEFINED, // sse
	/*[0xE4]*/	INSTR_UNDEFINED, // sse
	/*[0xE5]*/	INSTR_UNDEFINED, // mmx
	/*[0xE6]*/	INSTR_UNDEFINED,
	/*[0xE7]*/	INSTR_UNDEFINED, // sse
	/*[0xE8]*/	INSTR_UNDEFINED, // mmx
	/*[0xE9]*/	INSTR_UNDEFINED, // mmx
	/*[0xEA]*/	INSTR_UNDEFINED, // sse
	/*[0xEB]*/	INSTR_UNDEFINED, // mmx
	/*[0xEC]*/	INSTR_UNDEFINED, // mmx
	/*[0xED]*/	INSTR_UNDEFINED, // mmx
	/*[0xEE]*/	INSTR_UNDEFINED, // sse
	/*[0xEF]*/	INSTR_UNDEFINED, // mmx
	/*[0xF0]*/	INSTR_UNDEFINED,
	/*[0xF1]*/	INSTR_UNDEFINED, // mmx
	/*[0xF2]*/	INSTR_UNDEFINED, // mmx
	/*[0xF3]*/	INSTR_UNDEFINED, // mmx
	/*[0xF4]*/	INSTR_UNDEFINED,
	/*[0xF5]*/	INSTR_UNDEFINED, // mmx
	/*[0xF6]*/	INSTR_UNDEFINED, // sse
	/*[0xF7]*/	INSTR_UNDEFINED, // sse
	/*[0xF8]*/	INSTR_UNDEFINED, // mmx
	/*[0xF9]*/	INSTR_UNDEFINED, // mmx
	/*[0xFA]*/	INSTR_UNDEFINED, // mmx
	/*[0xFB]*/	INSTR_UNDEFINED,
	/*[0xFC]*/	INSTR_UNDEFINED, // mmx
	/*[0xFD]*/	INSTR_UNDEFINED, // mmx
	/*[0xFE]*/	INSTR_UNDEFINED, // mmx
	/*[0xFF]*/	INSTR_UNDEFINED,
};

static const uint32_t grp1_decode_table[8] = {
	/*[0x00]*/	INSTR_ADD,
	/*[0x01]*/	INSTR_OR,
	/*[0x02]*/	INSTR_ADC,
	/*[0x03]*/	INSTR_SBB,
	/*[0x04]*/	INSTR_AND,
	/*[0x05]*/	INSTR_SUB,
	/*[0x06]*/	INSTR_XOR,
	/*[0x07]*/	INSTR_CMP,
};

static const uint32_t grp2_decode_table[8] = {
	/*[0x00]*/	INSTR_ROL,
	/*[0x01]*/	INSTR_ROR,
	/*[0x02]*/	INSTR_RCL,
	/*[0x03]*/	INSTR_RCR,
	/*[0x04]*/	INSTR_SHL,
	/*[0x05]*/	INSTR_SHR,
	/*[0x06]*/	0,
	/*[0x07]*/	INSTR_SAR,
};

static const uint32_t grp3_decode_table[8] = {
	/*[0x00]*/	INSTR_TEST,
	/*[0x01]*/	0,
	/*[0x02]*/	INSTR_NOT,
	/*[0x03]*/	INSTR_NEG,
	/*[0x04]*/	INSTR_MUL,
	/*[0x05]*/	INSTR_IMUL,
	/*[0x06]*/	INSTR_DIV,
	/*[0x07]*/	INSTR_IDIV,
};

static const uint32_t grp4_decode_table[8] = {
	/*[0x00]*/	INSTR_INC,
	/*[0x01]*/	INSTR_DEC,
	/*[0x02]*/	0,
	/*[0x03]*/	0,
	/*[0x04]*/	0,
	/*[0x05]*/	0,
	/*[0x06]*/	0,
	/*[0x07]*/	0,
};

static const uint32_t grp5_decode_table[8] = {
	/*[0x00]*/	INSTR_INC,
	/*[0x01]*/	INSTR_DEC,
	/*[0x02]*/	INSTR_CALL,
	/*[0x03]*/	INSTR_DIFF_SYNTAX,
	/*[0x04]*/	INSTR_JMP,
	/*[0x05]*/	INSTR_DIFF_SYNTAX,
	/*[0x06]*/	INSTR_PUSH,
	/*[0x07]*/	0,
};

static const uint32_t grp6_decode_table[8] = {
	/*[0x00]*/	INSTR_SLDT,
	/*[0x01]*/	INSTR_STR,
	/*[0x02]*/	INSTR_LLDT,
	/*[0x03]*/	INSTR_LTR,
	/*[0x04]*/	INSTR_VERR,
	/*[0x05]*/	INSTR_VERW,
	/*[0x06]*/	0,
	/*[0x07]*/	0,
};

static const uint32_t grp7_decode_table[8] = {
	/*[0x00]*/	INSTR_DIFF_SYNTAX,
	/*[0x01]*/	INSTR_DIFF_SYNTAX,
	/*[0x02]*/	INSTR_DIFF_SYNTAX,
	/*[0x03]*/	INSTR_DIFF_SYNTAX,
	/*[0x04]*/	INSTR_SMSW,
	/*[0x05]*/	0,
	/*[0x06]*/	INSTR_LMSW,
	/*[0x07]*/	INSTR_INVLPG,
};

static const uint32_t grp8_decode_table[8] = {
	/*[0x00]*/	0,
	/*[0x01]*/	0,
	/*[0x02]*/	0,
	/*[0x03]*/	0,
	/*[0x04]*/	INSTR_BT,
	/*[0x05]*/	INSTR_BTS,
	/*[0x06]*/	INSTR_BTR,
	/*[0x07]*/	INSTR_BTC,
};

static const uint32_t grp9_decode_table[8] = {
	/*[0x00]*/	0,
	/*[0x01]*/	INSTR_CMPXCHG8B,
	/*[0x02]*/	0,
	/*[0x03]*/	0,
	/*[0x04]*/	0,
	/*[0x05]*/	0,
	/*[0x06]*/	0,
	/*[0x07]*/	0,
};

static void
decode_third_operand(struct x86_instr *instr)
{
	struct x86_operand *operand = &instr->operand[OPNUM_THIRD];

	switch (instr->flags & OP3_MASK) {
	case OP3_NONE:
		break;
	case OP3_IMM:
	case OP3_IMM8:
		operand->type	= OPTYPE_IMM;
		operand->imm	= instr->imm_data[0];
		break;
	case OP3_CL:
		operand->type	= OPTYPE_REG8;
		operand->reg	= 1; /* CL */
		break;
	default:
		break;
	}
}

static uint8_t
decode_dst_reg(struct x86_instr *instr)
{
	if (!(instr->flags & MOD_RM))
		return instr->opcode & 0x07;

	if (instr->flags & DIR_REVERSED)
		return instr->rm;

	return instr->reg_opc;
}

static uint8_t
decode_dst_mem(struct x86_instr *instr)
{
	if (instr->flags & DIR_REVERSED)
		return instr->rm;

	return instr->reg_opc;
}

static void
decode_dst_operand(struct x86_instr *instr)
{
	struct x86_operand *operand = &instr->operand[OPNUM_DST];

	switch (instr->flags & DST_MASK) {
	case DST_NONE:
		break;
	case DST_IMM16:
		operand->type	= OPTYPE_IMM;
		operand->imm	= instr->imm_data[1];
		break;
	case DST_REG:
		operand->type	= OPTYPE_REG;
		operand->reg	= decode_dst_reg(instr);
		break;
	case DST_SEG3_REG:
		operand->type	= OPTYPE_SEG_REG;
		operand->reg	= instr->reg_opc;
		break;
	case DST_CR_REG:
		operand->type	= OPTYPE_CR_REG;
		operand->reg	= instr->reg_opc;
		break;
	case DST_DBG_REG:
		operand->type	= OPTYPE_DBG_REG;
		operand->reg	= instr->reg_opc;
		break;
	case DST_ACC:
		operand->type	= OPTYPE_REG;
		operand->reg	= 0; /* AL/AX/EAX */
		break;
	case DST_MOFFSET:
		operand->type	= OPTYPE_MOFFSET;
		operand->disp	= instr->disp;
		break;
	case DST_MEM:
		if (instr->flags & SIB) {
			operand->type = OPTYPE_SIB_MEM;
		}
		else {
			operand->type = OPTYPE_MEM;
			operand->reg = decode_dst_mem(instr);
		}
		break;
	case DST_MEM_DISP_BYTE:
	case DST_MEM_DISP_WORD:
	case DST_MEM_DISP_FULL:
		if (instr->flags & SIB) {
			operand->type = OPTYPE_SIB_DISP;
			operand->disp = instr->disp;
		}
		else {
			operand->type = OPTYPE_MEM_DISP;
			operand->reg = instr->rm;
			operand->disp = instr->disp;
		}
		break;
	default:
		break;
	}
}

static uint8_t
decode_src_reg(struct x86_instr *instr)
{
	if (!(instr->flags & MOD_RM))
		return instr->opcode & 0x07;

	if (instr->flags & DIR_REVERSED)
		return instr->reg_opc;

	return instr->rm;
}

static uint8_t
decode_src_mem(struct x86_instr* instr)
{
	if (instr->flags & DIR_REVERSED)
		return instr->reg_opc;

	return instr->rm;
}

static void
decode_src_operand(struct x86_instr *instr)
{
	struct x86_operand *operand = &instr->operand[OPNUM_SRC];

	switch (instr->flags & SRC_MASK) {
	case SRC_NONE:
		break;
	case SRC_REL:
		operand->type	= OPTYPE_REL;
		operand->rel	= instr->rel_data[0];
		break;
	case SRC_IMM:
	case SRC_IMM8:
		operand->type	= OPTYPE_IMM;
		operand->imm	= instr->imm_data[0];
		break;
	case SRC_IMM48:
		operand->type	= OPTYPE_FAR_PTR;
		operand->imm	= instr->imm_data[0];
		operand->seg_sel = instr->imm_data[1];
		break;
	case SRC_REG:
		operand->type	= OPTYPE_REG;
		operand->reg	= decode_src_reg(instr);
		break;
	case SRC_SEG2_REG:
		operand->type	= OPTYPE_SEG_REG;
		operand->reg	= instr->opcode >> 3;
		break;
	case SRC_SEG3_REG:
		operand->type	= OPTYPE_SEG_REG;
		if (instr->flags & MOD_RM) {
			operand->reg = instr->reg_opc;
		}
		else {
			operand->reg = (instr->opcode & 0x38) >> 3;
		}
		break;
	case SRC_CR_REG:
		operand->type	= OPTYPE_CR_REG;
		operand->reg	= instr->reg_opc;
		break;
	case SRC_DBG_REG:
		operand->type	= OPTYPE_DBG_REG;
		operand->reg	= instr->reg_opc;
		break;
	case SRC_ACC:
		operand->type	= OPTYPE_REG;
		operand->reg	= 0; /* AL/AX/EAX */
		break;
	case SRC_MOFFSET:
		operand->type	= OPTYPE_MOFFSET;
		operand->disp	= instr->disp;
		break;
	case SRC_MEM:
		if (instr->flags & SIB) {
			operand->type = OPTYPE_SIB_MEM;
			operand->disp = instr->disp;
		}
		else {
			operand->type = OPTYPE_MEM;
			operand->reg = decode_src_mem(instr);
		}
		break;
	case SRC_MEM_DISP_BYTE:
	case SRC_MEM_DISP_WORD:
	case SRC_MEM_DISP_FULL:
		if (instr->flags & SIB) {
			operand->type = OPTYPE_SIB_DISP;
			operand->disp = instr->disp;
		}
		else {
			operand->type = OPTYPE_MEM_DISP;
			operand->reg = instr->rm;
			operand->disp = instr->disp;
		}
		break;
	default:
		break;
	}
}

static uint8_t read_u8(uint8_t* RAM, addr_t *pc)
{
	addr_t new_pc = *pc;

	uint8_t ret = (uint8_t)RAM[new_pc++];

	*pc = new_pc;

	return ret;
}

static int8_t read_s8(uint8_t* RAM, addr_t *pc)
{
	addr_t new_pc = *pc;

	int8_t ret = (int8_t)RAM[new_pc++];

	*pc = new_pc;

	return ret;
}

static uint16_t read_u16(uint8_t* RAM, addr_t *pc)
{
	addr_t new_pc = *pc;

	uint8_t lo = RAM[new_pc++];
	uint8_t hi = RAM[new_pc++];

	uint16_t ret = (uint16_t)((hi << 8) | lo);

	*pc = new_pc;

	return ret;
}

static int16_t read_s16(uint8_t* RAM, addr_t *pc)
{
	addr_t new_pc = *pc;

	uint8_t lo = RAM[new_pc++];
	uint8_t hi = RAM[new_pc++];

	int16_t ret = (int16_t)((hi << 8) | lo);

	*pc = new_pc;

	return ret;
}

static uint32_t read_u32(uint8_t* RAM, addr_t* pc)
{
	addr_t new_pc = *pc;

	uint8_t byte1 = RAM[new_pc++];
	uint8_t byte2 = RAM[new_pc++];
	uint8_t byte3 = RAM[new_pc++];
	uint8_t byte4 = RAM[new_pc++];

	uint32_t ret = (uint32_t)((byte4 << 24) | (byte3 << 16) | (byte2 << 8) | byte1);

	*pc = new_pc;

	return ret;
}

static int32_t read_s32(uint8_t* RAM, addr_t* pc)
{
	addr_t new_pc = *pc;

	uint8_t byte1 = RAM[new_pc++];
	uint8_t byte2 = RAM[new_pc++];
	uint8_t byte3 = RAM[new_pc++];
	uint8_t byte4 = RAM[new_pc++];

	int32_t ret = (int32_t)((byte4 << 24) | (byte3 << 16) | (byte2 << 8) | byte1);

	*pc = new_pc;

	return ret;
}

static void
decode_imm(struct x86_instr *instr, uint8_t* RAM, addr_t *pc)
{
	switch (instr->flags & (SRC_IMM8 | SRC_IMM48 | OP3_IMM_MASK | DST_IMM16)) {
	case SRC_IMM8:
		instr->imm_data[0] = read_u8(RAM, pc);
		return;
	case SRC_IMM48: // far JMP and far CALL instr
		if (instr->flags & WIDTH_DWORD) {
			instr->imm_data[0] = read_u32(RAM, pc);
		}
		else {
			instr->imm_data[0] = read_u16(RAM, pc);
		}
		instr->imm_data[1] = read_u16(RAM, pc);
		return;
	case SRC_IMM8|DST_IMM16: // ENTER instr
		instr->imm_data[1] = read_u16(RAM, pc);
		instr->imm_data[0] = read_u8(RAM, pc);
		return;
	case OP3_IMM8:
		instr->imm_data[0] = read_u8(RAM, pc);
		return;
	case OP3_IMM:
		if (instr->flags & WIDTH_DWORD) {
			instr->imm_data[0] = read_u32(RAM, pc);
		}
		else {
			instr->imm_data[0] = read_u16(RAM, pc);
		}
		return;
	default:
		break;
	}

	switch (instr->flags & WIDTH_MASK) {
	case WIDTH_DWORD:
		instr->imm_data[0] = read_u32(RAM, pc);
		break;
	case WIDTH_WORD:
		instr->imm_data[0] = read_u16(RAM, pc);
		break;
	case WIDTH_BYTE:
		instr->imm_data[0] = read_u8(RAM, pc);
		break;
	default:
		break;
	}
}

static void
decode_rel(struct x86_instr *instr, uint8_t* RAM, addr_t *pc)
{
	switch (instr->flags & WIDTH_MASK) {
	case WIDTH_DWORD:
		instr->rel_data[0] = read_s32(RAM, pc);
		break;
	case WIDTH_WORD:
		instr->rel_data[0] = read_s16(RAM, pc);
		break;
	case WIDTH_BYTE:
		instr->rel_data[0] = read_s8(RAM, pc);
		break;
	default:
		break;
	}
}

static void
decode_moffset(struct x86_instr *instr, uint8_t* RAM, addr_t *pc)
{
	if (instr->addr_size_override == 0) {
		instr->disp = read_u32(RAM, pc);
	}
	else {
		instr->disp = read_u16(RAM, pc);
	}
}

static void
decode_disp(struct x86_instr *instr, uint8_t* RAM, addr_t *pc)
{
	switch (instr->flags & MEM_DISP_MASK) {
	case SRC_MEM_DISP_FULL:
	case DST_MEM_DISP_FULL:
		instr->disp = read_s32(RAM, pc);
		break;
	case SRC_MEM_DISP_WORD:
	case DST_MEM_DISP_WORD:
		instr->disp	= read_s16(RAM, pc);
		break;
	case SRC_MEM_DISP_BYTE:
	case DST_MEM_DISP_BYTE:
		instr->disp	= read_s8(RAM, pc);
		break;
	}
}

static const uint64_t sib_dst_decode[] = {
	/*[0x00]*/	DST_MEM_DISP_FULL,
	/*[0x01]*/	DST_MEM_DISP_BYTE,
	/*[0x02]*/	DST_MEM_DISP_FULL,
};

static const uint64_t sib_src_decode[] = {
	/*[0x00]*/	SRC_MEM_DISP_FULL,
	/*[0x01]*/	SRC_MEM_DISP_BYTE,
	/*[0x02]*/	SRC_MEM_DISP_FULL,
};

static void
decode_sib_byte(struct x86_instr *instr, uint8_t sib)
{
	instr->scale = (sib & 0xc0) >> 6;
	instr->idx = (sib & 0x38) >> 3;
	instr->base = (sib & 0x07);

	if (instr->base == 5) {
		if (instr->flags & DIR_REVERSED) {
			instr->flags &= ~DST_MEM;
			instr->flags |= sib_dst_decode[instr->mod];
		}
		else {
			instr->flags &= ~SRC_MEM;
			instr->flags |= sib_src_decode[instr->mod];
		}
	}
}

static const uint64_t mod_dst_decode[] = {
	/*[0x00]*/	DST_MEM,
	/*[0x01]*/	DST_MEM_DISP_BYTE,
	/*[0x02]*/	0,
	/*[0x03]*/	DST_REG,
};

static const uint64_t mod_src_decode[] = {
	/*[0x00]*/	SRC_MEM,
	/*[0x01]*/	SRC_MEM_DISP_BYTE,
	/*[0x02]*/	0,
	/*[0x03]*/	SRC_REG,
};

static void
decode_modrm_fields(struct x86_instr *instr, uint8_t modrm)
{
	instr->mod = (modrm & 0xc0) >> 6;
	instr->reg_opc = (modrm & 0x38) >> 3;
	instr->rm = (modrm & 0x07);
}

static void
decode_modrm_addr_modes(struct x86_instr *instr)
{
	if (instr->flags & DIR_REVERSED) {
		instr->flags |= mod_dst_decode[instr->mod];
		switch ((instr->addr_size_override << 16) | (instr->mod << 8) | instr->rm) {
		case 512:   // 0, 2, 0
		case 513:   // 0, 2, 1
		case 514:   // 0, 2, 2
		case 515:   // 0, 2, 3
		case 517:   // 0, 2, 5
		case 518:   // 0, 2, 6
		case 519:   // 0, 2, 7
			instr->flags |= DST_MEM_DISP_FULL;
			break;
		case 66048: // 1, 2, 0
		case 66049: // 1, 2, 1
		case 66050: // 1, 2, 2
		case 66051: // 1, 2, 3
		case 66052: // 1, 2, 4
		case 66053: // 1, 2, 5
		case 66054: // 1, 2, 6
		case 66055: // 1, 2, 7
			instr->flags |= DST_MEM_DISP_WORD;
			break;
		case 4:     // 0, 0, 4
		case 260:   // 0, 1, 4
			instr->flags |= SIB;
			break;
		case 516:   // 0, 2, 4
			instr->flags |= (DST_MEM_DISP_FULL | SIB);
			break;
		case 5:     // 0, 0, 5
			instr->flags &= ~DST_MEM;
			instr->flags |= DST_MEM_DISP_FULL;
			break;
		case 65542: // 1, 0, 6
			instr->flags &= ~DST_MEM;
			instr->flags |= DST_MEM_DISP_WORD;
			break;
		default:
			break;
		}
	}
	else {
		instr->flags |= mod_src_decode[instr->mod];
		switch ((instr->addr_size_override << 16) | (instr->mod << 8) | instr->rm) {
		case 512:   // 0, 2, 0
		case 513:   // 0, 2, 1
		case 514:   // 0, 2, 2
		case 515:   // 0, 2, 3
		case 517:   // 0, 2, 5
		case 518:   // 0, 2, 6
		case 519:   // 0, 2, 7
			instr->flags |= SRC_MEM_DISP_FULL;
			break;
		case 66048: // 1, 2, 0
		case 66049: // 1, 2, 1
		case 66050: // 1, 2, 2
		case 66051: // 1, 2, 3
		case 66052: // 1, 2, 4
		case 66053: // 1, 2, 5
		case 66054: // 1, 2, 6
		case 66055: // 1, 2, 7
			instr->flags |= SRC_MEM_DISP_WORD;
			break;
		case 4:     // 0, 0, 4
		case 260:   // 0, 1, 4
			instr->flags |= SIB;
			break;
		case 516:   // 0, 2, 4
			instr->flags |= (SRC_MEM_DISP_FULL | SIB);
			break;
		case 5:     // 0, 0, 5
			instr->flags &= ~SRC_MEM;
			instr->flags |= SRC_MEM_DISP_FULL;
			break;
		case 65542: // 1, 0, 6
			instr->flags &= ~SRC_MEM;
			instr->flags |= SRC_MEM_DISP_WORD;
			break;
		default:
			break;
		}
	}
}

int
arch_x86_decode_instr(struct x86_instr *instr, uint8_t* RAM, addr_t pc, char use_intel)
{
	uint64_t decode;
	uint8_t opcode;
	addr_t start_pc;

	start_pc = pc;

	/* Prefixes */
	instr->seg_override	= NO_OVERRIDE;
	instr->rep_prefix	= NO_PREFIX;
	instr->addr_size_override = 0;
	instr->op_size_override = 0;
	instr->lock_prefix	= 0;
	instr->is_two_byte_instr = 0;

	for (;;) {
		switch (opcode = RAM[pc++]) {
		case 0x26:
			instr->seg_override	= ES_OVERRIDE;
			break;
		case 0x2E:
			instr->seg_override	= CS_OVERRIDE;
			break;
		case 0x36:
			instr->seg_override	= SS_OVERRIDE;
			break;
		case 0x3E:
			instr->seg_override	= DS_OVERRIDE;
			break;
		case 0x64:
			instr->seg_override = FS_OVERRIDE;
			break;
		case 0x65:
			instr->seg_override = GS_OVERRIDE;
			break;
		case 0x66:
			instr->op_size_override = 1;
			if (instr->flags & WIDTH_DWORD) {
				instr->flags &= ~WIDTH_DWORD;
				instr->flags |= WIDTH_WORD;
			}
			else if (instr->flags & WIDTH_QWORD) {
				instr->flags &= ~WIDTH_QWORD;
				instr->flags |= WIDTH_DWORD;
			}
			break;
		case 0x67:
			instr->addr_size_override = 1;
			break;
		case 0xF0:	/* LOCK */
			instr->lock_prefix	= 1;
			break;
		case 0xF2:	/* REPNE/REPNZ */
			instr->rep_prefix	= REPNZ_PREFIX;
			break;
		case 0xF3:	/* REP/REPE/REPZ */
			instr->rep_prefix	= REPZ_PREFIX;
			break;
		default:
			goto done_prefixes;
		}
	}

done_prefixes:

	/* Opcode byte */
	if (opcode == 0x0F) {
		opcode = RAM[pc++];
		decode = decode_table_two[opcode];
		instr->is_two_byte_instr = 1;
	}
	else {
		decode = decode_table_one[opcode];
	}

	instr->opcode	= opcode;
	instr->type	= decode & X86_INSTR_TYPE_MASK;
	instr->flags	= decode & ~X86_INSTR_TYPE_MASK;

	if (instr->flags == 0) /* Unrecognized? */
		return -1;

	if (instr->type == 0) {
		switch ((use_intel << 16) | (instr->op_size_override << 8) | instr->opcode) {
		case 0x62:
			instr->type = INSTR_BOUND;
			instr->flags |= WIDTH_DWORD;
			break;
		case 0x98:
			instr->type = INSTR_CWTL;
			break;
		case 0x99:
			instr->type = INSTR_CLTD;
			break;
		case 0x9A:
		case 0x9A | OP_SIZE_OVERRIDE:
			instr->type = INSTR_LCALL;
			break;
		case 0xB6:
		case 0xB6 | OP_SIZE_OVERRIDE:
			instr->type = INSTR_MOVZXB;
			break;
		case 0xB7:
		case 0xB7 | OP_SIZE_OVERRIDE:
			instr->type = INSTR_MOVZXW;
			break;
		case 0xBE:
		case 0xBE | OP_SIZE_OVERRIDE:
			instr->type = INSTR_MOVSXB;
			break;
		case 0xBF:
		case 0xBF | OP_SIZE_OVERRIDE:
			instr->type = INSTR_MOVSXW;
			break;
		case 0xCA:
		case 0xCB:
		case 0xCA | OP_SIZE_OVERRIDE:
		case 0xCB | OP_SIZE_OVERRIDE:
			instr->type = INSTR_LRET;
			break;
		case 0xEA:
		case 0xEA | OP_SIZE_OVERRIDE:
			instr->type = INSTR_LJMP;
			break;
		case 0x62 | OP_SIZE_OVERRIDE:
			instr->type = INSTR_BOUND;
			instr->flags |= WIDTH_WORD;
			break;
		case 0x98 | OP_SIZE_OVERRIDE:
			instr->type = INSTR_CBTV;
			break;
		case 0x99 | OP_SIZE_OVERRIDE:
			instr->type = INSTR_CWTD;
			break;
		case 0x62 | USE_INTEL:
			instr->type = INSTR_BOUND;
			instr->flags |= WIDTH_QWORD;
			break;
		case 0x98 | USE_INTEL:
			instr->type = INSTR_CWDE;
			break;
		case 0x99 | USE_INTEL:
			instr->type = INSTR_CDQ;
			break;
		case 0x9A | USE_INTEL:
		case 0x9A | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_CALL;
			break;
		case 0xB6 | USE_INTEL:
		case 0xB7 | USE_INTEL:
		case 0xB6 | OP_SIZE_OVERRIDE | USE_INTEL:
		case 0xB7 | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_MOVZX;
			break;
		case 0xBE | USE_INTEL:
		case 0xBF | USE_INTEL:
		case 0xBE | OP_SIZE_OVERRIDE | USE_INTEL:
		case 0xBF | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_MOVSX;
			break;
		case 0xCA | USE_INTEL:
		case 0xCB | USE_INTEL:
		case 0xCA | OP_SIZE_OVERRIDE | USE_INTEL:
		case 0xCB | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_RETF;
			break;
		case 0xEA | USE_INTEL:
		case 0xEA | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_JMP;
			break;
		case 0x62 | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_BOUND;
			instr->flags |= WIDTH_DWORD;
			break;
		case 0x98 | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_CBW;
			break;
		case 0x99 | OP_SIZE_OVERRIDE | USE_INTEL:
			instr->type = INSTR_CWD;
			break;
		default:
			decode_modrm_fields(instr, RAM[pc++]);
			/* Opcode groups */
			switch (instr->flags & GROUP_MASK) {
			case GROUP_1:
				instr->type = grp1_decode_table[instr->reg_opc];
				break;
			case GROUP_2:
				instr->type = grp2_decode_table[instr->reg_opc];
				break;
			case GROUP_3:
				instr->type = grp3_decode_table[instr->reg_opc];
				if (instr->type == INSTR_TEST) {
					instr->flags &= ~ADDMODE_RM;
					instr->flags |= ADDMODE_IMM_RM;
				}
				break;
			case GROUP_4:
				instr->type = grp4_decode_table[instr->reg_opc];
				break;
			case GROUP_5:
				instr->type = grp5_decode_table[instr->reg_opc];
				switch (instr->type) {
				case INSTR_DIFF_SYNTAX:
					switch ((use_intel << 16) | instr->reg_opc) {
						case 0x3:
							instr->type = INSTR_LCALL;
							break;
						case 0x5:
							instr->type = INSTR_LJMP;
							break;
						case 0x3 | USE_INTEL:
							instr->type = INSTR_CALL;
							break;
						case 0x5 | USE_INTEL:
							instr->type = INSTR_JMP;
							break;
						default:
							break;
					}
				}
				break;
			case GROUP_6:
				instr->type = grp6_decode_table[instr->reg_opc];
				break;
			case GROUP_7:
				instr->type = grp7_decode_table[instr->reg_opc];
				switch (instr->type) {
				case INSTR_DIFF_SYNTAX:
					switch ((use_intel << 16) | (instr->op_size_override << 8) | instr->reg_opc) {
					case 0x0:
						instr->type = INSTR_SGDTL;
						break;
					case 0x1:
						instr->type = INSTR_SIDTL;
						break;
					case 0x2:
						instr->type = INSTR_LGDTL;
						break;
					case 0x3:
						instr->type = INSTR_LIDTL;
						break;
					case 0x0 | OP_SIZE_OVERRIDE:
						instr->type = INSTR_SGDTW;
						break;
					case 0x1 | OP_SIZE_OVERRIDE:
						instr->type = INSTR_SIDTW;
						break;
					case 0x2 | OP_SIZE_OVERRIDE:
						instr->type = INSTR_LGDTW;
						break;
					case 0x3 | OP_SIZE_OVERRIDE:
						instr->type = INSTR_LIDTW;
						break;
					case 0x0 | USE_INTEL:
						instr->type = INSTR_SGDTD;
						break;
					case 0x1 | USE_INTEL:
						instr->type = INSTR_SIDTD;
						break;
					case 0x2 | USE_INTEL:
						instr->type = INSTR_LGDTD;
						break;
					case 0x3 | USE_INTEL:
						instr->type = INSTR_LIDTD;
						break;
					case 0x0 | OP_SIZE_OVERRIDE | USE_INTEL:
						instr->type = INSTR_SGDTW;
						break;
					case 0x1 | OP_SIZE_OVERRIDE | USE_INTEL:
						instr->type = INSTR_SIDTW;
						break;
					case 0x2 | OP_SIZE_OVERRIDE | USE_INTEL:
						instr->type = INSTR_LGDTW;
						break;
					case 0x3 | OP_SIZE_OVERRIDE | USE_INTEL:
						instr->type = INSTR_LIDTW;
						break;
					default:
						break;
					}
					break;
				case INSTR_LMSW:
				case INSTR_SMSW:
					instr->flags &= ~WIDTH_DWORD;
					instr->flags |= WIDTH_WORD;
					break;
				case INSTR_INVLPG:
					instr->flags &= ~WIDTH_DWORD;
					instr->flags |= WIDTH_BYTE;
					break;
				default:
					break;
				}
				break;
			case GROUP_8:
				instr->type = grp8_decode_table[instr->reg_opc];
				break;
			case GROUP_9:
				instr->type = grp9_decode_table[instr->reg_opc];
				break;
			default:
				break;
			}
			decode_modrm_addr_modes(instr);
			goto done_rm;
		}
	}

	if (instr->flags & MOD_RM) {
		decode_modrm_fields(instr, RAM[pc++]);
		decode_modrm_addr_modes(instr);
	}

done_rm:

	if (instr->flags & SIB)
		decode_sib_byte(instr, RAM[pc++]);

	if (instr->flags & MEM_DISP_MASK)
		decode_disp(instr, RAM, &pc);

	if (instr->flags & MOFFSET_MASK)
		decode_moffset(instr, RAM, &pc);

	if (instr->flags & IMM_MASK)
		decode_imm(instr, RAM, &pc);

	if (instr->flags & REL_MASK)
		decode_rel(instr, RAM, &pc);

	decode_src_operand(instr);

	decode_dst_operand(instr);

	decode_third_operand(instr);

	instr->nr_bytes = (unsigned long)(pc - start_pc);

	return 0;
}

int
arch_x86_instr_length(struct x86_instr *instr)
{
	return instr->nr_bytes;
}
