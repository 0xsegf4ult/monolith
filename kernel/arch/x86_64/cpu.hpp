#pragma once

#include <lib/types.hpp>
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
struct thread_t;

struct cpu_t
{
	void set_pagetable(page_table* pt_address);
	constexpr page_table* get_pagetable()
	{
		return pt;
	}

	void set_current_thread(thread_t* thr);
	constexpr thread_t* get_current_thread()
	{
		return current_thread;
	}

	uint32_t id;
	uint32_t lapic_id;
	
	page_table* pt{nullptr};
	thread_t* current_thread{nullptr};
	
	alignas(16) gdt_entry_t gdt_entries[8]; 
	tss_t tss;

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

extern "C" uint64_t disable_interrupts();
extern "C" void restore_flags(uint64_t rflags);

inline void enable_interrupts()
{
	asm volatile("sti");
}

extern "C" void cpu_switch_thread(thread_t* prev, thread_t* next);
