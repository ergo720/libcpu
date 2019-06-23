/*
 * libcpu: x86_decode.h
 *
 * instruction decoding
 */

#ifndef X86_DECODE_H
#define X86_DECODE_H

#include "libcpu.h"

#include <stdint.h>

enum x86_operand_type {
	OP_IMM,
	OP_MEM,
	OP_MEM_DISP,
	OP_REG,
	OP_SEG_REG,
	OP_REL,
};

enum x86_seg_override {
	NO_OVERRIDE,
	ES_OVERRIDE,
	CS_OVERRIDE,
	SS_OVERRIDE,
	DS_OVERRIDE,
	FS_OVERRIDE,
	GS_OVERRIDE,
};

enum x86_rep_prefix {
	NO_PREFIX,
	REPNZ_PREFIX,
	REPZ_PREFIX,
};

struct x86_operand {
	enum x86_operand_type	type;
	uint8_t			reg;
	uint16_t		seg_sel;
	int32_t			disp;		/* address displacement can be negative */
	union {
		uint32_t		imm;
		int32_t			rel;
	};
};

enum x86_instr_flags : uint64_t {
	MOD_RM			= (1ULL << 8),
	SIB				= (1ULL << 9),
	DIR_REVERSED		= (1ULL << 10),

	/* Operand sizes */
	WIDTH_BYTE		= (1ULL << 11),	/* 8 bits */
	WIDTH_WORD		= (1ULL << 12), /* 16 bits only */
	WIDTH_FULL		= (1ULL << 13),	/* 16 bits or 32 bits */
	WIDTH_MASK		= WIDTH_BYTE|WIDTH_WORD|WIDTH_FULL,

	/* Source operand */
	SRC_NONE		= (1ULL << 14),

	SRC_IMM			= (1ULL << 15),
	SRC_IMM8		= (1ULL << 16),
	SRC_IMM48		= (1ULL << 17),
	IMM_MASK		= SRC_IMM|SRC_IMM8|SRC_IMM48,

	SRC_REL			= (1ULL << 18),
	REL_MASK		= SRC_REL,

	SRC_REG			= (1ULL << 19),
	SRC_SEG_REG		= (1ULL << 20),
	SRC_ACC			= (1ULL << 21),
	SRC_MEM			= (1ULL << 22),
	SRC_MOFFSET		= (1ULL << 23),
	SRC_MEM_DISP_BYTE	= (1ULL << 24),	/* 8 bits */
	SRC_MEM_DISP_WORD	= (1ULL << 25), /* 16 bits */
	SRC_MEM_DISP_FULL	= (1ULL << 26), /* 32 bits */
	SRC_MASK		= SRC_NONE|IMM_MASK|REL_MASK|SRC_REG|SRC_SEG_REG|SRC_ACC|SRC_MEM|SRC_MOFFSET|SRC_MEM_DISP_BYTE|SRC_MEM_DISP_WORD|SRC_MEM_DISP_FULL,

	/* Destination operand */
	DST_NONE		= (1ULL << 27),

	DST_IMM16		= (1ULL << 28),
	DST_REG			= (1ULL << 29),
	DST_ACC			= (1ULL << 30),	/* AL/AX/EAX */
	DST_MEM			= (1ULL << 31),
	DST_MOFFSET		= (1ULL << 32),
	DST_MEM_DISP_BYTE	= (1ULL << 33),	/* 8 bits */
	DST_MEM_DISP_WORD	= (1ULL << 34),	/* 16 bits */
	DST_MEM_DISP_FULL	= (1ULL << 35),	/* 32 bits */
	DST_MASK		= DST_NONE|DST_IMM16|DST_REG|DST_ACC|DST_MOFFSET|DST_MEM|DST_MEM_DISP_BYTE|DST_MEM_DISP_WORD|DST_MEM_DISP_FULL,

	/* Third operand */
	OP3_IMM			= (1ULL << 36),
	OP3_IMM8		= (1ULL << 37),
	OP3_IMM_MASK		= OP3_IMM|OP3_IMM8,

	OP3_MASK		= OP3_IMM_MASK,

	MEM_DISP_MASK		= SRC_MEM|SRC_MEM_DISP_BYTE|SRC_MEM_DISP_WORD|SRC_MEM_DISP_FULL|DST_MEM|DST_MEM_DISP_BYTE|DST_MEM_DISP_WORD|DST_MEM_DISP_FULL,

	MOFFSET_MASK		= SRC_MOFFSET|DST_MOFFSET,

	GROUP_1			= (1ULL << 38),
	GROUP_2			= (1ULL << 39),
	GROUP_3			= (1ULL << 40),
	GROUP_4			= (1ULL << 41),
	GROUP_5			= (1ULL << 42),
	GROUP_MASK		= GROUP_1|GROUP_2|GROUP_3|GROUP_4|GROUP_5,
};

/*
 *	Addressing modes.
 */
enum x86_addmode : uint64_t {
	ADDMODE_ACC_MOFFSET	= SRC_ACC|DST_MOFFSET,		/* AL/AX/EAX -> moffset */
	ADDMODE_ACC_REG		= SRC_ACC|DST_REG,		/* AL/AX/EAX -> reg */
	ADDMODE_IMM		= SRC_IMM|DST_NONE,		/* immediate operand */
	ADDMODE_IMM8_RM		= SRC_IMM8|MOD_RM|DIR_REVERSED,	/* immediate8 -> register/memory */
	ADDMODE_IMM_RM		= SRC_IMM|MOD_RM|DIR_REVERSED,	/* immediate -> register/memory */
	ADDMODE_IMM_ACC		= SRC_IMM|DST_ACC,		/* immediate -> AL/AX/EAX */
	ADDMODE_IMM_REG		= SRC_IMM|DST_REG,		/* immediate -> register */
	ADDMODE_IMPLIED		= SRC_NONE|DST_NONE,		/* no operands */
	ADDMODE_MOFFSET_ACC	= SRC_MOFFSET|DST_ACC,		/* moffset -> AL/AX/EAX */
	ADDMODE_REG		= SRC_REG|DST_NONE,		/* register */
	ADDMODE_SEG_REG		= SRC_SEG_REG|DST_NONE,		/* segment register */
	ADDMODE_SEG_REG_RM	= SRC_SEG_REG|MOD_RM|DIR_REVERSED,		/* segment register -> register/memory */
	ADDMODE_REG_RM		= SRC_REG|MOD_RM|DIR_REVERSED,	/* register -> register/memory */
	ADDMODE_REL		= SRC_REL|DST_NONE,		/* relative */
	ADDMODE_RM_REG		= DST_REG|MOD_RM,		/* register/memory -> register */
	ADDMODE_RM		= DST_NONE|MOD_RM,	/* register/memory */
	ADDMODE_RM_IMM_REG	= DST_REG|MOD_RM|OP3_IMM,	/* register/memory, immediate -> register */
	ADDMODE_RM_IMM8_REG	= DST_REG|MOD_RM|OP3_IMM8,	/* register/memory, immediate8 -> register */
	ADDMODE_FAR_PTR		= DST_NONE|SRC_IMM48,	/* far pointer */
	ADDMODE_IMM8_IMM16	= SRC_IMM8|DST_IMM16, /* immediate8 -> immediate16 */
};

struct x86_instr {
	unsigned long		nr_bytes;

	uint8_t			opcode;		/* Opcode byte */
	uint8_t			width;
	uint8_t			mod;		/* Mod */
	uint8_t			rm;		/* R/M */
	uint8_t			reg_opc;	/* Reg/Opcode */
	uint8_t			base;		/* SIB base */
	uint8_t			idx;		/* SIB index */
	uint8_t			scale;		/* SIB scale */
	uint32_t		disp;		/* Address displacement */
	union {
		uint32_t		imm_data[2];	/* Immediate data; src/op3 (0), dst/seg sel (1) */
		int32_t			rel_data[2];	/* Relative address data */
	};

	unsigned long long	type;		/* See enum x86_instr_types */
	unsigned long long	flags;		/* See enum x86_instr_flags */
	enum x86_seg_override	seg_override;
	enum x86_rep_prefix	rep_prefix;
	unsigned char		lock_prefix;
	unsigned char		operand_size_override;
	unsigned char		addr_size_override;
	struct x86_operand	src;
	struct x86_operand	dst;
	struct x86_operand	third;
};

int
arch_x86_decode_instr(struct x86_instr *instr, uint8_t* RAM, addr_t pc);

int
arch_8086_instr_length(struct x86_instr *instr);

#endif /* X86_DECODE_H */
