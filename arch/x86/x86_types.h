/*
 * libcpu: x86_types.h
 *
 * the register file
 */

/* XXX: high and low byte endianess! */
#define DEFINE_SPLIT_REG8(_reg32, _reg16, _high, _low) \
	union {					\
		uint32_t		_reg32;	\
		uint16_t		_reg16;	\
		struct {			\
			uint8_t		_high;	\
			uint8_t		_low;	\
		};				\
	};

#define DEFINE_SPLIT_REG16(_reg32, _reg16) \
	union {					\
		uint32_t		_reg32;	\
		uint16_t		_reg16;	\
	};

#define DEFINE_REG80(_reg)			\
	struct {				\
		uint8_t		_reg[10];	\
	}

#define DEFINE_REG48(_reg)			\
	struct {				\
		uint8_t		_reg[6];	\
	}

#define DEFINE_REG32(_reg)			\
	struct {				\
		uint32_t		_reg;	\
	}

#define DEFINE_REG16(_reg)			\
	struct {				\
		uint16_t		_reg;	\
	}

PACKED(struct reg_x86_s {
	/* General registers */
	DEFINE_SPLIT_REG8(eax, ax, ah, al);
	DEFINE_SPLIT_REG8(ebx, bx, bh, bl);
	DEFINE_SPLIT_REG8(ecx, cx, ch, cl);
	DEFINE_SPLIT_REG8(edx, dx, dh, dl);
	/* Index registers */
	DEFINE_SPLIT_REG16(esi, si);
	DEFINE_SPLIT_REG16(edi, di);
	/* Pointer registers */
	DEFINE_SPLIT_REG16(ebp, bp);
	DEFINE_SPLIT_REG16(esp, sp);
	/* Special purpose registers */
	DEFINE_REG32(eip);
	/* Eflags register */
	DEFINE_REG32(eflags);
	/* Segment registers */
	DEFINE_REG16(cs);
	DEFINE_REG16(ds);
	DEFINE_REG16(ss);
	DEFINE_REG16(es);
	DEFINE_REG16(fs);
	DEFINE_REG16(gs);
	/* Control registers */
	DEFINE_REG32(cr0);
	DEFINE_REG32(cr1);
	DEFINE_REG32(cr2);
	DEFINE_REG32(cr3);
	DEFINE_REG32(cr4);
	/* FPU registers */
	DEFINE_REG80(st0);
	DEFINE_REG80(st1);
	DEFINE_REG80(st2);
	DEFINE_REG80(st3);
	DEFINE_REG80(st4);
	DEFINE_REG80(st5);
	DEFINE_REG80(st6);
	DEFINE_REG80(st7);
	DEFINE_REG16(ctrl);
	DEFINE_REG16(status);
	DEFINE_REG16(tag);
	DEFINE_REG16(opcode); // 11-bit reg
	DEFINE_REG48(last_instr_ptr);
	DEFINE_REG48(last_data_ptr);
	// TODO: GDTR, LDTR, IDTR, task, MTRR, MSR, MMX, SSE, debug regs
});
typedef struct reg_x86_s reg_x86_t;
