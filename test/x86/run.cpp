#define START 0
#define ENTRY 0

#include "timings.h"

#include <libcpu.h>
#include "arch/m88k/m88k_isa.h"

#define RET_MAGIC 0x4D495354

static void
debug_function(cpu_t *cpu)
{
	fprintf(stderr, "%s:%u\n", __FILE__, __LINE__);
}

//////////////////////////////////////////////////////////////////////
#define SINGLESTEP_NONE	0
#define SINGLESTEP_STEP	1
#define SINGLESTEP_BB	2
//////////////////////////////////////////////////////////////////////

int
main(int argc, char **argv)
{
	char *executable = "";
	cpu_arch_t arch;
	cpu_t *cpu;
	uint8_t *RAM;
	FILE *f;
	size_t ramsize;

	int singlestep = SINGLESTEP_NONE;
	int log = 1;
	int print_ir = 0;
	int intel_syntax = 0;

	/* parameter parsing */
	if (argc < 2) {
		printf("Usage: %s [--print-ir] [--intel] <binary>\n", argv[0]);
		return 0;
	}

	for (int idx = 1; idx < argc; idx++) {
		char *arg;
		if (*(arg = argv[idx]) != '-') {
			executable = arg;
			continue;
		}
		if (*(arg = ++argv[idx]) == '-') {
			++arg;
			if (!strcmp(arg, "print-ir")) {
				print_ir = 1;
			}
			else if (!strcmp(arg, "intel")) {
				intel_syntax = 1;
			}
		}
	}

	arch = CPU_ARCH_X86;

	ramsize = 1*1024*1024;
	RAM = (uint8_t*)malloc(ramsize);

	cpu = cpu_new(arch, 0, 0);

	cpu_set_flags_codegen(cpu, CPU_CODEGEN_OPTIMIZE);
	cpu_set_flags_debug(cpu, 0
		| (print_ir? CPU_DEBUG_PRINT_IR : 0)
		| (print_ir? CPU_DEBUG_PRINT_IR_OPTIMIZED : 0)
		| (log? CPU_DEBUG_LOG :0)
		| (singlestep == SINGLESTEP_STEP? CPU_DEBUG_SINGLESTEP    : 0)
		| (singlestep == SINGLESTEP_BB?   CPU_DEBUG_SINGLESTEP_BB : 0)
		| (intel_syntax? CPU_DEBUG_INTEL_SYNTAX : 0)
		);

	cpu_set_ram(cpu, RAM);
	
	/* load code */
	if (!(f = fopen(executable, "rb"))) {
		printf("Could not open %s!\n", executable);
		return 2;
	}
	cpu->code_start = START;
	cpu->code_end = cpu->code_start + (addr_t)fread(&RAM[cpu->code_start], 1, ramsize-(size_t)cpu->code_start, f);
	fclose(f);
	cpu->code_entry = cpu->code_start + ENTRY;

	cpu_tag(cpu, cpu->code_entry);

	cpu_translate(cpu); /* force translation now */

	printf("\n*** Executing...\n");

	printf("GUEST run..."); fflush(stdout);

	cpu_run(cpu, debug_function);

	printf("done!\n");

	cpu_free(cpu);

	return 0;
}
