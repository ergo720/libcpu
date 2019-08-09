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
#define X86_INSTR_OPC_MASK	0xFF // Note : When opcode count surpasses 8 bits, update this mask AND x86_instr_flags!

/* Readability markers. Only X86_OPC_DIFF_SYNTAX is checked during decoding! */
#define X86_OPC_PREFIX      0 // prefix bytes, fetched in arch_x86_decode_instr()
#define X86_OPC_UNDEFINED   X86_OPC_ILLEGAL	// non-existent instruction - NO flags allowed!
#define X86_OPC_DIFF_SYNTAX 0 // instr has different syntax between intel and att

/* Flags steering decoding differences between Intel and AT&T syntaxes - DO NOT mix these with x86_instr_flags! */
#define DIFF_SYNTAX_SIZE_OVERRIDE_SHIFT 8
#define DIFF_SYNTAX_USE_INTEL_SHIFT     16
#define DIFF_SYNTAX_SIZE_OVERRIDE (1 << DIFF_SYNTAX_SIZE_OVERRIDE_SHIFT)
#define DIFF_SYNTAX_USE_INTEL     (1 << DIFF_SYNTAX_USE_INTEL_SHIFT)

/* Shorthand for common conditional instruction flags */
#define Jb (ADDRMOD_REL | WIDTH_BYTE)
#define Jv (ADDRMOD_REL | WIDTH_DWORD)
#define Cv (ADDRMOD_RM_REG | WIDTH_DWORD)
#define Sb (ADDRMOD_RM | WIDTH_BYTE)

static const uint64_t decode_table_one[256] = {
	/*[0x00]*/	X86_OPC_ADD | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x01]*/	X86_OPC_ADD | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x02]*/	X86_OPC_ADD | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x03]*/	X86_OPC_ADD | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x04]*/	X86_OPC_ADD | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x05]*/	X86_OPC_ADD | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x06]*/	X86_OPC_PUSH | ADDRMOD_SEG2_REG /* ES */ | WIDTH_DWORD,
	/*[0x07]*/	X86_OPC_POP | ADDRMOD_SEG2_REG /* ES */ | WIDTH_DWORD,
	/*[0x08]*/	X86_OPC_OR | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x09]*/	X86_OPC_OR | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x0A]*/	X86_OPC_OR | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x0B]*/	X86_OPC_OR | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x0C]*/	X86_OPC_OR | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x0D]*/	X86_OPC_OR | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x0E]*/	X86_OPC_PUSH | ADDRMOD_SEG2_REG /* CS */ | WIDTH_DWORD,
	/*[0x0F]*/	X86_OPC_PREFIX /* TWO BYTE OPCODE */,
	/*[0x10]*/	X86_OPC_ADC | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x11]*/	X86_OPC_ADC | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x12]*/	X86_OPC_ADC | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x13]*/	X86_OPC_ADC | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x14]*/	X86_OPC_ADC | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x15]*/	X86_OPC_ADC | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x16]*/	X86_OPC_PUSH | ADDRMOD_SEG2_REG /* SS */ | WIDTH_DWORD,
	/*[0x17]*/	X86_OPC_POP | ADDRMOD_SEG2_REG /* SS */ | WIDTH_DWORD,
	/*[0x18]*/	X86_OPC_SBB | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x19]*/	X86_OPC_SBB | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x1A]*/	X86_OPC_SBB | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x1B]*/	X86_OPC_SBB | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x1C]*/	X86_OPC_SBB | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x1D]*/	X86_OPC_SBB | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x1E]*/	X86_OPC_PUSH | ADDRMOD_SEG2_REG /* DS */ | WIDTH_DWORD,
	/*[0x1F]*/	X86_OPC_POP | ADDRMOD_SEG2_REG /* DS */ | WIDTH_DWORD,
	/*[0x20]*/	X86_OPC_AND | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x21]*/	X86_OPC_AND | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x22]*/	X86_OPC_AND | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x23]*/	X86_OPC_AND | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x24]*/	X86_OPC_AND | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x25]*/	X86_OPC_AND | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x26]*/	X86_OPC_PREFIX /* ES_OVERRIDE */,
	/*[0x27]*/	X86_OPC_DAA | ADDRMOD_IMPLIED,
	/*[0x28]*/	X86_OPC_SUB | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x29]*/	X86_OPC_SUB | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x2A]*/	X86_OPC_SUB | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x2B]*/	X86_OPC_SUB | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x2C]*/	X86_OPC_SUB | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x2D]*/	X86_OPC_SUB | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x2E]*/	X86_OPC_PREFIX /* CS_OVERRIDE */,
	/*[0x2F]*/	X86_OPC_DAS | ADDRMOD_IMPLIED,
	/*[0x30]*/	X86_OPC_XOR | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x31]*/	X86_OPC_XOR | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x32]*/	X86_OPC_XOR | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x33]*/	X86_OPC_XOR | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x34]*/	X86_OPC_XOR | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x35]*/	X86_OPC_XOR | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x36]*/	X86_OPC_PREFIX /* SS_OVERRIDE */,
	/*[0x37]*/	X86_OPC_AAA | ADDRMOD_IMPLIED,
	/*[0x38]*/	X86_OPC_CMP | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x39]*/	X86_OPC_CMP | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x3A]*/	X86_OPC_CMP | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x3B]*/	X86_OPC_CMP | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x3C]*/	X86_OPC_CMP | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0x3D]*/	X86_OPC_CMP | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0x3E]*/	X86_OPC_PREFIX /* DS_OVERRIDE */,
	/*[0x3F]*/	X86_OPC_AAS | ADDRMOD_IMPLIED,
	/*[0x40]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x41]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x42]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x43]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x44]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x45]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x46]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x47]*/	X86_OPC_INC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x48]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x49]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x4A]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x4B]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x4C]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x4D]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x4E]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x4F]*/	X86_OPC_DEC | ADDRMOD_REG | WIDTH_DWORD,
	/*[0x50]*/	X86_OPC_PUSH | ADDRMOD_REG /* AX */ | WIDTH_DWORD,
	/*[0x51]*/	X86_OPC_PUSH | ADDRMOD_REG /* CX */ | WIDTH_DWORD,
	/*[0x52]*/	X86_OPC_PUSH | ADDRMOD_REG /* DX */ | WIDTH_DWORD,
	/*[0x53]*/	X86_OPC_PUSH | ADDRMOD_REG /* BX */ | WIDTH_DWORD,
	/*[0x54]*/	X86_OPC_PUSH | ADDRMOD_REG /* SP */ | WIDTH_DWORD,
	/*[0x55]*/	X86_OPC_PUSH | ADDRMOD_REG /* BP */ | WIDTH_DWORD,
	/*[0x56]*/	X86_OPC_PUSH | ADDRMOD_REG /* SI */ | WIDTH_DWORD,
	/*[0x57]*/	X86_OPC_PUSH | ADDRMOD_REG /* DI */ | WIDTH_DWORD,
	/*[0x58]*/	X86_OPC_POP | ADDRMOD_REG /* AX */  | WIDTH_DWORD,
	/*[0x59]*/	X86_OPC_POP | ADDRMOD_REG /* CX */  | WIDTH_DWORD,
	/*[0x5A]*/	X86_OPC_POP | ADDRMOD_REG /* DX */  | WIDTH_DWORD,
	/*[0x5B]*/	X86_OPC_POP | ADDRMOD_REG /* BX */  | WIDTH_DWORD,
	/*[0x5C]*/	X86_OPC_POP | ADDRMOD_REG /* SP */  | WIDTH_DWORD,
	/*[0x5D]*/	X86_OPC_POP | ADDRMOD_REG /* BP */  | WIDTH_DWORD,
	/*[0x5E]*/	X86_OPC_POP | ADDRMOD_REG /* SI */  | WIDTH_DWORD,
	/*[0x5F]*/	X86_OPC_POP | ADDRMOD_REG /* DI */  | WIDTH_DWORD,
	/*[0x60]*/	X86_OPC_PUSHA | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0x61]*/	X86_OPC_POPA | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0x62]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_RM_REG,
	/*[0x63]*/	X86_OPC_ARPL | ADDRMOD_REG_RM | WIDTH_WORD,
	/*[0x64]*/	X86_OPC_PREFIX /* FS_OVERRIDE */,
	/*[0x65]*/	X86_OPC_PREFIX /* GS_OVERRIDE */,
	/*[0x66]*/	X86_OPC_PREFIX /* OPERAND_SIZE_OVERRIDE */,
	/*[0x67]*/	X86_OPC_PREFIX /* ADDRESS_SIZE_OVERRIDE */,
	/*[0x68]*/	X86_OPC_PUSH | ADDRMOD_IMM | WIDTH_DWORD,
	/*[0x69]*/	X86_OPC_IMUL | ADDRMOD_RM_IMM_REG | WIDTH_DWORD,
	/*[0x6A]*/	X86_OPC_PUSH | ADDRMOD_IMM | WIDTH_BYTE,
	/*[0x6B]*/	X86_OPC_IMUL | ADDRMOD_RM_IMM8_REG | WIDTH_DWORD,
	/*[0x6C]*/	X86_OPC_INS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0x6D]*/	X86_OPC_INS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0x6E]*/	X86_OPC_OUTS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0x6F]*/	X86_OPC_OUTS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0x70]*/	X86_OPC_JO  | Jb,
	/*[0x71]*/	X86_OPC_JNO | Jb,
	/*[0x72]*/	X86_OPC_JB  | Jb,
	/*[0x73]*/	X86_OPC_JNB | Jb,
	/*[0x74]*/	X86_OPC_JZ  | Jb,
	/*[0x75]*/	X86_OPC_JNE | Jb,
	/*[0x76]*/	X86_OPC_JBE | Jb,
	/*[0x77]*/	X86_OPC_JA  | Jb,
	/*[0x78]*/	X86_OPC_JS  | Jb,
	/*[0x79]*/	X86_OPC_JNS | Jb,
	/*[0x7A]*/	X86_OPC_JPE | Jb,
	/*[0x7B]*/	X86_OPC_JPO | Jb,
	/*[0x7C]*/	X86_OPC_JL  | Jb,
	/*[0x7D]*/	X86_OPC_JGE | Jb,
	/*[0x7E]*/	X86_OPC_JLE | Jb,
	/*[0x7F]*/	X86_OPC_JG  | Jb,
	/*[0x80]*/	GROUP_1 | ADDRMOD_IMM8_RM | WIDTH_BYTE,
	/*[0x81]*/	GROUP_1 | ADDRMOD_IMM_RM | WIDTH_DWORD,
	/*[0x82]*/	GROUP_1 | ADDRMOD_IMM8_RM | WIDTH_BYTE,
	/*[0x83]*/	GROUP_1 | ADDRMOD_IMM8_RM | WIDTH_DWORD,
	/*[0x84]*/	X86_OPC_TEST | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x85]*/	X86_OPC_TEST | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x86]*/	X86_OPC_XCHG | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x87]*/	X86_OPC_XCHG | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x88]*/	X86_OPC_MOV | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0x89]*/	X86_OPC_MOV | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0x8A]*/	X86_OPC_MOV | ADDRMOD_RM_REG | WIDTH_BYTE,
	/*[0x8B]*/	X86_OPC_MOV | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x8C]*/	X86_OPC_MOV | ADDRMOD_SEG3_REG_RM | WIDTH_WORD,
	/*[0x8D]*/	X86_OPC_LEA | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x8E]*/	X86_OPC_MOV | ADDRMOD_RM_SEG3_REG | WIDTH_WORD,
	/*[0x8F]*/	X86_OPC_POP | ADDRMOD_RM | WIDTH_DWORD,
	/*[0x90]*/	X86_OPC_NOP | ADDRMOD_IMPLIED,	/* xchg eax, eax */
	/*[0x91]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x92]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x93]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x94]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x95]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x96]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x97]*/	X86_OPC_XCHG | ADDRMOD_ACC_REG | WIDTH_DWORD,
	/*[0x98]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_IMPLIED,
	/*[0x99]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_IMPLIED,
	/*[0x9A]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_FAR_PTR | WIDTH_DWORD,
	/*[0x9B]*/	X86_OPC_UNDEFINED, // fpu
	/*[0x9C]*/	X86_OPC_PUSHF | ADDRMOD_IMPLIED,
	/*[0x9D]*/	X86_OPC_POPF | ADDRMOD_IMPLIED,
	/*[0x9E]*/	X86_OPC_SAHF | ADDRMOD_IMPLIED,
	/*[0x9F]*/	X86_OPC_LAHF | ADDRMOD_IMPLIED,
	/*[0xA0]*/	X86_OPC_MOV | ADDRMOD_MOFFSET_ACC | WIDTH_BYTE, /* load */
	/*[0xA1]*/	X86_OPC_MOV | ADDRMOD_MOFFSET_ACC | WIDTH_DWORD, /* load */
	/*[0xA2]*/	X86_OPC_MOV | ADDRMOD_ACC_MOFFSET | WIDTH_BYTE, /* store */
	/*[0xA3]*/	X86_OPC_MOV | ADDRMOD_ACC_MOFFSET | WIDTH_DWORD, /* store */
	/*[0xA4]*/	X86_OPC_MOVS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xA5]*/	X86_OPC_MOVS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xA6]*/	X86_OPC_CMPS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xA7]*/	X86_OPC_CMPS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xA8]*/	X86_OPC_TEST | ADDRMOD_IMM_ACC | WIDTH_BYTE,
	/*[0xA9]*/	X86_OPC_TEST | ADDRMOD_IMM_ACC | WIDTH_DWORD,
	/*[0xAA]*/	X86_OPC_STOS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xAB]*/	X86_OPC_STOS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xAC]*/	X86_OPC_LODS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xAD]*/	X86_OPC_LODS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xAE]*/	X86_OPC_SCAS | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xAF]*/	X86_OPC_SCAS | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xB0]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB1]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB2]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB3]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB4]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB5]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB6]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB7]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_BYTE,
	/*[0xB8]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xB9]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xBA]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xBB]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xBC]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xBD]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xBE]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xBF]*/	X86_OPC_MOV | ADDRMOD_IMM_REG | WIDTH_DWORD,
	/*[0xC0]*/	GROUP_2 | ADDRMOD_IMM8_RM | WIDTH_BYTE,
	/*[0xC1]*/	GROUP_2 | ADDRMOD_IMM8_RM | WIDTH_DWORD,
	/*[0xC2]*/	X86_OPC_RET | ADDRMOD_IMM | WIDTH_WORD,
	/*[0xC3]*/	X86_OPC_RET | ADDRMOD_IMPLIED,
	/*[0xC4]*/	X86_OPC_LES | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xC5]*/	X86_OPC_LDS | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xC6]*/	X86_OPC_MOV | ADDRMOD_IMM8_RM | WIDTH_BYTE,
	/*[0xC7]*/	X86_OPC_MOV | ADDRMOD_IMM_RM | WIDTH_DWORD,
	/*[0xC8]*/	X86_OPC_ENTER | ADDRMOD_IMM8_IMM16 | WIDTH_WORD,
	/*[0xC9]*/	X86_OPC_LEAVE | ADDRMOD_IMPLIED,
	/*[0xCA]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_IMM | WIDTH_WORD,
	/*[0xCB]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_IMPLIED,
	/*[0xCC]*/	X86_OPC_INT3 | ADDRMOD_IMPLIED,
	/*[0xCD]*/	X86_OPC_INT | ADDRMOD_IMM | WIDTH_BYTE,
	/*[0xCE]*/	X86_OPC_INTO | ADDRMOD_IMPLIED,
	/*[0xCF]*/	X86_OPC_IRET | ADDRMOD_IMPLIED,
	/*[0xD0]*/	GROUP_2 | ADDRMOD_RM | WIDTH_BYTE,
	/*[0xD1]*/	GROUP_2 | ADDRMOD_RM | WIDTH_DWORD,
	/*[0xD2]*/	GROUP_2 | ADDRMOD_RM | WIDTH_BYTE,
	/*[0xD3]*/	GROUP_2 | ADDRMOD_RM | WIDTH_DWORD,
	/*[0xD4]*/	X86_OPC_AAM | ADDRMOD_IMM | WIDTH_BYTE,
	/*[0xD5]*/	X86_OPC_AAD | ADDRMOD_IMM | WIDTH_BYTE,
	/*[0xD6]*/	X86_OPC_UNDEFINED, // SALC undocumented instr, should add this?
	/*[0xD7]*/	X86_OPC_XLATB | ADDRMOD_IMPLIED,
	/*[0xD8]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xD9]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xDA]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xDB]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xDC]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xDD]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xDE]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xDF]*/	X86_OPC_UNDEFINED, // fpu
	/*[0xE0]*/	X86_OPC_LOOPNE | ADDRMOD_REL | WIDTH_BYTE,
	/*[0xE1]*/	X86_OPC_LOOPE | ADDRMOD_REL | WIDTH_BYTE,
	/*[0xE2]*/	X86_OPC_LOOP | ADDRMOD_REL | WIDTH_BYTE,
	/*[0xE3]*/	X86_OPC_JECXZ | Jb,
	/*[0xE4]*/	X86_OPC_IN | ADDRMOD_IMM | WIDTH_BYTE,
	/*[0xE5]*/	X86_OPC_IN | ADDRMOD_IMM8 | WIDTH_DWORD,
	/*[0xE6]*/	X86_OPC_OUT | ADDRMOD_IMM | WIDTH_BYTE,
	/*[0xE7]*/	X86_OPC_OUT | ADDRMOD_IMM8 | WIDTH_DWORD,
	/*[0xE8]*/	X86_OPC_CALL | Jv,
	/*[0xE9]*/	X86_OPC_JMP  | Jv,
	/*[0xEA]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_FAR_PTR | WIDTH_DWORD,
	/*[0xEB]*/	X86_OPC_JMP  | Jb,
	/*[0xEC]*/	X86_OPC_IN | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xED]*/	X86_OPC_IN | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xEE]*/	X86_OPC_OUT | ADDRMOD_IMPLIED | WIDTH_BYTE,
	/*[0xEF]*/	X86_OPC_OUT | ADDRMOD_IMPLIED | WIDTH_DWORD,
	/*[0xF0]*/	X86_OPC_PREFIX /* LOCK */,
	/*[0xF1]*/	X86_OPC_UNDEFINED, // INT1 undocumented instr, should add this?
	/*[0xF2]*/	X86_OPC_PREFIX /* REPNZ_PREFIX */,
	/*[0xF3]*/	X86_OPC_PREFIX /* REPZ_PREFIX */,
	/*[0xF4]*/	X86_OPC_HLT | ADDRMOD_IMPLIED,
	/*[0xF5]*/	X86_OPC_CMC | ADDRMOD_IMPLIED,
	/*[0xF6]*/	GROUP_3 | ADDRMOD_RM | WIDTH_BYTE,
	/*[0xF7]*/	GROUP_3 | ADDRMOD_RM | WIDTH_DWORD,
	/*[0xF8]*/	X86_OPC_CLC | ADDRMOD_IMPLIED,
	/*[0xF9]*/	X86_OPC_STC | ADDRMOD_IMPLIED,
	/*[0xFA]*/	X86_OPC_CLI | ADDRMOD_IMPLIED,
	/*[0xFB]*/	X86_OPC_STI | ADDRMOD_IMPLIED,
	/*[0xFC]*/	X86_OPC_CLD | ADDRMOD_IMPLIED,
	/*[0xFD]*/	X86_OPC_STD | ADDRMOD_IMPLIED,
	/*[0xFE]*/	GROUP_4 | ADDRMOD_RM | WIDTH_BYTE,
	/*[0xFF]*/	GROUP_5 | ADDRMOD_RM | WIDTH_DWORD,
};

static const uint64_t decode_table_two[256] = {
	/*[0x00]*/	GROUP_6 | ADDRMOD_RM | WIDTH_WORD,
	/*[0x01]*/	GROUP_7 | ADDRMOD_RM | WIDTH_DWORD,
	/*[0x02]*/	X86_OPC_LAR | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x03]*/	X86_OPC_LSL | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0x04]*/	X86_OPC_UNDEFINED,
	/*[0x05]*/	X86_OPC_UNDEFINED,
	/*[0x06]*/	X86_OPC_CLTS | ADDRMOD_IMPLIED,
	/*[0x07]*/	X86_OPC_UNDEFINED,
	/*[0x08]*/	X86_OPC_INVD | ADDRMOD_IMPLIED,
	/*[0x09]*/	X86_OPC_WBINVD | ADDRMOD_IMPLIED,
	/*[0x0A]*/	X86_OPC_UNDEFINED,
	/*[0x0B]*/	X86_OPC_UD2 | ADDRMOD_IMPLIED,
	/*[0x0C]*/	X86_OPC_UNDEFINED,
	/*[0x0D]*/	X86_OPC_UNDEFINED,
	/*[0x0E]*/	X86_OPC_UNDEFINED,
	/*[0x0F]*/	X86_OPC_UNDEFINED,
	/*[0x10]*/	X86_OPC_UNDEFINED, // sse
	/*[0x11]*/	X86_OPC_UNDEFINED, // sse
	/*[0x12]*/	X86_OPC_UNDEFINED, // sse
	/*[0x13]*/	X86_OPC_UNDEFINED, // sse
	/*[0x14]*/	X86_OPC_UNDEFINED, // sse
	/*[0x15]*/	X86_OPC_UNDEFINED, // sse
	/*[0x16]*/	X86_OPC_UNDEFINED, // sse
	/*[0x17]*/	X86_OPC_UNDEFINED, // sse
	/*[0x18]*/	X86_OPC_UNDEFINED, // sse
	/*[0x19]*/	X86_OPC_UNDEFINED,
	/*[0x1A]*/	X86_OPC_UNDEFINED,
	/*[0x1B]*/	X86_OPC_UNDEFINED,
	/*[0x1C]*/	X86_OPC_UNDEFINED,
	/*[0x1D]*/	X86_OPC_UNDEFINED,
	/*[0x1E]*/	X86_OPC_UNDEFINED,
	/*[0x1F]*/	X86_OPC_UNDEFINED,
	/*[0x20]*/	X86_OPC_MOV | ADDRMOD_CR_RM | WIDTH_DWORD,
	/*[0x21]*/	X86_OPC_MOV | ADDRMOD_DBG_RM | WIDTH_DWORD,
	/*[0x22]*/	X86_OPC_MOV | ADDRMOD_RM_CR | WIDTH_DWORD,
	/*[0x23]*/	X86_OPC_MOV | ADDRMOD_RM_DBG | WIDTH_DWORD,
	/*[0x24]*/	X86_OPC_UNDEFINED,
	/*[0x25]*/	X86_OPC_UNDEFINED,
	/*[0x26]*/	X86_OPC_UNDEFINED,
	/*[0x27]*/	X86_OPC_UNDEFINED,
	/*[0x28]*/	X86_OPC_UNDEFINED, // sse
	/*[0x29]*/	X86_OPC_UNDEFINED, // sse
	/*[0x2A]*/	X86_OPC_UNDEFINED, // sse
	/*[0x2B]*/	X86_OPC_UNDEFINED, // sse
	/*[0x2C]*/	X86_OPC_UNDEFINED, // sse
	/*[0x2D]*/	X86_OPC_UNDEFINED, // sse
	/*[0x2E]*/	X86_OPC_UNDEFINED, // sse
	/*[0x2F]*/	X86_OPC_UNDEFINED, // sse
	/*[0x30]*/	X86_OPC_WRMSR | ADDRMOD_IMPLIED,
	/*[0x31]*/	X86_OPC_RDTSC | ADDRMOD_IMPLIED,
	/*[0x32]*/	X86_OPC_RDMSR | ADDRMOD_IMPLIED,
	/*[0x33]*/	X86_OPC_RDPMC | ADDRMOD_IMPLIED,
	/*[0x34]*/	X86_OPC_SYSENTER | ADDRMOD_IMPLIED,
	/*[0x35]*/	X86_OPC_SYSEXIT | ADDRMOD_IMPLIED,
	/*[0x36]*/	X86_OPC_UNDEFINED,
	/*[0x37]*/	X86_OPC_UNDEFINED,
	/*[0x38]*/	X86_OPC_UNDEFINED,
	/*[0x39]*/	X86_OPC_UNDEFINED,
	/*[0x3A]*/	X86_OPC_UNDEFINED,
	/*[0x3B]*/	X86_OPC_UNDEFINED,
	/*[0x3C]*/	X86_OPC_UNDEFINED,
	/*[0x3D]*/	X86_OPC_UNDEFINED,
	/*[0x3E]*/	X86_OPC_UNDEFINED,
	/*[0x3F]*/	X86_OPC_UNDEFINED,
	/*[0x40]*/	X86_OPC_CMOVO  | Cv,
	/*[0x41]*/	X86_OPC_CMOVNO | Cv,
	/*[0x42]*/	X86_OPC_CMOVB  | Cv,
	/*[0x43]*/	X86_OPC_CMOVNB | Cv,
	/*[0x44]*/	X86_OPC_CMOVZ  | Cv,
	/*[0x45]*/	X86_OPC_CMOVNE | Cv,
	/*[0x46]*/	X86_OPC_CMOVBE | Cv,
	/*[0x47]*/	X86_OPC_CMOVA  | Cv,
	/*[0x48]*/	X86_OPC_CMOVS  | Cv,
	/*[0x49]*/	X86_OPC_CMOVNS | Cv,
	/*[0x4A]*/	X86_OPC_CMOVPE | Cv,
	/*[0x4B]*/	X86_OPC_CMOVPO | Cv,
	/*[0x4C]*/	X86_OPC_CMOVL  | Cv,
	/*[0x4D]*/	X86_OPC_CMOVGE | Cv,
	/*[0x4E]*/	X86_OPC_CMOVLE | Cv,
	/*[0x4F]*/	X86_OPC_CMOVG  | Cv,
	/*[0x50]*/	X86_OPC_UNDEFINED, // sse
	/*[0x51]*/	X86_OPC_UNDEFINED, // sse
	/*[0x52]*/	X86_OPC_UNDEFINED, // sse
	/*[0x53]*/	X86_OPC_UNDEFINED, // sse
	/*[0x54]*/	X86_OPC_UNDEFINED, // sse
	/*[0x55]*/	X86_OPC_UNDEFINED, // sse
	/*[0x56]*/	X86_OPC_UNDEFINED, // sse
	/*[0x57]*/	X86_OPC_UNDEFINED, // sse
	/*[0x58]*/	X86_OPC_UNDEFINED, // sse
	/*[0x59]*/	X86_OPC_UNDEFINED, // sse
	/*[0x5A]*/	X86_OPC_UNDEFINED,
	/*[0x5B]*/	X86_OPC_UNDEFINED,
	/*[0x5C]*/	X86_OPC_UNDEFINED, // sse
	/*[0x5D]*/	X86_OPC_UNDEFINED, // sse
	/*[0x5E]*/	X86_OPC_UNDEFINED, // sse
	/*[0x5F]*/	X86_OPC_UNDEFINED, // sse
	/*[0x60]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x61]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x62]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x63]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x64]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x65]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x66]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x67]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x68]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x69]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x6A]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x6B]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x6C]*/	X86_OPC_UNDEFINED,
	/*[0x6D]*/	X86_OPC_UNDEFINED,
	/*[0x6E]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x6F]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x70]*/	X86_OPC_UNDEFINED, // sse
	/*[0x71]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x72]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x73]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x74]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x75]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x76]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x77]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x78]*/	X86_OPC_UNDEFINED,
	/*[0x79]*/	X86_OPC_UNDEFINED,
	/*[0x7A]*/	X86_OPC_UNDEFINED,
	/*[0x7B]*/	X86_OPC_UNDEFINED,
	/*[0x7C]*/	X86_OPC_UNDEFINED,
	/*[0x7D]*/	X86_OPC_UNDEFINED,
	/*[0x7E]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x7F]*/	X86_OPC_UNDEFINED, // mmx
	/*[0x80]*/	X86_OPC_JO  | Jv,
	/*[0x81]*/	X86_OPC_JNO | Jv,
	/*[0x82]*/	X86_OPC_JB  | Jv,
	/*[0x83]*/	X86_OPC_JNB | Jv,
	/*[0x84]*/	X86_OPC_JZ  | Jv,
	/*[0x85]*/	X86_OPC_JNE | Jv,
	/*[0x86]*/	X86_OPC_JBE | Jv,
	/*[0x87]*/	X86_OPC_JA  | Jv,
	/*[0x88]*/	X86_OPC_JS  | Jv,
	/*[0x89]*/	X86_OPC_JNS | Jv,
	/*[0x8A]*/	X86_OPC_JPE | Jv,
	/*[0x8B]*/	X86_OPC_JPO | Jv,
	/*[0x8C]*/	X86_OPC_JL  | Jv,
	/*[0x8D]*/	X86_OPC_JGE | Jv,
	/*[0x8E]*/	X86_OPC_JLE | Jv,
	/*[0x8F]*/	X86_OPC_JG  | Jv,
	/*[0x90]*/	X86_OPC_SETO  | Sb,
	/*[0x91]*/	X86_OPC_SETNO | Sb,
	/*[0x92]*/	X86_OPC_SETB  | Sb,
	/*[0x93]*/	X86_OPC_SETNB | Sb,
	/*[0x94]*/	X86_OPC_SETZ  | Sb,
	/*[0x95]*/	X86_OPC_SETNE | Sb,
	/*[0x96]*/	X86_OPC_SETBE | Sb,
	/*[0x97]*/	X86_OPC_SETA  | Sb,
	/*[0x98]*/	X86_OPC_SETS  | Sb,
	/*[0x99]*/	X86_OPC_SETNS | Sb,
	/*[0x9A]*/	X86_OPC_SETPE | Sb,
	/*[0x9B]*/	X86_OPC_SETPO | Sb,
	/*[0x9C]*/	X86_OPC_SETL  | Sb,
	/*[0x9D]*/	X86_OPC_SETGE | Sb,
	/*[0x9E]*/	X86_OPC_SETLE | Sb,
	/*[0x9F]*/	X86_OPC_SETG  | Sb,
	/*[0xA0]*/	X86_OPC_PUSH | ADDRMOD_SEG3_REG /* FS */ | WIDTH_DWORD,
	/*[0xA1]*/	X86_OPC_POP | ADDRMOD_SEG3_REG /* FS */ | WIDTH_DWORD,
	/*[0xA2]*/	X86_OPC_CPUID | ADDRMOD_IMPLIED,
	/*[0xA3]*/	X86_OPC_BT | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0xA4]*/	X86_OPC_SHLD | ADDRMOD_REG_IMM8_RM | WIDTH_DWORD,
	/*[0xA5]*/	X86_OPC_SHLD | ADDRMOD_REG_CL_RM | WIDTH_DWORD,
	/*[0xA6]*/	X86_OPC_UNDEFINED,
	/*[0xA7]*/	X86_OPC_UNDEFINED,
	/*[0xA8]*/	X86_OPC_PUSH | ADDRMOD_SEG3_REG /* GS */ | WIDTH_DWORD,
	/*[0xA9]*/	X86_OPC_POP | ADDRMOD_SEG3_REG /* GS */ | WIDTH_DWORD,
	/*[0xAA]*/	X86_OPC_RSM | ADDRMOD_IMPLIED,
	/*[0xAB]*/	X86_OPC_BTS | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0xAC]*/	X86_OPC_SHRD | ADDRMOD_REG_IMM8_RM | WIDTH_DWORD,
	/*[0xAD]*/	X86_OPC_SHRD | ADDRMOD_REG_CL_RM | WIDTH_DWORD,
	/*[0xAE]*/	X86_OPC_UNDEFINED, // fpu, sse
	/*[0xAF]*/	X86_OPC_IMUL | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xB0]*/	X86_OPC_CMPXCHG | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0xB1]*/	X86_OPC_CMPXCHG | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0xB2]*/	X86_OPC_LSS | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xB3]*/	X86_OPC_BTR | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0xB4]*/	X86_OPC_LFS | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xB5]*/	X86_OPC_LGS | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xB6]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xB7]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xB8]*/	X86_OPC_UNDEFINED,
	/*[0xB9]*/	X86_OPC_UD1 | ADDRMOD_IMPLIED,
	/*[0xBA]*/	GROUP_8 | ADDRMOD_IMM8_RM | WIDTH_DWORD,
	/*[0xBB]*/	X86_OPC_BTC | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0xBC]*/	X86_OPC_BSF | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xBD]*/	X86_OPC_BSR | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xBE]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xBF]*/	X86_OPC_DIFF_SYNTAX | ADDRMOD_RM_REG | WIDTH_DWORD,
	/*[0xC0]*/	X86_OPC_XADD | ADDRMOD_REG_RM | WIDTH_BYTE,
	/*[0xC1]*/	X86_OPC_XADD | ADDRMOD_REG_RM | WIDTH_DWORD,
	/*[0xC2]*/	X86_OPC_UNDEFINED, // sse
	/*[0xC3]*/	X86_OPC_UNDEFINED,
	/*[0xC4]*/	X86_OPC_UNDEFINED, // sse
	/*[0xC5]*/	X86_OPC_UNDEFINED, // sse
	/*[0xC6]*/	X86_OPC_UNDEFINED, // sse
	/*[0xC7]*/	GROUP_9 | ADDRMOD_RM | WIDTH_QWORD,
	/*[0xC8]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xC9]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xCA]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xCB]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xCC]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xCD]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xCE]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xCF]*/	X86_OPC_BSWAP | ADDRMOD_REG | WIDTH_DWORD,
	/*[0xD0]*/	X86_OPC_UNDEFINED,
	/*[0xD1]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xD2]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xD3]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xD4]*/	X86_OPC_UNDEFINED,
	/*[0xD5]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xD6]*/	X86_OPC_UNDEFINED,
	/*[0xD7]*/	X86_OPC_UNDEFINED, // sse
	/*[0xD8]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xD9]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xDA]*/	X86_OPC_UNDEFINED, // sse
	/*[0xDB]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xDC]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xDD]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xDE]*/	X86_OPC_UNDEFINED, // sse
	/*[0xDF]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xE0]*/	X86_OPC_UNDEFINED, // sse
	/*[0xE1]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xE2]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xE3]*/	X86_OPC_UNDEFINED, // sse
	/*[0xE4]*/	X86_OPC_UNDEFINED, // sse
	/*[0xE5]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xE6]*/	X86_OPC_UNDEFINED,
	/*[0xE7]*/	X86_OPC_UNDEFINED, // sse
	/*[0xE8]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xE9]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xEA]*/	X86_OPC_UNDEFINED, // sse
	/*[0xEB]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xEC]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xED]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xEE]*/	X86_OPC_UNDEFINED, // sse
	/*[0xEF]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xF0]*/	X86_OPC_UNDEFINED,
	/*[0xF1]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xF2]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xF3]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xF4]*/	X86_OPC_UNDEFINED,
	/*[0xF5]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xF6]*/	X86_OPC_UNDEFINED, // sse
	/*[0xF7]*/	X86_OPC_UNDEFINED, // sse
	/*[0xF8]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xF9]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xFA]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xFB]*/	X86_OPC_UNDEFINED,
	/*[0xFC]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xFD]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xFE]*/	X86_OPC_UNDEFINED, // mmx
	/*[0xFF]*/	X86_OPC_UNDEFINED,
};

static const uint32_t grp1_decode_table[8] = {
	/*[0x00]*/	X86_OPC_ADD,
	/*[0x01]*/	X86_OPC_OR,
	/*[0x02]*/	X86_OPC_ADC,
	/*[0x03]*/	X86_OPC_SBB,
	/*[0x04]*/	X86_OPC_AND,
	/*[0x05]*/	X86_OPC_SUB,
	/*[0x06]*/	X86_OPC_XOR,
	/*[0x07]*/	X86_OPC_CMP,
};

static const uint32_t grp2_decode_table[8] = {
	/*[0x00]*/	X86_OPC_ROL,
	/*[0x01]*/	X86_OPC_ROR,
	/*[0x02]*/	X86_OPC_RCL,
	/*[0x03]*/	X86_OPC_RCR,
	/*[0x04]*/	X86_OPC_SHL,
	/*[0x05]*/	X86_OPC_SHR,
	/*[0x06]*/	X86_OPC_UNDEFINED,
	/*[0x07]*/	X86_OPC_SAR,
};

static const uint32_t grp3_decode_table[8] = {
	/*[0x00]*/	X86_OPC_TEST,
	/*[0x01]*/	X86_OPC_UNDEFINED,
	/*[0x02]*/	X86_OPC_NOT,
	/*[0x03]*/	X86_OPC_NEG,
	/*[0x04]*/	X86_OPC_MUL,
	/*[0x05]*/	X86_OPC_IMUL,
	/*[0x06]*/	X86_OPC_DIV,
	/*[0x07]*/	X86_OPC_IDIV,
};

static const uint32_t grp4_decode_table[8] = {
	/*[0x00]*/	X86_OPC_INC,
	/*[0x01]*/	X86_OPC_DEC,
	/*[0x02]*/	X86_OPC_UNDEFINED,
	/*[0x03]*/	X86_OPC_UNDEFINED,
	/*[0x04]*/	X86_OPC_UNDEFINED,
	/*[0x05]*/	X86_OPC_UNDEFINED,
	/*[0x06]*/	X86_OPC_UNDEFINED,
	/*[0x07]*/	X86_OPC_UNDEFINED,
};

static const uint32_t grp5_decode_table[8] = {
	/*[0x00]*/	X86_OPC_INC,
	/*[0x01]*/	X86_OPC_DEC,
	/*[0x02]*/	X86_OPC_CALL,
	/*[0x03]*/	X86_OPC_DIFF_SYNTAX,
	/*[0x04]*/	X86_OPC_JMP,
	/*[0x05]*/	X86_OPC_DIFF_SYNTAX,
	/*[0x06]*/	X86_OPC_PUSH,
	/*[0x07]*/	X86_OPC_UNDEFINED,
};

static const uint32_t grp6_decode_table[8] = {
	/*[0x00]*/	X86_OPC_SLDT,
	/*[0x01]*/	X86_OPC_STR,
	/*[0x02]*/	X86_OPC_LLDT,
	/*[0x03]*/	X86_OPC_LTR,
	/*[0x04]*/	X86_OPC_VERR,
	/*[0x05]*/	X86_OPC_VERW,
	/*[0x06]*/	X86_OPC_UNDEFINED,
	/*[0x07]*/	X86_OPC_UNDEFINED,
};

static const uint32_t grp7_decode_table[8] = {
	/*[0x00]*/	X86_OPC_DIFF_SYNTAX,
	/*[0x01]*/	X86_OPC_DIFF_SYNTAX,
	/*[0x02]*/	X86_OPC_DIFF_SYNTAX,
	/*[0x03]*/	X86_OPC_DIFF_SYNTAX,
	/*[0x04]*/	X86_OPC_SMSW,
	/*[0x05]*/	X86_OPC_UNDEFINED,
	/*[0x06]*/	X86_OPC_LMSW,
	/*[0x07]*/	X86_OPC_INVLPG,
};

static const uint32_t grp8_decode_table[8] = {
	/*[0x00]*/	X86_OPC_UNDEFINED,
	/*[0x01]*/	X86_OPC_UNDEFINED,
	/*[0x02]*/	X86_OPC_UNDEFINED,
	/*[0x03]*/	X86_OPC_UNDEFINED,
	/*[0x04]*/	X86_OPC_BT,
	/*[0x05]*/	X86_OPC_BTS,
	/*[0x06]*/	X86_OPC_BTR,
	/*[0x07]*/	X86_OPC_BTC,
};

static const uint32_t grp9_decode_table[8] = {
	/*[0x00]*/	X86_OPC_UNDEFINED,
	/*[0x01]*/	X86_OPC_CMPXCHG8B,
	/*[0x02]*/	X86_OPC_UNDEFINED,
	/*[0x03]*/	X86_OPC_UNDEFINED,
	/*[0x04]*/	X86_OPC_UNDEFINED,
	/*[0x05]*/	X86_OPC_UNDEFINED,
	/*[0x06]*/	X86_OPC_UNDEFINED,
	/*[0x07]*/	X86_OPC_UNDEFINED,
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
			// Incorrect? operand->disp = instr->disp;
		}
		else {
			operand->type = OPTYPE_MEM;
			operand->reg = decode_dst_mem(instr);
		}
		break;
	case DST_MEM_DISP_BYTE:
	case DST_MEM_DISP_WORD:
	case DST_MEM_DISP_DWORD:
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
			// Incorrect? operand->disp = instr->disp;
		}
		else {
			operand->type = OPTYPE_MEM;
			operand->reg = decode_src_mem(instr);
		}
		break;
	case SRC_MEM_DISP_BYTE:
	case SRC_MEM_DISP_WORD:
	case SRC_MEM_DISP_DWORD:
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
	// TODO case WIDTH_QWORD:
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
	// TODO case WIDTH_QWORD:
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
	case SRC_MEM_DISP_DWORD:
	case DST_MEM_DISP_DWORD:
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
	/*[0x00]*/	DST_MEM_DISP_DWORD,
	/*[0x01]*/	DST_MEM_DISP_BYTE,
	/*[0x02]*/	DST_MEM_DISP_DWORD,
};

static const uint64_t sib_src_decode[] = {
	/*[0x00]*/	SRC_MEM_DISP_DWORD,
	/*[0x01]*/	SRC_MEM_DISP_BYTE,
	/*[0x02]*/	SRC_MEM_DISP_DWORD,
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

#define RM_SIZE_SHIFT 16
#define RM_SIZE (1 << RM_SIZE_SHIFT)

#define RM_MOD_SHIFT 8
#define RM_MOD_0 (0 << RM_MOD_SHIFT) // TODO : Rename
#define RM_MOD_1 (1 << RM_MOD_SHIFT) // TODO : Rename
#define RM_MOD_2 (2 << RM_MOD_SHIFT) // TODO : Rename

static void
decode_modrm_addr_modes(struct x86_instr *instr)
{
	if (instr->flags & DIR_REVERSED) {
		instr->flags |= mod_dst_decode[instr->mod];
		switch ((instr->addr_size_override << RM_SIZE_SHIFT) | (instr->mod << RM_MOD_SHIFT) | instr->rm) {
		case 0 | RM_MOD_2: // fallthrough
		case 1 | RM_MOD_2: // fallthrough
		case 2 | RM_MOD_2: // fallthrough
		case 3 | RM_MOD_2: // fallthrough
		case 5 | RM_MOD_2: // fallthrough
		case 6 | RM_MOD_2: // fallthrough
		case 7 | RM_MOD_2:
			instr->flags |= DST_MEM_DISP_DWORD;
			break;
		case 0 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 1 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 2 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 3 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 4 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 5 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 6 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 7 | RM_MOD_2 | RM_SIZE:
			instr->flags |= DST_MEM_DISP_WORD;
			break;
		case 4 | RM_MOD_0: // fallthrough
		case 4 | RM_MOD_1:
			instr->flags |= SIB;
			break;
		case 4 | RM_MOD_2:
			instr->flags |= (DST_MEM_DISP_DWORD | SIB);
			break;
		case 5 | RM_MOD_0:
			instr->flags &= ~DST_MEM;
			instr->flags |= DST_MEM_DISP_DWORD;
			break;
		case 6 | RM_MOD_0 | RM_SIZE:
			instr->flags &= ~DST_MEM;
			instr->flags |= DST_MEM_DISP_WORD;
			break;
		default:
			break;
		}
	}
	else {
		instr->flags |= mod_src_decode[instr->mod];
		switch ((instr->addr_size_override << RM_SIZE_SHIFT) | (instr->mod << RM_MOD_SHIFT) | instr->rm) {
		case 0 | RM_MOD_2: // fallthrough
		case 1 | RM_MOD_2: // fallthrough
		case 2 | RM_MOD_2: // fallthrough
		case 3 | RM_MOD_2: // fallthrough
		case 5 | RM_MOD_2: // fallthrough
		case 6 | RM_MOD_2: // fallthrough
		case 7 | RM_MOD_2:
			instr->flags |= SRC_MEM_DISP_DWORD;
			break;
		case 0 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 1 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 2 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 3 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 4 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 5 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 6 | RM_MOD_2 | RM_SIZE: // fallthrough
		case 7 | RM_MOD_2 | RM_SIZE:
			instr->flags |= SRC_MEM_DISP_WORD;
			break;
		case 4 | RM_MOD_0: // fallthrough
		case 4 | RM_MOD_1:
			instr->flags |= SIB;
			break;
		case 4 | RM_MOD_2:
			instr->flags |= (SRC_MEM_DISP_DWORD | SIB);
			break;
		case 5 | RM_MOD_0:
			instr->flags &= ~SRC_MEM;
			instr->flags |= SRC_MEM_DISP_DWORD;
			break;
		case 6 | RM_MOD_0 | RM_SIZE:
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

	/* Prefixes (X86_OPC_PREFIX) */
	instr->seg_override	= NO_OVERRIDE;
	instr->rep_prefix	= NO_PREFIX;
	instr->addr_size_override = 0;
	unsigned op_size_override = 0;
	instr->lock_prefix	= 0;
	instr->is_two_byte_instr = 0;

	while (true) {
		switch (opcode = RAM[pc++]) {
		case 0x26:
			instr->seg_override	= ES_OVERRIDE;
			continue;
		case 0x2E:
			instr->seg_override	= CS_OVERRIDE;
			continue;
		case 0x36:
			instr->seg_override	= SS_OVERRIDE;
			continue;
		case 0x3E:
			instr->seg_override	= DS_OVERRIDE;
			continue;
		case 0x64:
			instr->seg_override = FS_OVERRIDE;
			continue;
		case 0x65:
			instr->seg_override = GS_OVERRIDE;
			continue;
		case 0x66:
			op_size_override = 1;
			continue;
		case 0x67:
			instr->addr_size_override = 1;
			continue;
		case 0xF0:	/* LOCK */
			instr->lock_prefix = 1;
			continue;
		case 0xF2:	/* REPNE/REPNZ */
			instr->rep_prefix = REPNZ_PREFIX;
			continue;
		case 0xF3:	/* REP/REPE/REPZ */
			instr->rep_prefix = REPZ_PREFIX;
			continue;
		// no default
		}

		break; // done reading prefixes
	}

	/* Opcode byte */
	if (opcode == 0x0F) { /* X86_OPC_PREFIX /* /* TWO BYTE OPCODE */
		opcode = RAM[pc++];
		decode = decode_table_two[opcode];
		instr->is_two_byte_instr = 1;
	}
	else {
		decode = decode_table_one[opcode];
	}

	instr->opcode = opcode;
	instr->type = decode & X86_INSTR_OPC_MASK;
	instr->flags = decode & ~X86_INSTR_OPC_MASK;

	// Detect X86_OPC_UNDEFINED cases in decode_table_one and decode_table_two, which are forbidden to have flags :
	if (instr->flags == 0) /* Unrecognized? */
		return -1;

	if (op_size_override) {
		if (instr->flags & WIDTH_DWORD) {
			instr->flags &= ~WIDTH_DWORD;
			instr->flags |= WIDTH_WORD;
		}
		else if (instr->flags & WIDTH_QWORD) {
			instr->flags &= ~WIDTH_QWORD;
			instr->flags |= WIDTH_DWORD;
		}
	}

	if (instr->type == 0) { // This handles X86_OPC_DIFF_SYNTAX, X86_OPC_UNDEFINED and all GROUP_*'s :
		switch ((use_intel << DIFF_SYNTAX_USE_INTEL_SHIFT) | (op_size_override << DIFF_SYNTAX_SIZE_OVERRIDE_SHIFT) | instr->opcode) {
		// Handle decode_table_one entries marked X86_OPC_DIFF_SYNTAX :
		case 0x62:
			instr->type = X86_OPC_BOUND;
			instr->flags |= WIDTH_DWORD;
			break;
		case 0x62 | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_BOUND;
			instr->flags |= WIDTH_WORD;
			break;
		case 0x62 | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_BOUND;
			instr->flags |= WIDTH_QWORD;
			break;
		case 0x62 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_BOUND;
			instr->flags |= WIDTH_DWORD;
			break;
		case 0x98:
			instr->type = X86_OPC_CWTL;
			break;
		case 0x98 | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_CBTV;
			break;
		case 0x98 | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_CWDE;
			break;
		case 0x98 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_CBW;
			break;
		case 0x99:
			instr->type = X86_OPC_CLTD;
			break;
		case 0x99 | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_CWTD;
			break;
		case 0x99 | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_CDQ;
			break;
		case 0x99 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_CWD;
			break;
		case 0x9A:
		case 0x9A | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_LCALL;
			break;
		case 0x9A | DIFF_SYNTAX_USE_INTEL:
		case 0x9A | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_CALL;
			break;
		case 0xCA:
		case 0xCA | DIFF_SYNTAX_SIZE_OVERRIDE:
		case 0xCB:
		case 0xCB | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_LRET;
			break;
		case 0xCA | DIFF_SYNTAX_USE_INTEL:
		case 0xCA | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
		case 0xCB | DIFF_SYNTAX_USE_INTEL:
		case 0xCB | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_RETF;
			break;
		case 0xEA:
		case 0xEA | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_LJMP;
			break;
		case 0xEA | DIFF_SYNTAX_USE_INTEL:
		case 0xEA | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_JMP;
			break;
		// Handle decode_table_two entries marked X86_OPC_DIFF_SYNTAX :
		case 0xB6:
		case 0xB6 | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_MOVZXB;
			break;
		case 0xB6 | DIFF_SYNTAX_USE_INTEL:
		case 0xB6 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
		case 0xB7 | DIFF_SYNTAX_USE_INTEL:
		case 0xB7 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_MOVZX;
			break;
		case 0xB7:
		case 0xB7 | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_MOVZXW;
			break;
		case 0xBE:
		case 0xBE | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_MOVSXB;
			break;
		case 0xBE | DIFF_SYNTAX_USE_INTEL:
		case 0xBE | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
		case 0xBF | DIFF_SYNTAX_USE_INTEL:
		case 0xBF | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
			instr->type = X86_OPC_MOVSX;
			break;
		case 0xBF:
		case 0xBF | DIFF_SYNTAX_SIZE_OVERRIDE:
			instr->type = X86_OPC_MOVSXW;
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
				if (instr->type == X86_OPC_TEST) {
					instr->flags &= ~ADDRMOD_RM;
					instr->flags |= ADDRMOD_IMM_RM;
				}
				break;
			case GROUP_4:
				instr->type = grp4_decode_table[instr->reg_opc];
				break;
			case GROUP_5:
				instr->type = grp5_decode_table[instr->reg_opc];
				if (instr->type == X86_OPC_DIFF_SYNTAX) {
					switch ((use_intel << DIFF_SYNTAX_USE_INTEL_SHIFT) | instr->reg_opc) {
					// Handle grp5_decode_table entries marked X86_OPC_DIFF_SYNTAX :
					case 0x3:
						instr->type = X86_OPC_LCALL;
						break;
					case 0x3 | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_CALL;
						break;
					case 0x5:
						instr->type = X86_OPC_LJMP;
						break;
					case 0x5 | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_JMP;
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
				case X86_OPC_DIFF_SYNTAX:
					switch ((use_intel << DIFF_SYNTAX_USE_INTEL_SHIFT) | (op_size_override << DIFF_SYNTAX_SIZE_OVERRIDE_SHIFT) | instr->reg_opc) {
					// Handle grp7_decode_table entries marked X86_OPC_DIFF_SYNTAX :
					case 0x0:
						instr->type = X86_OPC_SGDTL;
						break;
					case 0x0 | DIFF_SYNTAX_SIZE_OVERRIDE:
						instr->type = X86_OPC_SGDTW;
						break;
					case 0x0 | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_SGDTD;
						break;
					case 0x0 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_SGDTW;
						break;
					case 0x1:
						instr->type = X86_OPC_SIDTL;
						break;
					case 0x1 | DIFF_SYNTAX_SIZE_OVERRIDE:
						instr->type = X86_OPC_SIDTW;
						break;
					case 0x1 | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_SIDTD;
						break;
					case 0x1 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_SIDTW;
						break;
					case 0x2:
						instr->type = X86_OPC_LGDTL;
						break;
					case 0x2 | DIFF_SYNTAX_SIZE_OVERRIDE:
						instr->type = X86_OPC_LGDTW;
						break;
					case 0x2 | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_LGDTD;
						break;
					case 0x2 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_LGDTW;
						break;
					case 0x3:
						instr->type = X86_OPC_LIDTL;
						break;
					case 0x3 | DIFF_SYNTAX_SIZE_OVERRIDE:
						instr->type = X86_OPC_LIDTW;
						break;
					case 0x3 | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_LIDTD;
						break;
					case 0x3 | DIFF_SYNTAX_SIZE_OVERRIDE | DIFF_SYNTAX_USE_INTEL:
						instr->type = X86_OPC_LIDTW;
						break;
					default:
						break;
					}
					break;
				case X86_OPC_LMSW:
				case X86_OPC_SMSW:
					instr->flags &= ~WIDTH_DWORD;
					instr->flags |= WIDTH_WORD;
					break;
				case X86_OPC_INVLPG:
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
