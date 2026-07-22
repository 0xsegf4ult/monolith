#pragma once

#include <arch/x86_64/gdt.h>
#include <types.h>

struct task;
struct page_table;

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

	struct page_table* cur_pgt;
	struct task* current_task;

	alignas(16) gdt_t gdt;
	tss_t tss;
} cpu_t;

void cpu_set_pagetable(struct page_table* pgt);

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

static inline void native_cpu_halt()
{
	for(;;)
		asm volatile("cli; hlt");
}

static inline void native_cpu_idle()
{
	asm volatile("hlt");
}

static inline void native_cpu_relax()
{
	asm volatile("pause" ::: "memory");
}

static inline void native_memory_barrier()
{
	asm volatile("mfence" ::: "memory");
}

void native_set_tls(virtaddr_t base);
extern void native_context_switch(struct task* prev, struct task* next);
extern void native_switch_to_usermode(virtaddr_t stack, virtaddr_t entry);

struct cpu_context
{
	uint8_t simd[512];
};

struct cpu_context* cpu_context_new();
void cpu_context_destroy(struct cpu_context* ctx);
void cpu_context_save(struct cpu_context* ctx);
void cpu_context_restore(struct cpu_context* ctx);

struct interrupt_frame;
void dump_registers(struct interrupt_frame* frame);
void stacktrace(virtaddr_t frame);
