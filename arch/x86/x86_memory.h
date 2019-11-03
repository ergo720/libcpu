/*
 * libcpu: x86_memory.cpp
 *
 * memory accessors
 */

#pragma once

#include "llvm/support/SwapByteOrder.h"

#include "libcpu.h"


/*
 * RAM specific accessors
 */
template<typename T>
T arch_x86_ram_read(cpu_t *cpu, addr_t *pc)
{
	T value;

	memcpy(&value, &cpu->RAM[*pc], sizeof(T));
	*pc = *pc + sizeof(T);

	if (cpu->flags & CPU_FLAG_SWAPMEM && sizeof(T) != 1) {
		switch (sizeof(T)) {
		case 4: {
			value = sys::SwapByteOrder_32(value);
		}
		break;

		case 2: {
			value = sys::SwapByteOrder_16(value);
		}
		break;

		default:
			fprintf(stderr, "%s: invalid size %u specified\n", __func__, sizeof(T));
			exit(1);
		}
	}

	return value;
}

template<typename T>
void arch_x86_ram_write(cpu_t *cpu, addr_t *pc, T value)
{
	if (cpu->flags & CPU_FLAG_SWAPMEM && sizeof(T) != 1) {
		switch (sizeof(T)) {
		case 4: {
			value = sys::SwapByteOrder_32(value);
		}
		break;

		case 2: {
			value = sys::SwapByteOrder_16(value);
		}
		break;

		default:
			fprintf(stderr, "%s: invalid size %u specified\n", __func__, sizeof(T));
			exit(1);
		}
	}

	memcpy(&cpu->RAM[*pc], &value, sizeof(T));
	*pc = *pc + sizeof(T);

	return val;
}

/*
 * generic memory accessors
 */
template<typename T>
T arch_x86_mem_read(cpu_t *cpu, addr_t addr)
{
	addr_t end;

	end = addr + sizeof(T) - 1;
	cpu->memory_space_tree->search(addr, end, cpu->memory_out);

	if ((addr >= std::get<0>(*cpu->memory_out.begin())) && (end <= std::get<1>(*cpu->memory_out.begin()))) {
		switch (std::get<2>(*cpu->memory_out.begin())->type)
		{
		case MEM_RAM: {
			return arch_x86_ram_read<T>(cpu, &addr);
		}
		break;

		case MEM_MMIO: {
			T value = 0;
			memory_region_t<addr_t> *region = std::get<2>(*cpu->memory_out.begin()).get();
			if (region->read_handler) {
				value = region->read_handler(addr, sizeof(T), region->opaque);
			}
			else {
				printf("%s: unhandled MMIO read at address $%02llx with size %d\n", __func__, addr, sizeof(T));
			}
			return value;
		}
		break;

		case MEM_ALIAS: {
			memory_region_t<addr_t> *region = std::get<2>(*cpu->memory_out.begin()).get();
			addr_t alias_offset = region->alias_offset;
			while (region->aliased_region) {
				region = region->aliased_region;
				alias_offset += region->alias_offset;
			}
			return arch_x86_mem_read<T>(cpu, region->start + alias_offset + (addr - std::get<0>(*cpu->memory_out.begin())));
		}
		break;

		case MEM_UNMAPPED: {
			// XXX handle this properly instead of just aborting
			fprintf(stderr, "%s: memory access to unmapped memory at address $%02llx with size %d\n", __func__, addr, sizeof(T));
			exit(1);
		}
		break;

		default:
			assert(0 && "arch_x86_mem_read: unknown region type\n");
			return 0;
		}
	}
	else {
		// XXX handle this properly instead of just aborting
		fprintf(stderr, "%s: memory access at address $%02llx with size %d is not completely inside a memory region\n", __func__, addr, sizeof(T));
		exit(1);
	}
}

template<typename T>
void arch_x86_mem_write(cpu_t *cpu, addr_t addr, T value)
{
	addr_t end;

	end = addr + sizeof(T) - 1;
	cpu->memory_space_tree->search(addr, end, cpu->memory_out);

	if ((addr >= std::get<0>(*cpu->memory_out.begin())) && (end <= std::get<1>(*cpu->memory_out.begin()))) {
		switch (std::get<2>(*cpu->memory_out.begin())->type)
		{
		case MEM_RAM: {
			arch_x86_ram_write<T>(cpu, &addr, value);
		}
		break;

		case MEM_MMIO: {
			memory_region_t<addr_t> *region = std::get<2>(*cpu->memory_out.begin()).get();
			if (region->write_handler) {
				region->write_handler(addr, sizeof(T), value, region->opaque);
			}
			else {
				printf("%s: unhandled MMIO write at address $%02llx with size %d\n", __func__, addr, sizeof(T));
			}
		}
		break;

		case MEM_ALIAS: {
			memory_region_t<addr_t> *region = std::get<2>(*cpu->memory_out.begin()).get();
			addr_t alias_offset = region->alias_offset;
			while (region->aliased_region) {
				region = region->aliased_region;
				alias_offset += region->alias_offset;
			}
			arch_x86_mem_write<T>(cpu, region->start + alias_offset + (addr - std::get<0>(*cpu->memory_out.begin())), value);
		}
		break;

		case MEM_UNMAPPED: {
			// XXX handle this properly instead of just aborting
			fprintf(stderr, "%s: memory access to unmapped memory at address $%02llx with size %d\n", __func__, addr, sizeof(T));
			exit(1);
		}
		break;

		default:
			assert(0 && "arch_x86_mem_write: unknown region type\n");
			return;
		}
	}
	else {
		// XXX handle this properly instead of just aborting
		fprintf(stderr, "%s: memory access at address $%02llx with size %d is not completely inside a memory region\n", __func__, addr, sizeof(T));
		exit(1);
	}
}

/*
 * pmio specific accessors
 */
template<typename T>
T arch_x86_io_read(cpu_t *cpu, io_port_t addr)
{
	io_port_t end;

	end = addr + sizeof(T) - 1;
	cpu->io_space_tree->search(addr, end, cpu->io_out);

	if ((addr >= std::get<0>(*cpu->io_out.begin())) && (end <= std::get<1>(*cpu->io_out.begin()))) {
		switch (std::get<2>(*cpu->io_out.begin())->type)
		{
		case MEM_PMIO: {
			T value = 0;
			memory_region_t<io_port_t> *region = std::get<2>(*cpu->io_out.begin()).get();
			if (region->read_handler) {
				value = region->read_handler(addr, sizeof(T), region->opaque);
			}
			else {
				printf("%s: unhandled PMIO read at address $%02hx with size %d\n", __func__, addr, sizeof(T));
			}
			return value;
		}
		break;

		case MEM_UNMAPPED: {
			// XXX handle this properly instead of just aborting
			fprintf(stderr, "%s: memory access to unmapped memory at address $%02hx with size %d\n", __func__, addr, sizeof(T));
			exit(1);
		}
		break;

		default:
			assert(0 && "arch_x86_io_read: unknown region type\n");
			return 0;
		}
	}
	else {
		// XXX handle this properly instead of just aborting
		fprintf(stderr, "%s: io access at address $%02hx with size %d is not completely inside a memory region\n", __func__, addr, sizeof(T));
		exit(1);
	}
}

template<typename T>
void arch_x86_io_write(cpu_t *cpu, io_port_t addr, T value)
{
	io_port_t end;

	end = addr + sizeof(T) - 1;
	cpu->io_space_tree->search(addr, end, cpu->io_out);

	if ((addr >= std::get<0>(*cpu->io_out.begin())) && (end <= std::get<1>(*cpu->io_out.begin()))) {
		switch (std::get<2>(*cpu->io_out.begin())->type)
		{
		case MEM_PMIO: {
			memory_region_t<io_port_t> *region = std::get<2>(*cpu->io_out.begin()).get();
			if (region->write_handler) {
				region->write_handler(addr, sizeof(T), value, region->opaque);
			}
			else {
				printf("%s: unhandled PMIO write at address $%02hx with size %d\n", __func__, addr, sizeof(T));
			}
		}
		break;

		case MEM_UNMAPPED: {
			// XXX handle this properly instead of just aborting
			fprintf(stderr, "%s: memory access to unmapped memory at address $%02hx with size %d\n", __func__, addr, sizeof(T));
			exit(1);
		}
		break;

		default:
			assert(0 && "arch_x86_io_write: unknown region type\n");
			return;
		}
	}
	else {
		// XXX handle this properly instead of just aborting
		fprintf(stderr, "%s: io access at address $%02hx with size %d is not completely inside a memory region\n", __func__, addr, sizeof(T));
		exit(1);
	}
}
