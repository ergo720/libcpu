/*
 * libcpu: function.cpp
 *
 * Create the master function and fill it with the helper
 * basic blocks
 */

#include <vector>

#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"

#include "libcpu.h"
#include "libcpu_llvm.h"
#include "frontend.h" // XXX for arch_flags_encode() / arch_flags_decode()

//////////////////////////////////////////////////////////////////////
// function
//////////////////////////////////////////////////////////////////////

static StructType *
get_struct_reg(cpu_t *cpu, const char* name) {
	std::vector<Type*>type_struct_reg_t_fields;

	uint32_t count, offset;
	
	// GPRs
	count = cpu->info.regclass_count[CPU_REGCLASS_GPR];
	offset = cpu->info.regclass_offset[CPU_REGCLASS_GPR];
	for (uint32_t n = 0; n < count; n++)
		type_struct_reg_t_fields.push_back(getIntegerType(cpu->info.register_layout[offset + n].bits_size));

	// XRs
	count = cpu->info.regclass_count[CPU_REGCLASS_XR];
	offset = cpu->info.regclass_offset[CPU_REGCLASS_XR];
	for (uint32_t n = 0; n < count; n++)
		type_struct_reg_t_fields.push_back(getIntegerType(cpu->info.register_layout[offset + n].bits_size));

//	type_struct_reg_t_fields.push_back(getIntegerType(cpu->info.address_size)); /* PC */

	return getNamedStructType(type_struct_reg_t_fields, name, /*isPacked=*/true);
}

static StructType *
get_struct_fp_reg(cpu_t *cpu, const char* name) {
	std::vector<Type*>type_struct_fp_reg_t_fields;

	uint32_t count, size;

	count = cpu->info.regclass_count[CPU_REGCLASS_FPR];
	size  = cpu->info.float_size;
	for (uint32_t n = 0; n < count; n++) {
		if (size == 80) {
			if ((cpu->flags & CPU_FLAG_FP80) == 0) {
				/* two 64bits words hold the data */
				type_struct_fp_reg_t_fields.push_back(getIntegerType(64));
				type_struct_fp_reg_t_fields.push_back(getIntegerType(64));
			} else {
				// XXX ensure it is aligned to 16byte boundary!
				type_struct_fp_reg_t_fields.push_back(getFloatType(80));
			}
		} else if (size == 128) {
			if ((cpu->flags & CPU_FLAG_FP128) == 0) {
				/* two 64bits words hold the data */
				type_struct_fp_reg_t_fields.push_back(getIntegerType(64));
				type_struct_fp_reg_t_fields.push_back(getIntegerType(64));
			} else {
				type_struct_fp_reg_t_fields.push_back(getFloatType(128));
			}
		} else {
			type_struct_fp_reg_t_fields.push_back(getFloatType(size));
		}
	}

	return getNamedStructType(type_struct_fp_reg_t_fields, name, /*isPacked=*/true);
}

static Value *
get_struct_member_pointer(cpu_t *cpu, Value *s, int index, BasicBlock *bb) {
	ConstantInt* const_0 = ConstantInt::get(XgetType(Int32Ty), 0);
	ConstantInt* const_index = ConstantInt::get(XgetType(Int32Ty), index);

	std::vector<Value*> ptr_11_indices;
	ptr_11_indices.push_back(const_0);
	ptr_11_indices.push_back(const_index);
	return (Value*) GetElementPtrInst::CreateInBounds(s, ptr_11_indices, "", bb);
}

static void
emit_decode_reg_helper(cpu_t *cpu, unsigned regclass,
	Value *rf, Value **in_ptr_r, Value **ptr_r, BasicBlock *bb)
{
	size_t count = cpu->info.regclass_count[regclass];
	size_t offset = cpu->info.regclass_offset[regclass];
#ifdef OPT_LOCAL_REGISTERS
	// decode struct reg and copy the registers into local variables
	for (uint32_t i = 0; i < count; i++) {
		char reg_name[16];
		snprintf(reg_name, sizeof(reg_name), "%s", cpu->info.register_layout[i + offset].name);

		in_ptr_r[i] = get_struct_member_pointer(cpu, rf, i + offset, bb);
		ptr_r[i] = new AllocaInst(getIntegerType(cpu->info.register_layout[i + offset].bits_size), 0, reg_name, bb);
		LoadInst* v = new LoadInst(in_ptr_r[i], "", false, bb);
		new StoreInst(v, ptr_r[i], false, bb);
	}
#else
	// just decode struct reg
	for (uint32_t i = 0; i < count; i++) 
		ptr_r[i] = get_struct_member_pointer(cpu, rf, i + offset, bb);
#endif
}

static inline unsigned
fp_alignment(unsigned width) {
	return ((width == 80 ? 128 : width) >> 3);
}

static void
emit_decode_fp_reg_helper(cpu_t *cpu, BasicBlock *bb)
{
	unsigned count = cpu->info.regclass_count[CPU_REGCLASS_FPR];
	Value **ptr_r = cpu->ptr_fpr;
#ifdef OPT_LOCAL_REGISTERS
	Value **in_ptr_r = cpu->in_ptr_fpr;
	unsigned offset = cpu->info.regclass_offset[CPU_REGCLASS_FPR];
	unsigned width = cpu->info.float_size;
	// decode struct reg and copy the registers into local variables
	for (uint32_t i = 0; i < count; i++) {
		char reg_name[16];
		if ((width == 80 && (cpu->flags & CPU_FLAG_FP80) == 0) ||
			(width == 128 && (cpu->flags & CPU_FLAG_FP128) == 0)) {
			snprintf(reg_name, sizeof(reg_name), "%s_%u_0", cpu->info.register_layout[i + offset].name, i);

			in_ptr_r[i*2+0] = get_struct_member_pointer(cpu, cpu->ptr_frf, i*2+0, bb);
			ptr_r[i*2+0] = new AllocaInst(getIntegerType(64), 0, 0, 0, reg_name, bb);
			LoadInst* v = new LoadInst(in_ptr_r[i*2+0], "", false, 0, bb);
			new StoreInst(v, ptr_r[i*2+0], false, 0, bb);

			snprintf(reg_name, sizeof(reg_name), "%s_%u_1", cpu->info.register_layout[i + offset].name, i);

			in_ptr_r[i*2+1] = get_struct_member_pointer(cpu, cpu->ptr_frf, i*2+1, bb);
			ptr_r[i*2+1] = new AllocaInst(getIntegerType(64), 0, 0, 0, reg_name, bb);
			v = new LoadInst(in_ptr_r[i*2+1], "", false, 0, bb);
			new StoreInst(v, ptr_r[i*2+1], false, 0, bb);
		} else {
			snprintf(reg_name, sizeof(reg_name), "%s", cpu->info.register_layout[i + offset].name);
			in_ptr_r[i] = get_struct_member_pointer(cpu, cpu->ptr_frf, i, bb);
			ptr_r[i] = new AllocaInst(getFloatType(width), 0, 0, fp_alignment(width), reg_name, bb);
			LoadInst* v = new LoadInst(in_ptr_r[i], "", false, fp_alignment(width), bb);
			new StoreInst(v, ptr_r[i], false, fp_alignment(width), bb);
		}
	}
#else
	// just decode struct reg
	for (uint32_t i = 0; i < count; i++) 
		ptr_r[i] = get_struct_member_pointer(cpu, cpu->ptr_frf, i, bb);
#endif
}

static void
emit_decode_reg(cpu_t *cpu, BasicBlock *bb)
{
	// GPRs
	emit_decode_reg_helper(cpu, CPU_REGCLASS_GPR,
		cpu->ptr_grf,
		cpu->in_ptr_gpr, cpu->ptr_gpr, bb);

	// XRs
	emit_decode_reg_helper(cpu, CPU_REGCLASS_XR,
		cpu->ptr_grf, // TODO : Shouldn't this be cpu->ptr_xr ?!?
		cpu->in_ptr_xr, cpu->ptr_xr, bb);

	// FPRs
	emit_decode_fp_reg_helper(cpu, bb);

	// PC pointer.
	IntegerType *intptr_type = cpu->dl->getIntPtrType(_CTX());
	Constant *v_pc = ConstantInt::get(intptr_type, (uintptr_t)cpu->rf.pc);
	cpu->ptr_PC = ConstantExpr::getIntToPtr(v_pc, PointerType::getUnqual(getIntegerType(cpu->info.address_size)));
	cpu->ptr_PC->setName("pc");

	// flags
	if (cpu->info.psr_size != 0) {
		// declare flags
		cpu_flags_layout_t const *flags_layout = cpu->info.flags_layout;
		for (size_t i = 0; i < cpu->info.flags_count; i++) {
			Value *f = new AllocaInst(getIntegerType(1), 0, flags_layout[i].name,
					bb);
			cpu->ptr_FLAG[flags_layout[i].shift] = f;
			/* set pointers to standard NVZC flags */
			switch (flags_layout[i].type) {
				case CPU_FLAGTYPE_NEGATIVE:
					cpu->ptr_N = f;
					break;
				case CPU_FLAGTYPE_OVERFLOW:
					cpu->ptr_V = f;
					break;
				case CPU_FLAGTYPE_ZERO:
					cpu->ptr_Z = f;
					break;
				case CPU_FLAGTYPE_CARRY:
					cpu->ptr_C = f;
					break;
			}
		}

		// decode P
		Value *flags = new LoadInst(cpu->ptr_xr[0], "", false, bb);
		arch_flags_decode(cpu, flags, bb);
	}
	
	// frontend specific part
	if (cpu->f.emit_decode_reg != nullptr)
		cpu->f.emit_decode_reg(cpu, bb);
}

static void
spill_reg_state_helper(uint32_t count, Value **in_ptr_r, Value **ptr_r,
	BasicBlock *bb)
{
#ifdef OPT_LOCAL_REGISTERS
	for (uint32_t i = 0; i < count; i++) {
		LoadInst* v = new LoadInst(ptr_r[i], "", false, bb);
		new StoreInst(v, in_ptr_r[i], false, bb);
	}
#endif
}

static void
spill_fp_reg_state_helper(cpu_t *cpu, BasicBlock *bb)
{
#ifdef OPT_LOCAL_REGISTERS
	size_t count = cpu->info.regclass_count[CPU_REGCLASS_FPR];
	uint32_t width = cpu->info.float_size;
	Value **in_ptr_r = cpu->in_ptr_fpr;
	Value **ptr_r = cpu->ptr_fpr;
	for (uint32_t i = 0; i < count; i++) {
		if ((width == 80 && (cpu->flags & CPU_FLAG_FP80) == 0) ||
			(width == 128 && (cpu->flags & CPU_FLAG_FP128) == 0)) {
			LoadInst* v = new LoadInst(ptr_r[i*2+0], "", false, 0, bb);
			new StoreInst(v, in_ptr_r[i*2+0], false, 0, bb);

			v = new LoadInst(ptr_r[i*2+1], "", false, 0, bb);
			new StoreInst(v, in_ptr_r[i*2+1], false, 0, bb);
		} else {
			LoadInst* v = new LoadInst(ptr_r[i], "", false,
				fp_alignment(width), bb);
			new StoreInst(v, in_ptr_r[i], false, fp_alignment(width), bb);
		}
	}
#endif
}

static void
spill_reg_state(cpu_t *cpu, BasicBlock *bb)
{
	// frontend specific part.
	if (cpu->f.spill_reg_state != nullptr)
		cpu->f.spill_reg_state(cpu, bb);

	// flags
	if (cpu->info.psr_size != 0) {
		Value *flags = arch_flags_encode(cpu, bb);
		new StoreInst(flags, cpu->ptr_xr[0], false, bb);
	}

	// GPRs
	spill_reg_state_helper(cpu->info.regclass_count[CPU_REGCLASS_GPR],
		cpu->in_ptr_gpr, cpu->ptr_gpr, bb);

	// XRs
	spill_reg_state_helper(cpu->info.regclass_count[CPU_REGCLASS_XR],
		cpu->in_ptr_xr, cpu->ptr_xr, bb);

	// FPRs
	spill_fp_reg_state_helper(cpu, bb);
}

Function*
cpu_create_function(cpu_t *cpu, const char *name,
	BasicBlock **p_bb_ret,
	BasicBlock **p_bb_trap,
	BasicBlock **p_label_entry)
{
	Function *func;

	// Type Definitions
	// - struct reg
	StructType *type_struct_reg_t = get_struct_reg(cpu, "struct.reg_t");
	// - struct reg *
	PointerType *type_pstruct_reg_t = PointerType::get(type_struct_reg_t, 0);
	// - struct fp_reg
	StructType *type_struct_fp_reg_t = get_struct_fp_reg(cpu, "struct.fp_reg_t");
	// - struct fp_reg *
	PointerType *type_pstruct_fp_reg_t = PointerType::get(type_struct_fp_reg_t, 0);
	// - uint8_t *
	PointerType *type_pi8 = PointerType::get(getIntegerType(8), 0);
	// - intptr *
	PointerType *type_intptr = PointerType::get(cpu->dl->getIntPtrType(_CTX()), 0);
	// - (*f)(cpu_t *) [debug_function() function pointer]
	std::vector<Type*>type_func_callout_args;
	type_func_callout_args.push_back(type_intptr);	/* intptr *cpu */
	FunctionType *type_func_callout = FunctionType::get(
		XgetType(VoidTy),	/* Result */
		type_func_callout_args,	/* Params */
		false);		      	/* isVarArg */
	cpu->type_pfunc_callout = PointerType::get(type_func_callout, 0);

	// - (*f)(uint8_t *, reg_t *, fp_reg_t *, (*)(...)) [jitmain() function pointer)
	std::vector<Type*>type_func_args;
	type_func_args.push_back(type_pi8);				/* uint8_t *RAM */
	type_func_args.push_back(type_pstruct_reg_t);	/* reg_t *reg */
	type_func_args.push_back(type_pstruct_fp_reg_t);	/* fp_reg_t *fp_reg */
	type_func_args.push_back(cpu->type_pfunc_callout);	/* (*debug)(...) */
	FunctionType* type_func = FunctionType::get(
		getIntegerType(32),		/* Result */
		type_func_args,		/* Params */
		false);						/* isVarArg */

	// Function Declarations
	func = Function::Create(
		type_func,				/* Type */
		GlobalValue::ExternalLinkage,	/* Linkage */
		name,	/* Name */
		cpu->mod[cpu->functions]);
	func->setCallingConv(CallingConv::C);
	func->addAttribute(1U, Attribute::NoCapture);
	func->addAttribute(4294967295U, Attribute::NoUnwind);

	// args
	Function::arg_iterator args = func->arg_begin();
	cpu->ptr_RAM = args++;
	cpu->ptr_RAM->setName("RAM");
	cpu->ptr_grf = args++;
	cpu->ptr_grf->setName("grf");
	cpu->ptr_frf = args++;
	cpu->ptr_frf->setName("frf");
	cpu->ptr_func_debug = args++;
	cpu->ptr_func_debug->setName("debug");

	// entry basicblock
	BasicBlock *label_entry = BasicBlock::Create(_CTX(), "entry", func, 0);
	emit_decode_reg(cpu, label_entry);

	// create exit code
	Value *exit_code = new AllocaInst(getIntegerType(32), 0, "exit_code", label_entry);
	// assume JIT_RETURN_FUNCNOTFOUND or JIT_RETURN_SINGLESTEP if in in single step.
	new StoreInst(ConstantInt::get(XgetType(Int32Ty),
					(cpu->flags_debug & (CPU_DEBUG_SINGLESTEP | CPU_DEBUG_SINGLESTEP_BB)) ? JIT_RETURN_SINGLESTEP :
					JIT_RETURN_FUNCNOTFOUND), exit_code, false, 0, label_entry);

#if 0 // bad for debugging, minimal speedup
	/* make the RAM pointer a constant */
	PointerType* type_pi8 = PointerType::get(IntegerType::get(8), 0);
	cpu->ptr_RAM = ConstantExpr::getCast(Instruction::IntToPtr, ConstantInt::get(Type::Int64Ty, (uint64_t)(long)cpu->RAM), type_pi8);
#endif

	// create ret basicblock
	BasicBlock *bb_ret = BasicBlock::Create(_CTX(), "ret", func, 0);  
	spill_reg_state(cpu, bb_ret);
	ReturnInst::Create(_CTX(), new LoadInst(exit_code, "", false, 0, bb_ret), bb_ret);
	// create trap return basicblock
	BasicBlock *bb_trap = BasicBlock::Create(_CTX(), "trap", func, 0);  
	new StoreInst(ConstantInt::get(XgetType(Int32Ty), JIT_RETURN_TRAP), exit_code, false, 0, bb_trap);
	// return
	BranchInst::Create(bb_ret, bb_trap);

	*p_bb_ret = bb_ret;
	*p_bb_trap = bb_trap;
	*p_label_entry = label_entry;
	return func;
}
