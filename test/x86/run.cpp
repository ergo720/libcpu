#define START 0
#define ENTRY 0

#include <libcpu.h>
#include <string>
#include <stdexcept>
#include <fstream>

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

void print_help()
{
	static const char *help =
"usage: [options] <path of the binary to run>\n\
options: \n\
-p         Print llvm IR code\n\
-i         Use Intel syntax (default is AT&T)\n\
-c <addr>  Start address of code\n\
-e <addr>  Address of first instruction\n\
-s <size>  Size (in bytes) to allocate for RAM\n\
-h         Print this message\n";

	printf("%s", help);
}

int
main(int argc, char **argv)
{
	std::string executable;
	cpu_arch_t arch;
	cpu_t *cpu;
	uint8_t *RAM;
	size_t ramsize;
	addr_t code_start, code_entry;
	size_t length;

	int singlestep = SINGLESTEP_NONE;
	int log = 1;
	int print_ir = 0;
	int intel_syntax = 0;

	ramsize = 1 * 1024 * 1024;
	arch = CPU_ARCH_X86;
	code_start = START;
	code_entry = ENTRY;

	/* parameter parsing */
	if (argc < 2) {
		print_help();
		return 0;
	}

	for (int idx = 1; idx < argc; idx++) {
		try {
			std::string arg_str(argv[idx]);
			if (arg_str.size() == 2 && arg_str.front() == '-') {
				switch (arg_str.at(1))
				{
				case 'p':
					print_ir = 1;
					break;

				case 'i':
					intel_syntax = 1;
					break;

				case 'c':
					if (++idx == argc || argv[idx][0] == '-') {
						printf("Missing argument for option \"c\"\n");
						return 0;
					}
					code_start = std::stoull(std::string(argv[idx]), nullptr, 0);
					break;

				case 'e':
					if (++idx == argc || argv[idx][0] == '-') {
						printf("Missing argument for option \"e\"\n");
						return 0;
					}
					code_entry = std::stoull(std::string(argv[idx]), nullptr, 0);
					break;

				case 's':
					if (++idx == argc || argv[idx][0] == '-') {
						printf("Missing argument for option \"s\"\n");
						return 0;
					}
					ramsize = std::stoull(std::string(argv[idx]), nullptr, 0);
					break;

				case 'h':
					print_help();
					return 0;

				default:
					printf("Unknown option %s\n", arg_str.c_str());
					print_help();
					return 0;
				}
			}
			else if ((idx + 1) == argc) {
					executable = std::move(arg_str);
					break;
				}
			else {
				printf("Unknown option %s\n", arg_str.c_str());
				print_help();
				return 0;
			}
		}
		/* Handle exceptions thrown by std::stoull */
		catch (std::exception &e) {
			printf("Failed to parse addr and/or size arguments. The error was: %s\n", e.what());
			return 1;
		}
	}

#if TEST386_ASM

	ramsize = 1 * 1024 * 1024;
	code_start = 0xF0000;
	code_entry = 0xFFFF0;

#endif

	/* load code */
	std::ifstream ifs(executable, std::ios_base::in | std::ios_base::binary);
	if (!ifs.is_open()) {
		printf("Could not open binary file \"%s\"!\n", executable.c_str());
		return 1;
	}
	ifs.seekg(0, ifs.end);
	length = ifs.tellg();
	ifs.seekg(0, ifs.beg);

	/* Sanity checks */
	if (length == 0) {
		printf("Size of binary file \"%s\" detected as zero!\n", executable.c_str());
		return 1;
	}
	else if (length > ramsize - code_start) {
		printf("Binary file \"%s\" doesn't fit inside RAM!\n", executable.c_str());
		return 1;
	}

	RAM = (uint8_t*)malloc(ramsize);
	if (RAM == nullptr) {
		printf("Failed to allocate RAM buffer!\n");
		return 1;
	}

	if (!LIBCPU_CHECK_SUCCESS(cpu_new(arch, 0, 0, cpu))) {
		printf("Failed to initialize libcpu!\n");
		return 1;
	}

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
	
	cpu->code_start = code_start;
	cpu->code_end = cpu->code_start + (addr_t)length;
	cpu->code_entry = code_entry;
	ifs.read((char*)&RAM[cpu->code_start], length);
	ifs.close();

#if TEST386_ASM

	if (!LIBCPU_CHECK_SUCCESS(memory_init_region_ram(cpu, 0, ramsize, 1))) {
		printf("Failed to initialize ram memory!\n");
		return 1;
	}

	if (!LIBCPU_CHECK_SUCCESS(memory_init_region_alias(cpu, 0xFFFF0000, 0xF0000, 0x10000, 1))) {
		printf("Failed to initialize aliased ram memory!\n");
		return 1;
	}

	((reg_x86_t *)(cpu->rf.grf))->eip = 0xFFFFFFF0;

#else

	if (!LIBCPU_CHECK_SUCCESS(memory_init_region_ram(cpu, 0, ramsize, 1))) {
		printf("Failed to initialize ram memory!\n");
		return 1;
	}

#endif

	cpu_tag(cpu, cpu->code_entry);

	cpu_translate(cpu); /* force translation now */

	printf("\n*** Executing...\n");

	printf("GUEST run..."); fflush(stdout);

	cpu_run(cpu, debug_function);

	printf("done!\n");

	cpu_free(cpu);

	return 0;
}
