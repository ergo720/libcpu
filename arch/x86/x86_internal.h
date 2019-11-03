/*
 * libcpu: x86_internal.cpp
 *
 * prototypes of functions exported to core and internal variables
 */

int				arch_x86_tag_instr(cpu_t *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc);
int				arch_x86_disasm_instr(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);
Value				*arch_x86_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb);
int				arch_x86_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb);

#define CRO_PE_SHIFT 0
#define CR0_PE_MASK (1 << CRO_PE_SHIFT)

#define R_CR0 ((reg_x86_t *)(cpu->rf.grf))->cr0

#define CPU_PE_MODE (R_CR0 & CR0_PE_MASK)
