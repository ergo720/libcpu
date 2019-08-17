/*
 * libcpu: interface.cpp
 *
 * This is the interface to the client.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <memory>

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Utils/Cloning.h"
#if defined _WIN32
#include "llvm/ExecutionEngine/MCJIT.h"
//#include "llvm/ExecutionEngine/OrcMCJITReplacement.h"
//#include "llvm/ExecutionEngine/Interpreter.h"
#endif

/* project global headers */
#include "libcpu.h"
#include "libcpu_llvm.h"
#include "tag.h"
#include "translate_all.h"
#include "translate_singlestep.h"
#include "translate_singlestep_bb.h"
#include "function.h"
#include "optimize.h"
#include "stat.h"
#include "x86_internal.h"

/* architecture descriptors */
extern arch_func_t arch_func_6502;
extern arch_func_t arch_func_m68k;
extern arch_func_t arch_func_mips;
extern arch_func_t arch_func_m88k;
extern arch_func_t arch_func_arm;
extern arch_func_t arch_func_x86;
extern arch_func_t arch_func_fapra;

#define IS_LITTLE_ENDIAN(cpu) (((cpu)->info.common_flags & CPU_FLAG_ENDIAN_MASK) == CPU_FLAG_ENDIAN_LITTLE)

static inline bool
is_valid_gpr_size(cpu_t *cpu, uint32_t offset, uint32_t count)
{
	for (uint32_t idx = offset; idx < count + offset; idx++) {
		size_t size = cpu->info.register_layout[idx].bits_size;
		switch (size) {
		case 0: case 1: case 8: case 16: case 32: case 64:
			break;
		default:
			return false;
		}
	}
	return true;
}

static inline bool
is_valid_fpr_size(cpu_t *cpu, uint32_t offset, uint32_t count)
{
	for (uint32_t idx = offset; idx < count + offset; idx++) {
		size_t size = cpu->info.register_layout[idx].bits_size;
		switch (size) {
		case 0: case 32: case 64: case 80: case 128:
			break;
		default:
			return false;
		}
	}
	return true;
}

static inline bool
is_valid_vr_size(cpu_t *cpu, uint32_t offset, uint32_t count)
{
	for (uint32_t idx = offset; idx < count + offset; idx++) {
		size_t size = cpu->info.register_layout[idx].bits_size;
		switch (size) {
		case 0: case 64: case 128:
			break;
		default:
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
// cpu_t
//////////////////////////////////////////////////////////////////////

cpu_t *
cpu_new(cpu_arch_t arch, uint32_t flags, uint32_t arch_flags)
{
	cpu_t *cpu;

	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();

	cpu = new cpu_t();
	assert(cpu != NULL);

	cpu->ctx = new LLVMContext();
	cpu->info.type = arch;
	cpu->info.name = "noname";
	cpu->info.common_flags = flags;
	cpu->info.arch_flags = arch_flags;

	switch (arch) {
		case CPU_ARCH_6502:
			cpu->f = arch_func_6502;
			break;
		case CPU_ARCH_M68K:
			cpu->f = arch_func_m68k;
			break;
		case CPU_ARCH_MIPS:
			cpu->f = arch_func_mips;
			break;
		case CPU_ARCH_M88K:
			cpu->f = arch_func_m88k;
			break;
		case CPU_ARCH_ARM:
			cpu->f = arch_func_arm;
			break;
		case CPU_ARCH_X86:
			cpu->f = arch_func_x86;
			break;
		case CPU_ARCH_FAPRA:
			cpu->f = arch_func_fapra;
			break;
		default:
			printf("illegal arch: %d\n", arch);
			exit(1);
	}

	cpu->code_start = 0;
	cpu->code_end = 0;
	cpu->code_entry = 0;
	cpu->tag = NULL;

	uint32_t i;
	for (i = 0; i < sizeof(cpu->func)/sizeof(*cpu->func); i++)
		cpu->func[i] = NULL;
	for (i = 0; i < sizeof(cpu->fp)/sizeof(*cpu->fp); i++)
		cpu->fp[i] = NULL;
	cpu->functions = 0;

	cpu->flags_codegen = CPU_CODEGEN_OPTIMIZE;
	cpu->flags_debug = CPU_DEBUG_NONE;
	cpu->flags_hint = CPU_HINT_NONE;
	cpu->flags = 0;

	// init the frontend
	cpu->f.init(cpu, &cpu->info, &cpu->rf);

	size_t offset = 0;
	for (unsigned rc = 0; rc < CPU_REGCLASS_COUNT; rc++) {
		cpu->info.regclass_offset[rc] = offset;
		offset += cpu->info.regclass_count[rc];
	}

	assert(is_valid_gpr_size(cpu, cpu->info.regclass_offset[CPU_REGCLASS_GPR], cpu->info.regclass_count[CPU_REGCLASS_GPR]) &&
		"the specified GPR size is not guaranteed to work");
	assert(is_valid_gpr_size(cpu, cpu->info.regclass_offset[CPU_REGCLASS_XR], cpu->info.regclass_count[CPU_REGCLASS_XR]) &&
		"the specified XR size is not guaranteed to work");
	assert(is_valid_fpr_size(cpu, cpu->info.regclass_offset[CPU_REGCLASS_FPR], cpu->info.regclass_count[CPU_REGCLASS_FPR]) &&
		"the specified FPR size is not guaranteed to work");
	assert(is_valid_vr_size(cpu, cpu->info.regclass_offset[CPU_REGCLASS_VR], cpu->info.regclass_count[CPU_REGCLASS_VR]) &&
		"the specified VR size is not guaranteed to work");


	uint32_t count = cpu->info.regclass_count[CPU_REGCLASS_GPR];
	if (count != 0) {
		cpu->ptr_gpr = (Value **)calloc(count, sizeof(Value *));
		cpu->in_ptr_gpr = (Value **)calloc(count, sizeof(Value *));
	} else {
		cpu->ptr_gpr = NULL;
		cpu->in_ptr_gpr = NULL;
	}

	count = cpu->info.regclass_count[CPU_REGCLASS_XR];
	if (count != 0) {
		cpu->ptr_xr = (Value **)calloc(count, sizeof(Value *));
		cpu->in_ptr_xr = (Value **)calloc(count, sizeof(Value *));
	} else {
		cpu->ptr_xr = NULL;
		cpu->in_ptr_xr = NULL;
	}

	count = cpu->info.regclass_count[CPU_REGCLASS_FPR];
	if (count != 0) {
		cpu->ptr_fpr = (Value **)calloc(count, sizeof(Value *));
		cpu->in_ptr_fpr = (Value **)calloc(count, sizeof(Value *));
	} else {
		cpu->ptr_fpr = NULL;
		cpu->in_ptr_fpr = NULL;
	}

	if (cpu->info.psr_size != 0) {
		cpu->ptr_FLAG = (Value **)calloc(cpu->info.psr_size,
				sizeof(Value*));
		assert(cpu->ptr_FLAG != NULL);
	}

	// init LLVM
	std::unique_ptr<Module> mod_ptr(new Module(cpu->info.name, _CTX()));
	cpu->mod = mod_ptr.get();
	assert(cpu->mod != NULL);
	EngineBuilder builder{std::move(mod_ptr)};
	builder.setEngineKind(EngineKind::Kind::JIT);
	cpu->exec_engine = builder.create();
	assert(cpu->exec_engine != NULL);

	// check if FP80 and FP128 are supported by this architecture.
	// XXX there is a better way to do this?
	std::string data_layout = cpu->exec_engine->getDataLayout().getStringRepresentation();
	if (data_layout.find("f80") != std::string::npos) {
		LOG("INFO: FP80 supported.\n");
		cpu->flags |= CPU_FLAG_FP80;
	}
	if (data_layout.find("f128") != std::string::npos) {
		LOG("INFO: FP128 supported.\n");
		cpu->flags |= CPU_FLAG_FP128;
	}

	// check if we need to swap guest memory.
	if (cpu->exec_engine->getDataLayout().isLittleEndian()
			^ IS_LITTLE_ENDIAN(cpu))
		cpu->flags |= CPU_FLAG_SWAPMEM;

	cpu->timer_total[TIMER_TAG] = 0;
	cpu->timer_total[TIMER_FE] = 0;
	cpu->timer_total[TIMER_BE] = 0;
	cpu->timer_total[TIMER_RUN] = 0;

	return cpu;
}

void
cpu_free(cpu_t *cpu)
{
	if (cpu->f.done != NULL)
		cpu->f.done(cpu);
	if (cpu->exec_engine != NULL) {
		if (cpu->cur_func != NULL) {
			cpu->cur_func->eraseFromParent();
		}
		delete cpu->exec_engine;
	}
	if (cpu->ctx != NULL)
		delete cpu->ctx;
	if (cpu->ptr_FLAG != NULL)
		free(cpu->ptr_FLAG);
	if (cpu->in_ptr_fpr != NULL)
		free(cpu->in_ptr_fpr);
	if (cpu->ptr_fpr != NULL)
		free(cpu->ptr_fpr);
	if (cpu->in_ptr_xr != NULL)
		free(cpu->in_ptr_xr);
	if (cpu->ptr_xr != NULL)
		free(cpu->ptr_xr);
	if (cpu->in_ptr_gpr != NULL)
		free(cpu->in_ptr_gpr);
	if (cpu->ptr_gpr != NULL)
		free(cpu->ptr_gpr);

	delete cpu;
}

void
cpu_set_ram(cpu_t*cpu, uint8_t *r)
{
	cpu->RAM = r;
}

void
cpu_set_flags_codegen(cpu_t *cpu, uint32_t f)
{
	cpu->flags_codegen = f;
}

void
cpu_set_flags_debug(cpu_t *cpu, uint32_t f)
{
	cpu->flags_debug = f;
}

void
cpu_set_flags_hint(cpu_t *cpu, uint32_t f)
{
	cpu->flags_hint = f;
}

void
cpu_tag(cpu_t *cpu, addr_t pc)
{
	update_timing(cpu, TIMER_TAG, true);
	tag_start(cpu, pc);
	update_timing(cpu, TIMER_TAG, false);
}

static void
cpu_translate_function(cpu_t *cpu)
{
	BasicBlock *bb_ret, *bb_trap, *label_entry, *bb_start;

	/* create function and fill it with std basic blocks */
	cpu->cur_func = cpu_create_function(cpu, "jitmain", &bb_ret, &bb_trap, &label_entry);
	cpu->func[cpu->functions] = cpu->cur_func;

	/* TRANSLATE! */
	update_timing(cpu, TIMER_FE, true);
	if (cpu->flags_debug & CPU_DEBUG_SINGLESTEP) {
		bb_start = cpu_translate_singlestep(cpu, bb_ret, bb_trap);
	} else if (cpu->flags_debug & CPU_DEBUG_SINGLESTEP_BB) {
		bb_start = cpu_translate_singlestep_bb(cpu, bb_ret, bb_trap);
	} else {
		bb_start = cpu_translate_all(cpu, bb_ret, bb_trap);
	}
	update_timing(cpu, TIMER_FE, false);

	/* finish entry basicblock */
	BranchInst::Create(bb_start, label_entry);

	/* make sure everything is OK */
	verifyFunction(*cpu->cur_func, &llvm::errs());

	if (cpu->flags_debug & CPU_DEBUG_PRINT_IR)
		cpu->mod->print(llvm::errs(), nullptr);

	if (cpu->flags_codegen & CPU_CODEGEN_OPTIMIZE) {
		LOG("*** Optimizing...");
		optimize(cpu);
		LOG("done.\n");
		if (cpu->flags_debug & CPU_DEBUG_PRINT_IR_OPTIMIZED)
			cpu->mod->print(llvm::errs(), nullptr);
	}

	LOG("*** Translating...");
	update_timing(cpu, TIMER_BE, true);
	cpu->exec_engine->finalizeObject();
	cpu->fp[cpu->functions] = cpu->exec_engine->getPointerToFunction(cpu->cur_func);
	assert(cpu->fp[cpu->functions] != NULL);
	update_timing(cpu, TIMER_BE, false);
	LOG("done.\n");

	cpu->functions++;
}

/* forces ahead of time translation (e.g. for benchmarking the run) */
void
cpu_translate(cpu_t *cpu)
{
	/* on demand translation */
	if (cpu->tags_dirty)
		cpu_translate_function(cpu);

	cpu->tags_dirty = false;
}

typedef int (*fp_t)(uint8_t *RAM, void *grf, void *frf, debug_function_t fp);

#ifdef __GNUC__
void __attribute__((noinline))
breakpoint() {
asm("nop");
}
#else
void breakpoint() {}
#endif

int
cpu_run(cpu_t *cpu, debug_function_t debug_function)
{
	addr_t pc = 0, orig_pc = 0;
	uint32_t i;
	int ret;
	bool success;
	bool do_translate = true;

	/* try to find the entry in all functions */
	while(true) {
		if (do_translate) {
			cpu_translate(cpu);
			pc = cpu->f.get_pc(cpu, cpu->rf.grf);
		}

		orig_pc = pc;
		success = false;
		for (i = 0; i < cpu->functions; i++) {
			fp_t FP = (fp_t)cpu->fp[i];
			update_timing(cpu, TIMER_RUN, true);
			breakpoint();
			ret = FP(cpu->RAM, cpu->rf.grf, cpu->rf.frf, debug_function);
			update_timing(cpu, TIMER_RUN, false);
			pc = cpu->f.get_pc(cpu, cpu->rf.grf);
			if (ret != JIT_RETURN_FUNCNOTFOUND)
				return ret;
			if (!is_inside_code_area(cpu, pc))
				return ret;
			if (pc != orig_pc) {
				success = true;
				break;
			}
		}
		if (!success) {
			LOG("{%" PRIx64 "}", pc);
			cpu_tag(cpu, pc);
			do_translate = true;
		}
	}
}
//printf("%d\n", __LINE__);

void
cpu_flush(cpu_t *cpu)
{
	cpu->cur_func->eraseFromParent();

	cpu->functions = 0;

	// reset bb caching mapping
	cpu->func_bb.clear();

//	delete cpu->mod;
//	cpu->mod = NULL;
}

void
cpu_print_statistics(cpu_t *cpu)
{
	printf("tag = %8" PRId64 "\n", cpu->timer_total[TIMER_TAG]);
	printf("fe  = %8" PRId64 "\n", cpu->timer_total[TIMER_FE]);
	printf("be  = %8" PRId64 "\n", cpu->timer_total[TIMER_BE]);
	printf("run = %8" PRId64 "\n", cpu->timer_total[TIMER_RUN]);
}
//printf("%s:%d\n", __func__, __LINE__);
