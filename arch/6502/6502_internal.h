/*
 * libcpu: 6502_internal.h
 *
 * prototypes of functions exported to core
 */

int         arch_6502_tag_instr(cpu_t *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc);
int         arch_6502_disasm_instr(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);
Value      *arch_6502_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb);
int         arch_6502_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb);
