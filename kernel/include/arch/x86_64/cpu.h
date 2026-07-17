#pragma once

#include <stdint.h>
#include <gdt.h>

typedef struct task_t task_t;
typedef struct page_table page_table;

enum MSR_REGISTERS : uint64_t
{
	MSR_LAPIC_BASE 		= 0x1b,
	MSR_IA32_PAT 		= 0x277,
	MSR_FS_BASE 		= 0xc0000100,
	MSR_GS_BASE 		= 0xc0000101,
	MSR_KERNELGS_BASE 	= 0xc0000102
};

typedef struct cpu
{
	uint32_t id;
	uint32_t lapic_id;

	page_table* cur_pgt;
	task_t* current_task;

	alignas(16) gdt_t gdt;
	tss_t tss;
} cpu_t;

static inline uint64_t rdmsr(uint64_t msr)
{
	uint32_t low, high;
	asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr) : "memory");
	return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint64_t msr, uint64_t value)
{
	const uint32_t low = value & 0xFFFFFFFF;
	const uint32_t high = value >> 32;
	asm volatile("wrmsr" :: "c"(msr), "a"(low), "d"(high) : "memory");
}

static inline void native_halt_cpu()
{
	for(;;)
		asm volatile("cli; hlt");
}
