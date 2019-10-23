#ifndef _LIBCPU_H_
#define _LIBCPU_H_

#include "config.h"
#include "platform.h"
#include "interval_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <map>
#include <memory>

namespace llvm {
class LLVMContext;
class BasicBlock;
class ExecutionEngine;
class Function;
class Module;
class PointerType;
class StructType;
class Value;
class DataLayout;
namespace orc {
	class LLLazyJIT;
}
}

//////////////////////////////////////////////////////////////////////
// error flags
//////////////////////////////////////////////////////////////////////
enum libcpu_status {
	LIBCPU_NO_MEMORY = -3,
	LIBCPU_INVALID_PARAMETER,
	LIBCPU_LLVM_INTERNAL_ERROR,
	LIBCPU_SUCCESS,
};

#define LIBCPU_CHECK_SUCCESS(status) (((libcpu_status)(status)) == 0)

#include "types.h"
#include "fp_types.h"

using namespace llvm;

struct cpu;

typedef libcpu_status	(*fp_init)(struct cpu *cpu, struct cpu_archinfo *info, struct cpu_archrf *rf);
typedef void			(*fp_done)(struct cpu *cpu);
// @@@BEGIN_DEPRECATION
typedef StructType *	(*fp_get_struct_reg)(struct cpu *cpu);
typedef addr_t			(*fp_get_pc)(struct cpu *cpu, void *regs);
// @@@END_DEPRECATION
typedef void			(*fp_emit_decode_reg)(struct cpu *cpu, BasicBlock *bb);
typedef void			(*fp_spill_reg_state)(struct cpu *cpu, BasicBlock *bb);
typedef int				(*fp_tag_instr)(struct cpu *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc);
typedef int				(*fp_disasm_instr)(struct cpu *cpu, addr_t pc, char *line, unsigned int max_line);
typedef Value *			(*fp_translate_cond)(struct cpu *cpu, addr_t pc, BasicBlock *bb);
typedef int				(*fp_translate_instr)(struct cpu *cpu, addr_t pc, BasicBlock *bb);
// @@@BEGIN_DEPRECATION
// idbg support
typedef uint64_t		(*fp_get_psr)(struct cpu *cpu, void *regs);
typedef int				(*fp_get_reg)(struct cpu *cpu, void *regs, unsigned reg_no, uint64_t *value);
typedef int				(*fp_get_fp_reg)(struct cpu *cpu, void *regs, unsigned reg_no, void *value);
// @@@END_DEPRECATION

typedef struct {
	fp_init init;
	fp_done done;
// @@@BEGIN_DEPRECATION
	fp_get_pc get_pc;
// @@@END_DEPRECATION
	fp_emit_decode_reg emit_decode_reg;
	fp_spill_reg_state spill_reg_state;
	fp_tag_instr tag_instr;
	fp_disasm_instr disasm_instr;
	fp_translate_cond translate_cond;
	fp_translate_instr translate_instr;
// @@@BEGIN_DEPRECATION
	// idbg support
	fp_get_psr get_psr;
	fp_get_reg get_reg;
	fp_get_fp_reg get_fp_reg;
// @@@END_DEPRECATION
} arch_func_t;

typedef enum {
	CPU_ARCH_INVALID = 0, //XXX unused
	CPU_ARCH_6502,
	CPU_ARCH_M68K,
	CPU_ARCH_MIPS,
	CPU_ARCH_M88K,
	CPU_ARCH_ARM,
	CPU_ARCH_X86,
	CPU_ARCH_FAPRA
} cpu_arch_t;

enum {
	CPU_FLAG_ENDIAN_MASK   = (3 << 1),
	CPU_FLAG_ENDIAN_BIG    = (0 << 1),
	CPU_FLAG_ENDIAN_LITTLE = (1 << 1),
	CPU_FLAG_ENDIAN_MIDDLE = (2 << 1), // Middle endian (ie. PDP-11) NYI.
	CPU_FLAG_ENDIAN_NONE   = (3 << 1), // Mainly word oriented machines, where 
									   // word size is not a power of two.
									   // (ie. PDP-10)

	// @@@BEGIN_DEPRECATION
	CPU_FLAG_HARDWIRE_GPR0 = (1 << 4),
	CPU_FLAG_HARDWIRE_FPR0 = (1 << 4),
	// @@@END_DEPRECATION
	CPU_FLAG_DELAY_SLOT    = (1 << 5),
	CPU_FLAG_DELAY_NULLIFY = (1 << 6),

	// internal flags.
	CPU_FLAG_FP80          = (1 << 15), // FP80 is natively supported.
	CPU_FLAG_FP128         = (1 << 16), // FP128 is natively supported.
	CPU_FLAG_SWAPMEM       = (1 << 17), // Swap load/store
};

// @@@BEGIN_DEPRECATION
// Four register classes
enum {
	CPU_REGCLASS_GPR, // General Purpose
	CPU_REGCLASS_XR,  // Extra Registers, the core expects these to follow
				  // GPRs in the memory layout, they are kept separate
				  // to avoid confusing the client about the number of
				  // registers available.
	CPU_REGCLASS_FPR, // Floating Point
	CPU_REGCLASS_VR,  // Vector
	CPU_REGCLASS_COUNT
};
// @@@END_DEPRECATION

#define TIMER_COUNT	4
#define TIMER_TAG	0
#define TIMER_FE	1
#define TIMER_BE	2
#define TIMER_RUN	3

// flags' types
enum {
	CPU_FLAGTYPE_NONE = 0,
	CPU_FLAGTYPE_CARRY = 'C',
	CPU_FLAGTYPE_OVERFLOW = 'V',
	CPU_FLAGTYPE_NEGATIVE = 'N',
	CPU_FLAGTYPE_PARITY = 'P',
	CPU_FLAGTYPE_ZERO = 'Z'
};

// memory region type
enum mem_type_t {
	MEM_UNMAPPED,
	MEM_RAM,
	MEM_MMIO,
	MEM_PMIO,
	MEM_ALIAS,
};

// mmio/pmio access handlers
typedef uint32_t	(*fp_read)(addr_t addr, size_t size, void *opaque);
typedef void		(*fp_write)(addr_t addr, size_t size, uint32_t value, void *opaque);

typedef struct cpu_flags_layout {
	int shift;	/* bit position */
	char type;	/* 'N', 'V', 'Z', 'C' or 0 (some other flag unknown to the generic code) */
	const char *name; /* symbolic name */
} cpu_flags_layout_t;

// register layout types
enum {
	CPU_REGTYPE_INVALID = -1,
	CPU_REGTYPE_INT = 'i',
	CPU_REGTYPE_FLOAT = 'f',
	CPU_REGTYPE_VECTOR = 'v'
};

// register layout flags
enum {
	// special registers
	CPU_REGFLAG_SPECIAL_MASK = 0xf,
	CPU_REGFLAG_PC  = 1,
	CPU_REGFLAG_NPC = 2,
	CPU_REGFLAG_PSR = 3
};

template<typename T>
struct memory_region_t {
	T start;
	int type;
	int priority;
	fp_read read_handler;
	fp_write write_handler;
	void *opaque;
	addr_t alias_offset;
	memory_region_t<T> *aliased_region;
	memory_region_t() : start(0), alias_offset(0), type(MEM_UNMAPPED), priority(0), read_handler(nullptr), write_handler(nullptr),
		opaque(nullptr), aliased_region(nullptr) {};
};

template<typename T>
struct sort_by_priority
{
	bool operator() (const std::tuple<T, T, const std::unique_ptr<memory_region_t<T>> &> &lhs, const std::tuple<T, T, const std::unique_ptr<memory_region_t<T>> &> &rhs)
	{
		return std::get<2>(lhs)->priority > std::get<2>(rhs)->priority;
	}
};

typedef struct cpu_register_layout {
	char type;
	unsigned bits_size;
	unsigned aligned_size;
	unsigned byte_offset;
	unsigned flags;
	char const *name;
} cpu_register_layout_t;

typedef struct cpu_archinfo {
	cpu_arch_t type;
	
	char const *name;
	char const *full_name;

	uint32_t common_flags;
	uint32_t arch_flags;

	uint32_t delay_slots;

	uint32_t byte_size;
	uint32_t word_size;
	uint32_t float_size;
	uint32_t vector_size;
	uint32_t address_size;
	uint32_t psr_size;

	uint32_t min_page_size;
	uint32_t max_page_size;
	uint32_t default_page_size;

	cpu_register_layout_t const *register_layout;
	size_t regclass_count[CPU_REGCLASS_COUNT];
	size_t regclass_offset[CPU_REGCLASS_COUNT];
	uint32_t register_count2;
	cpu_flags_layout_t const *flags_layout;
	uint32_t flags_count;
} cpu_archinfo_t;

typedef struct cpu_archrf {
	// @@@BEGIN_DEPRECATION
	void *pc;  // Program Counter
	void *grf; // GP register file
	void *frf; // FP register file
	void *vrf; // Vector register file
	// @@@END_DEPRECATION
	void *storage;
} cpu_archrf_t;

typedef std::map<addr_t, BasicBlock *> bbaddr_map;
typedef std::map<Function *, bbaddr_map> funcbb_map;

typedef struct cpu {
	cpu_archinfo_t info;
	cpu_archrf_t rf;
	arch_func_t f;

	funcbb_map func_bb; // faster bb lookup

	uint16_t pc_offset;
	addr_t code_start;
	addr_t code_end;
	addr_t code_entry;
	uint32_t flags_codegen;
	uint32_t flags_debug;
	uint32_t flags_hint;
	uint32_t flags;
	uint8_t code_digest[20];
	FILE *file_entries;
	tag_t *tag;
	bool tags_dirty;
	void *fp[1024];
	Function *func[1024];
	Function *cur_func;
	uint32_t functions;
	uint8_t *RAM;
	Value *ptr_PC;
	Value *ptr_RAM;
	PointerType *type_pfunc_callout;
	Value *ptr_func_debug;

	Value *ptr_grf; // gpr register file
	Value **ptr_gpr; // GPRs
	Value **in_ptr_gpr;

	Value **ptr_xr; // XRs
	Value **in_ptr_xr;

	Value *ptr_frf; // fp register file
	Value **ptr_fpr; // FPRs
	Value **in_ptr_fpr;

	Value **ptr_FLAG; /* exploded version of flags */
	/* pointers to negative, overflow, zero and carry */
	Value *ptr_N;
	Value *ptr_V;
	Value *ptr_Z;
	Value *ptr_C;

	uint64_t timer_total[TIMER_COUNT];
	uint64_t timer_start[TIMER_COUNT];

	void *feptr; /* This pointer can be used freely by the frontend. */

	/* x86 specific variables */
	uint8_t prot; /* 1 when in protected mode */
	std::unique_ptr<interval_tree<addr_t, std::unique_ptr<memory_region_t<addr_t>>>> memory_space_tree;
	std::unique_ptr<interval_tree<io_port_t, std::unique_ptr<memory_region_t<io_port_t>>>> io_space_tree;
	std::set<std::tuple<addr_t, addr_t, const std::unique_ptr<memory_region_t<addr_t>> &>, sort_by_priority<addr_t>> memory_out;
	std::set<std::tuple<io_port_t, io_port_t, const std::unique_ptr<memory_region_t<io_port_t>> &>, sort_by_priority<io_port_t>> io_out;

	/* LLVM specific variables */
	std::unique_ptr<orc::LLLazyJIT> jit;
	LLVMContext *ctx[1024];
	Module *mod[1024];
	DataLayout *dl;
} cpu_t;

enum {
	JIT_RETURN_NOERR = 0,
	JIT_RETURN_FUNCNOTFOUND,
	JIT_RETURN_SINGLESTEP,
	JIT_RETURN_TRAP
};

//////////////////////////////////////////////////////////////////////
// codegen flags
//////////////////////////////////////////////////////////////////////
#define CPU_CODEGEN_NONE 0

// Run optimization passes on generated IR (disable for debugging)
#define CPU_CODEGEN_OPTIMIZE  (1<<1)

// Limits the DFS when tagging code, so that we don't
// translate all reachable code at a time, but only a
// certain amount of code in advance, and translate more
// on demand.
// If this is turned off, we do "entry caching", i.e. we
// create a file in /tmp that holds all entries to the code
// (i.e. all start addresses that can't be found automatically),
// and we start tagging at these addresses on load if the
// cache exists.
#define CPU_CODEGEN_TAG_LIMIT (1<<2)

//////////////////////////////////////////////////////////////////////
// debug flags
//////////////////////////////////////////////////////////////////////
#define CPU_DEBUG_NONE 0x00000000
#define CPU_DEBUG_SINGLESTEP			(1<<0)
#define CPU_DEBUG_SINGLESTEP_BB			(1<<1)
#define CPU_DEBUG_PRINT_IR				(1<<2)
#define CPU_DEBUG_PRINT_IR_OPTIMIZED	(1<<3)
#define CPU_DEBUG_LOG					(1<<4)
#define CPU_DEBUG_PROFILE				(1<<5)
#define CPU_DEBUG_INTEL_SYNTAX			(1<<6)
#define CPU_DEBUG_ALL 0xFFFFFFFF

//////////////////////////////////////////////////////////////////////
// debug flag shifts
//////////////////////////////////////////////////////////////////////
#define CPU_DEBUG_INTEL_SYNTAX_SHIFT		6

//////////////////////////////////////////////////////////////////////
// hints
//////////////////////////////////////////////////////////////////////
#define CPU_HINT_NONE 0x00000000
#define CPU_HINT_TRAP_RETURNS		(1<<0)
#define CPU_HINT_TRAP_RETURNS_TWICE	(1<<1)

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

#define LOGGING (cpu->flags_debug & CPU_DEBUG_LOG)
#define LOG(...) do { if (LOGGING) printf(__VA_ARGS__); } while(0)

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

/*
 * type of the debug callback; second parameter is
 * pointer to CPU specific register struct
 */
typedef void (*debug_function_t)(cpu_t*);

//////////////////////////////////////////////////////////////////////

API_FUNC libcpu_status cpu_new(cpu_arch_t arch, uint32_t flags, uint32_t arch_flags, cpu_t *&out);
API_FUNC void cpu_free(cpu_t *cpu);
API_FUNC libcpu_status cpu_set_flags_codegen(cpu_t *cpu, uint32_t f);
API_FUNC libcpu_status cpu_set_flags_hint(cpu_t *cpu, uint32_t f);
API_FUNC libcpu_status cpu_set_flags_debug(cpu_t *cpu, uint32_t f);
API_FUNC void cpu_tag(cpu_t *cpu, addr_t pc);
API_FUNC int cpu_run(cpu_t *cpu, debug_function_t debug_function);
API_FUNC void cpu_translate(cpu_t *cpu);
API_FUNC libcpu_status cpu_set_ram(cpu_t *cpu, uint8_t *RAM);
API_FUNC void cpu_flush(cpu_t *cpu);
API_FUNC void cpu_print_statistics(cpu_t *cpu);

/* runs the interactive debugger */
API_FUNC int cpu_debugger(cpu_t *cpu, debug_function_t debug_function);

/* for now, these only have an effect on the x86 arch */
API_FUNC libcpu_status memory_init_region_ram(cpu_t *cpu, addr_t start, size_t size, int priority);
API_FUNC libcpu_status memory_init_region_io(cpu_t *cpu, addr_t start, size_t size, bool io_space, fp_read read_func, fp_write write_func, void* opaque, int priority);
API_FUNC libcpu_status memory_init_region_alias(cpu_t *cpu, addr_t alias_start, addr_t ori_start, size_t ori_size, int priority);
API_FUNC libcpu_status memory_destroy_region(cpu_t *cpu, addr_t start, size_t size, bool io_space);

#endif
