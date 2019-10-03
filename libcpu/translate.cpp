/*
 * libcpu: translate.cpp
 *
 * This translates a single instruction by calling out into
 * the architecture dependent functions. It will optionally
 * create internal basic blocks if necessary.
 */

#include "llvm/IR/Instructions.h"

#include "libcpu.h"
#include "tag.h"
#include "basicblock.h"
/*
 * returns the basic block where code execution continues, or
 * nullptr if the instruction always branches away
 * (The caller needs this to link the basic block)
 */
BasicBlock *
translate_instr(cpu_t *cpu, addr_t pc, tag_t tag,
	BasicBlock *bb_target,	/* target for branch/call/rey */
	BasicBlock *bb_trap,	/* target for trap */
	BasicBlock *bb_next,	/* non-taken for conditional */
	BasicBlock *cur_bb)
{
	BasicBlock *bb_cond = nullptr;
	BasicBlock *bb_delay = nullptr;

	/* create internal basic blocks if needed */
	if (tag & TAG_CONDITIONAL)
		bb_cond = create_basicblock(cpu, pc, cpu->cur_func, BB_TYPE_COND);
	if ((tag & TAG_DELAY_SLOT) && (tag & TAG_CONDITIONAL))
		bb_delay = create_basicblock(cpu, pc, cpu->cur_func, BB_TYPE_DELAY);

	/* special case: delay slot */
	if (tag & TAG_DELAY_SLOT) {
		if (tag & TAG_CONDITIONAL) {
			addr_t delay_pc;
			// cur_bb:  if (cond) goto b_cond; else goto bb_delay;
			Value *c = cpu->f.translate_cond(cpu, pc, cur_bb);
			BranchInst::Create(bb_cond, bb_delay, c, cur_bb);
			// bb_cond: instr; delay; goto bb_target;
			pc += cpu->f.translate_instr(cpu, pc, bb_cond);
			delay_pc = pc;
			cpu->f.translate_instr(cpu, pc, bb_cond);
			BranchInst::Create(bb_target, bb_cond);
			// bb_cond: delay; goto bb_next;
			cpu->f.translate_instr(cpu, delay_pc, bb_delay);
			BranchInst::Create(bb_next, bb_delay);
		} else {
			// cur_bb:  instr; delay; goto bb_target;
			pc += cpu->f.translate_instr(cpu, pc, cur_bb);
			cpu->f.translate_instr(cpu, pc, cur_bb);
			BranchInst::Create(bb_target, cur_bb);
		}
		return nullptr; /* don't link */
	}

	/* no delay slot */
	if (tag & TAG_CONDITIONAL) {
		// cur_bb:  if (cond) goto b_cond; else goto bb_next;
		Value *c = cpu->f.translate_cond(cpu, pc, cur_bb);
		BranchInst::Create(bb_cond, bb_next, c, cur_bb);
		cur_bb = bb_cond;
	}

	cpu->f.translate_instr(cpu, pc, cur_bb);

	if (tag & (TAG_BRANCH | TAG_CALL | TAG_RET))
		BranchInst::Create(bb_target, cur_bb);
	else if (tag & TAG_TRAP)
		BranchInst::Create(bb_trap, cur_bb);

	if (tag & TAG_CONTINUE)
		return cur_bb;
	else
		return nullptr;
}
