#pragma once

#include <types.hpp>
#include <arch/x86_64/gdt.hpp> 

enum msr_registers
{
	LAPIC_BASE = 0x1b,
	IA32_PAT_MSR = 0x277,
	FS_BASE = 0xc0000100,
	GS_BASE = 0xc0000101,
	KERNEL_GS_BASE = 0xc0000102
};

struct page_table;
struct task_t;

struct cpu_t
{
	uint32_t id;
	uint32_t lapic_id;
	
	page_table* pt{nullptr};
	task_t* current_task{nullptr};
	
	tss_t tss;
	alignas(16) gdt_entry_t gdt_entries[8]; 
};

inline uint64_t rdmsr(uint64_t msr)
{
	uint32_t low, high;
	asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (static_cast<uint64_t>(high) << 32) | low;
}

inline void wrmsr(uint64_t msr, uint64_t value)
{
	const uint32_t low = value & 0xFFFFFFFF;
	const uint32_t high = value >> 32;
	asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}
