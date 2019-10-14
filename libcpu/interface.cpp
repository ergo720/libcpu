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
#include "llvm/ExecutionEngine/Orc/LLJIT.h"

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

libcpu_status
cpu_new(cpu_arch_t arch, uint32_t flags, uint32_t arch_flags, cpu_t *&out)
{
	cpu_t *cpu;

	cpu = new cpu_t();
	if (cpu == nullptr) {
		return LIBCPU_NO_MEMORY;
	}

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
			cpu_free(cpu);
			return LIBCPU_INVALID_PARAMETER;
	}

	std::unique_ptr<memory_region_t<addr_t>> mem_region(new memory_region_t<addr_t>);
	addr_t start;
	addr_t end;
	cpu->memory_space_tree = interval_tree<addr_t, std::unique_ptr<memory_region_t<addr_t>>>::create();
	start = 0;
	end = UINT64_MAX;
	cpu->memory_space_tree->insert(start, end, std::move(mem_region));
	std::unique_ptr<memory_region_t<io_port_t>> io_region(new memory_region_t<io_port_t>);
	io_port_t start_io;
	io_port_t end_io;
	cpu->io_space_tree = interval_tree<io_port_t, std::unique_ptr<memory_region_t<io_port_t>>>::create();
	start_io = 0;
	end_io = UINT16_MAX;
	cpu->io_space_tree->insert(start_io, end_io, std::move(io_region));

	cpu->code_start = 0;
	cpu->code_end = 0;
	cpu->code_entry = 0;
	cpu->tag = nullptr;

	uint32_t i;
	for (i = 0; i < sizeof(cpu->func)/sizeof(*cpu->func); i++)
		cpu->func[i] = nullptr;
	for (i = 0; i < sizeof(cpu->fp)/sizeof(*cpu->fp); i++)
		cpu->fp[i] = nullptr;
	cpu->functions = 0;

	cpu->flags_codegen = CPU_CODEGEN_OPTIMIZE;
	cpu->flags_debug = CPU_DEBUG_NONE;
	cpu->flags_hint = CPU_HINT_NONE;
	cpu->flags = 0;

	// init the frontend
	libcpu_status status = cpu->f.init(cpu, &cpu->info, &cpu->rf);
	if (!LIBCPU_CHECK_SUCCESS(status)) {
		cpu_free(cpu);
		return status;
	}

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
		cpu->ptr_gpr = nullptr;
		cpu->in_ptr_gpr = nullptr;
	}

	count = cpu->info.regclass_count[CPU_REGCLASS_XR];
	if (count != 0) {
		cpu->ptr_xr = (Value **)calloc(count, sizeof(Value *));
		cpu->in_ptr_xr = (Value **)calloc(count, sizeof(Value *));
	} else {
		cpu->ptr_xr = nullptr;
		cpu->in_ptr_xr = nullptr;
	}

	count = cpu->info.regclass_count[CPU_REGCLASS_FPR];
	if (count != 0) {
		cpu->ptr_fpr = (Value **)calloc(count, sizeof(Value *));
		cpu->in_ptr_fpr = (Value **)calloc(count, sizeof(Value *));
	} else {
		cpu->ptr_fpr = nullptr;
		cpu->in_ptr_fpr = nullptr;
	}

	if (cpu->info.psr_size != 0) {
		cpu->ptr_FLAG = (Value **)calloc(cpu->info.psr_size,
				sizeof(Value*));
		assert(cpu->ptr_FLAG != nullptr);
	}

	// init LLVM
	InitializeNativeTarget();
	InitializeNativeTargetAsmParser();
	InitializeNativeTargetAsmPrinter();
	cpu->ctx[cpu->functions] = new LLVMContext();
	if (cpu->ctx[cpu->functions] == nullptr) {
		cpu_free(cpu);
		return LIBCPU_NO_MEMORY;
	}
	cpu->mod[cpu->functions] = new Module(cpu->info.name, _CTX());
	if(cpu->mod[cpu->functions] == nullptr) {
		cpu_free(cpu);
		return LIBCPU_NO_MEMORY;
	}
	const auto& tt = cpu->mod[cpu->functions]->getTargetTriple();
	orc::JITTargetMachineBuilder jtmb =
		tt.empty() ? *orc::JITTargetMachineBuilder::detectHost()
		: orc::JITTargetMachineBuilder(Triple(tt));
	SubtargetFeatures features;
	StringMap<bool> host_features;
	if (sys::getHostCPUFeatures(host_features))
		for (auto &F : host_features) {
			features.AddFeature(F.first(), F.second);
		}
	jtmb.setCPU(sys::getHostCPUName())
		.addFeatures(features.getFeatures())
		.setRelocationModel(None)
		.setCodeModel(None);
	auto dl = jtmb.getDefaultDataLayoutForTarget();
	if (!dl) {
		cpu_free(cpu);
		return LIBCPU_LLVM_INTERNAL_ERROR;
	}
	cpu->dl = new DataLayout(*dl);
	if (cpu->dl == nullptr) {
		cpu_free(cpu);
		return LIBCPU_NO_MEMORY;
	}
	// XXX use sys::getHostNumPhysicalCores from LLVM to exclude logical cores?
	auto lazyjit = orc::LLLazyJIT::Create(std::move(jtmb), *dl, NULL, std::thread::hardware_concurrency());
	if (!lazyjit) {
		cpu_free(cpu);
		return LIBCPU_LLVM_INTERNAL_ERROR;
	}
	cpu->jit = std::move(*lazyjit);
	cpu->jit->setPartitionFunction(orc::CompileOnDemandLayer::compileRequested);
	cpu->jit->getMainJITDylib().setGenerator(
		*orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(*dl));

	// check if FP80 and FP128 are supported by this architecture.
	// XXX there is a better way to do this?
	std::string data_layout = cpu->dl->getStringRepresentation();
	if (data_layout.find("f80") != std::string::npos) {
		LOG("INFO: FP80 supported.\n");
		cpu->flags |= CPU_FLAG_FP80;
	}
	if (data_layout.find("f128") != std::string::npos) {
		LOG("INFO: FP128 supported.\n");
		cpu->flags |= CPU_FLAG_FP128;
	}

	// check if we need to swap guest memory.
	if (cpu->dl->isLittleEndian()
			^ IS_LITTLE_ENDIAN(cpu))
		cpu->flags |= CPU_FLAG_SWAPMEM;

	cpu->timer_total[TIMER_TAG] = 0;
	cpu->timer_total[TIMER_FE] = 0;
	cpu->timer_total[TIMER_BE] = 0;
	cpu->timer_total[TIMER_RUN] = 0;

	out = cpu;
	return LIBCPU_SUCCESS;
}

void
cpu_free(cpu_t *cpu)
{
	if (cpu->f.done != nullptr)
		cpu->f.done(cpu);
	if (cpu->jit != nullptr) {
		//if (cpu->cur_func != nullptr) {
		//	cpu->cur_func->eraseFromParent();
		//}
		llvm_shutdown();
		cpu->jit.reset(nullptr);
	}
	for (int i = 0; i < 1024; i++) {
		if (cpu->ctx[i])
			delete cpu->ctx[i];
	}
	for (int i = 0; i < 1024; i++) {
		if (cpu->mod[i])
			delete cpu->mod[i];
	}
	if (cpu->dl != nullptr)
		delete cpu->dl;
	if (cpu->ptr_FLAG != nullptr)
		free(cpu->ptr_FLAG);
	if (cpu->in_ptr_fpr != nullptr)
		free(cpu->in_ptr_fpr);
	if (cpu->ptr_fpr != nullptr)
		free(cpu->ptr_fpr);
	if (cpu->in_ptr_xr != nullptr)
		free(cpu->in_ptr_xr);
	if (cpu->ptr_xr != nullptr)
		free(cpu->ptr_xr);
	if (cpu->in_ptr_gpr != nullptr)
		free(cpu->in_ptr_gpr);
	if (cpu->ptr_gpr != nullptr)
		free(cpu->ptr_gpr);

	delete cpu;
}

libcpu_status
cpu_set_ram(cpu_t*cpu, uint8_t *r)
{
	if (r == nullptr) {
		return LIBCPU_INVALID_PARAMETER;
	}

	cpu->RAM = r;
	return LIBCPU_SUCCESS;
}

libcpu_status
cpu_set_flags_codegen(cpu_t *cpu, uint32_t f)
{
	if (f & ~(CPU_CODEGEN_OPTIMIZE | CPU_CODEGEN_TAG_LIMIT)) {
		return LIBCPU_INVALID_PARAMETER;
	}

	cpu->flags_codegen = f;
	return LIBCPU_SUCCESS;
}

libcpu_status
cpu_set_flags_debug(cpu_t *cpu, uint32_t f)
{
	if (f & ~(CPU_DEBUG_SINGLESTEP | CPU_DEBUG_SINGLESTEP_BB | CPU_DEBUG_PRINT_IR | CPU_DEBUG_PRINT_IR_OPTIMIZED
		| CPU_DEBUG_LOG | CPU_DEBUG_PROFILE | CPU_DEBUG_INTEL_SYNTAX)) {
		return LIBCPU_INVALID_PARAMETER;
	}

	if ((f & CPU_DEBUG_INTEL_SYNTAX) && (cpu->info.type != CPU_ARCH_X86)) {
		return LIBCPU_INVALID_PARAMETER;
	}

	cpu->flags_debug = f;
	return LIBCPU_SUCCESS;
}

libcpu_status
cpu_set_flags_hint(cpu_t *cpu, uint32_t f)
{
	if (f & ~(CPU_HINT_TRAP_RETURNS | CPU_HINT_TRAP_RETURNS_TWICE)) {
		return LIBCPU_INVALID_PARAMETER;
	}

	cpu->flags_hint = f;
	return LIBCPU_SUCCESS;
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

	if (cpu->ctx[cpu->functions] == nullptr) {
		cpu->ctx[cpu->functions] = new LLVMContext();
		assert(cpu->ctx[cpu->functions] != nullptr);
	}
	if (cpu->mod[cpu->functions] == nullptr) {
		cpu->mod[cpu->functions] = new Module(cpu->info.name, _CTX());
		assert(cpu->mod[cpu->functions] != nullptr);
	}

	/* create function and fill it with std basic blocks */
	cpu->cur_func = cpu_create_function(cpu, std::to_string(cpu->functions).c_str(), &bb_ret, &bb_trap, &label_entry);
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
		cpu->mod[cpu->functions]->print(llvm::errs(), nullptr);

	if (cpu->flags_codegen & CPU_CODEGEN_OPTIMIZE) {
		LOG("*** Optimizing...");
		optimize(cpu);
		LOG("done.\n");
		if (cpu->flags_debug & CPU_DEBUG_PRINT_IR_OPTIMIZED)
			cpu->mod[cpu->functions]->print(llvm::errs(), nullptr);
	}

	LOG("*** Translating...");
	update_timing(cpu, TIMER_BE, true);
	orc::ThreadSafeContext tsc(std::unique_ptr<LLVMContext>(cpu->ctx[cpu->functions]));
	orc::ThreadSafeModule tsm(std::unique_ptr<llvm::Module>(cpu->mod[cpu->functions]), tsc);
	auto err = cpu->jit->addLazyIRModule(std::move(tsm));
	assert(!err);
	auto fp = (void*)(cpu->jit->lookup(cpu->cur_func->getName())->getAddress());
	cpu->fp[cpu->functions] = fp;
	assert(fp != nullptr);
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
			cpu->ctx[i] = nullptr;
			cpu->mod[i] = nullptr;
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

void
cpu_flush(cpu_t *cpu)
{
	cpu->cur_func->eraseFromParent();

	cpu->functions = 0;

	// reset bb caching mapping
	cpu->func_bb.clear();
}

void
cpu_print_statistics(cpu_t *cpu)
{
	printf("tag = %8" PRId64 "\n", cpu->timer_total[TIMER_TAG]);
	printf("fe  = %8" PRId64 "\n", cpu->timer_total[TIMER_FE]);
	printf("be  = %8" PRId64 "\n", cpu->timer_total[TIMER_BE]);
	printf("run = %8" PRId64 "\n", cpu->timer_total[TIMER_RUN]);
}

libcpu_status
memory_init_region_ram(cpu_t *cpu, addr_t start, size_t size, int priority)
{
	addr_t end;
	std::unique_ptr<memory_region_t<addr_t>> ram(new memory_region_t<addr_t>);
	std::set<std::tuple<addr_t, addr_t, const std::unique_ptr<memory_region_t<addr_t>> &>> out;

	if (size == 0) {
		return LIBCPU_INVALID_PARAMETER;
	}

	end = start + size - 1;
	cpu->memory_space_tree->search(start, end, out);

	for (auto &region : out) {
		if (std::get<2>(region)->priority == priority) {
			return LIBCPU_INVALID_PARAMETER;
		}
	}

	ram->start = start;
	ram->type = MEM_RAM;
	ram->priority = priority;

	if (cpu->memory_space_tree->insert(start, end, std::move(ram))) {
		return LIBCPU_SUCCESS;
	}
	else {
		return LIBCPU_INVALID_PARAMETER;
	}
}

libcpu_status
memory_init_region_io(cpu_t *cpu, addr_t start, size_t size, bool io_space, fp_read read_func, fp_write write_func, void *opaque, int priority)
{
	bool inserted;

	if (size == 0) {
		return LIBCPU_INVALID_PARAMETER;
	}

	if (io_space) {
		io_port_t start_io;
		io_port_t end;
		std::unique_ptr<memory_region_t<io_port_t>> io(new memory_region_t<io_port_t>);
		std::set<std::tuple<io_port_t, io_port_t, const std::unique_ptr<memory_region_t<io_port_t>> &>> out;

		if (start > 65535 || (start + size) > 65536) {
			return LIBCPU_INVALID_PARAMETER;
		}

		start_io = static_cast<io_port_t>(start);
		end = start_io + size - 1;
		cpu->io_space_tree->search(start_io, end, out);

		for (auto &region : out) {
			if (std::get<2>(region)->priority == priority) {
				return LIBCPU_INVALID_PARAMETER;
			}
		}

		io->start = start_io;
		io->type = MEM_PMIO;
		io->priority = priority;
		if (read_func) {
			io->read_handler = read_func;
		}
		if (write_func) {
			io->write_handler = write_func;
		}
		if (opaque) {
			io->opaque = opaque;
		}

		inserted = cpu->io_space_tree->insert(start_io, end, std::move(io));
	}
	else {
		addr_t end;
		std::unique_ptr<memory_region_t<addr_t>> io(new memory_region_t<addr_t>);
		std::set<std::tuple<addr_t, addr_t, const std::unique_ptr<memory_region_t<addr_t>> &>> out;

		end = start + size - 1;
		cpu->memory_space_tree->search(start, end, out);

		for (auto &region : out) {
			if (std::get<2>(region)->priority == priority) {
				return LIBCPU_INVALID_PARAMETER;
			}
		}

		io->start = start;
		io->type = MEM_MMIO;
		io->priority = priority;
		if (read_func) {
			io->read_handler = read_func;
		}
		if (write_func) {
			io->write_handler = write_func;
		}
		if (opaque) {
			io->opaque = opaque;
		}

		inserted = cpu->memory_space_tree->insert(start, end, std::move(io));
	}

	if (inserted) {
		return LIBCPU_SUCCESS;
	}
	else {
		return LIBCPU_INVALID_PARAMETER;
	}
}

// XXX Are aliased regions allowed in the io space as well?
libcpu_status
memory_init_region_alias(cpu_t *cpu, addr_t alias_start, addr_t ori_start, size_t ori_size, int priority)
{
	addr_t end;
	memory_region_t<addr_t> *aliased_region;
	std::unique_ptr<memory_region_t<addr_t>> alias(new memory_region_t<addr_t>);
	std::set<std::tuple<addr_t, addr_t, const std::unique_ptr<memory_region_t<addr_t>> &>> out;

	if (ori_size == 0) {
		return LIBCPU_INVALID_PARAMETER;
	}

	aliased_region = nullptr;
	end = ori_start + ori_size - 1;
	cpu->memory_space_tree->search(ori_start, end, out);

	if (out.empty()) {
		return LIBCPU_INVALID_PARAMETER;
	}

	for (auto &region : out) {
		if ((std::get<0>(region) <= ori_start) && (std::get<1>(region) >= end)) {
			aliased_region = std::get<2>(region).get();
			break;
		}
	}

	if (!aliased_region) {
		return LIBCPU_INVALID_PARAMETER;
	}

	end = alias_start + ori_size - 1;
	cpu->memory_space_tree->search(alias_start, end, out);

	for (auto &region : out) {
		if (std::get<2>(region)->priority == priority) {
			return LIBCPU_INVALID_PARAMETER;
		}
	}

	alias->start = alias_start;
	alias->alias_offset = ori_start - aliased_region->start;
	alias->type = MEM_ALIAS;
	alias->priority = priority;
	alias->aliased_region = aliased_region;

	if (cpu->memory_space_tree->insert(alias_start, end, std::move(alias))) {
		return LIBCPU_SUCCESS;
	}
	else {
		return LIBCPU_INVALID_PARAMETER;
	}
}

libcpu_status
memory_destroy_region(cpu_t *cpu, addr_t start, size_t size, bool io_space)
{
	bool deleted;

	if (io_space) {
		io_port_t start_io;
		io_port_t end;

		start_io = static_cast<io_port_t>(start);
		end = start + size - 1;
		deleted = cpu->io_space_tree->erase(start_io, end);
	}
	else {
		addr_t end;

		end = start + size - 1;
		deleted = cpu->memory_space_tree->erase(start, end);
	}

	if (deleted) {
		return LIBCPU_SUCCESS;
	}
	else {
		return LIBCPU_INVALID_PARAMETER;
	}
}
