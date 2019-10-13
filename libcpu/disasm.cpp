/*
 * libcpu: disasm.cpp
 *
 * Disassemble and print an instruction. This appends
 * instructions in a delay slot in square brackets.
 */
#include "libcpu.h"
#include "tag.h"
#include "x86_internal.h"

void disasm_instr(cpu_t *cpu, addr_t pc) {
	char disassembly_line1[80];
	char disassembly_line2[80];
	int bytes, i;

	bytes = cpu->f.disasm_instr(cpu, pc, disassembly_line1, sizeof(disassembly_line1));

	LOG(".,%08llx ", (unsigned long long)pc);
	// TODO this should probably use a function pointer to an arch specific memory function
	if (cpu->info.type != CPU_ARCH_X86) {
		for (i = 0; i < bytes; i++) {
			LOG("%02X ", cpu->RAM[pc + i]);
		}
	}
	else {
		for (i = 0; i < bytes; i++) {
			LOG("%02X ", arch_x86_mem_read8(cpu, &pc));
		}
	}
	LOG("%*s", (24-3*bytes)+1, ""); /* TODO make this arch neutral */

	/* delay slot */
	tag_t tag;
	addr_t dummy, dummy2;
	cpu->f.tag_instr(cpu, pc, &tag, &dummy, &dummy2);
	if (tag & TAG_DELAY_SLOT) {
		bytes = cpu->f.disasm_instr(cpu, pc + bytes, disassembly_line2, sizeof(disassembly_line2));
		LOG("%-23s [%s]\n", disassembly_line1, disassembly_line2);
	}
	else {
		LOG("%-23s\n", disassembly_line1);
	}
}
