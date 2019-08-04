/*
 * libcpu: x86_internal.cpp
 *
 * prototypes of functions exported to core
 */

extern int         arch_x86_tag_instr(cpu_t *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc);
extern int         arch_x86_disasm_instr(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);
extern Value      *arch_8086_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb);
extern int         arch_x86_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb);
