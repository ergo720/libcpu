/*
 * libcpu: x86_internal.cpp
 *
 * prototypes of functions exported to core and internal variables
 */

int				arch_x86_tag_instr(cpu_t *cpu, addr_t pc, tag_t *tag, addr_t *new_pc, addr_t *next_pc);
int				arch_x86_disasm_instr(cpu_t *cpu, addr_t pc, char *line, unsigned int max_line);
Value				*arch_x86_translate_cond(cpu_t *cpu, addr_t pc, BasicBlock *bb);
int				arch_x86_translate_instr(cpu_t *cpu, addr_t pc, BasicBlock *bb);
JIT_EXTERNAL_CALL_C		uint8_t arch_x86_mem_read8(cpu_t *cpu, addr_t *addr);
JIT_EXTERNAL_CALL_C		uint16_t arch_x86_mem_read16(cpu_t *cpu, addr_t *addr);
JIT_EXTERNAL_CALL_C		uint32_t arch_x86_mem_read32(cpu_t *cpu, addr_t *addr);
JIT_EXTERNAL_CALL_C		void arch_x86_mem_write8(cpu_t *cpu, addr_t addr, uint8_t value);
JIT_EXTERNAL_CALL_C		void arch_x86_mem_write16(cpu_t *cpu, addr_t addr, uint16_t value);
JIT_EXTERNAL_CALL_C		void arch_x86_mem_write32(cpu_t *cpu, addr_t addr, uint32_t value);

#define CRO_PE_SHIFT 0
#define CR0_PE_MASK (1 << CRO_PE_SHIFT)
