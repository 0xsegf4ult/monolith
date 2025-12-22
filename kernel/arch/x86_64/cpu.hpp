#pragma once

#include <lib/types.hpp>

enum msr_registers
{
	LAPIC_BASE = 0x1b,
	IA32_PAT_MSR = 0x277,
	FS_BASE = 0xc0000100,
	GS_BASE = 0xc0000101,
	KERNEL_GS_BASE = 0xc0000102
};

enum segment_selector
{
	KERNEL_CS = (1 << 3),
	KERNEL_DS = (2 << 3),
	USER_CS = (3 << 3),
	USER_DS = (4 << 3),
	TSS_SELECTOR = (5 << 3)
};

enum gdt_flags
{
	GDT_ACCESSED = 0b1,
	GDT_WRITABLE = 0b10,
	GDT_READABLE = 0b10,
	GDT_EXECUTABLE = 0b1000,
	GDT_TSS = 0b1001,
	GDT_CS = 0b10000,
	GDT_DS = 0b10000,
	GDT_PRESENT = 0b10000000
};

enum DPL
{
	DPL_KERNEL = 0x0,
	DPL_USER = 0x3
};

using gdt_entry_t = uint64_t;
struct __attribute__((packed)) gdtr_t
{
	uint16_t limit;
	uint64_t* base;
};
static_assert(sizeof(gdtr_t) == 10);

enum idt_type
{
	IDT_INTR_GATE = 0x8e,
	IDT_TRAP_GATE = 0x8f
};

struct __attribute__((packed)) idt_entry_t
{
	idt_entry_t() = default;

	idt_entry_t(uint64_t base, uint16_t sel, uint8_t gate_type, uint8_t priv)
	{
		base16_low = base & 0xFFFF;
		base16_high = (base >> 16) & 0xFFFF;
		base32_high = base >> 32;
		selector = sel;
		type = gate_type | (priv << 5);
		zero = 0;
		padding = 0;
	}

	uint16_t base16_low;
	uint16_t selector;
	uint8_t zero;
	uint8_t type;
	uint16_t base16_high;
	uint32_t base32_high;
	uint32_t padding;
};
static_assert(sizeof(idt_entry_t) == 16);

struct __attribute__((packed)) idtr_t
{
	uint16_t limit;
	idt_entry_t* base;
};
static_assert(sizeof(idtr_t) == 10);

struct __attribute__((packed)) tss_t
{
	uint32_t reserved0;

	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;

	uint64_t reserved1;

	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;

	uint64_t reserved2;
	uint16_t reserved3;
	
	uint16_t iomap_base;
};

extern "C" void reload_segments();
extern "C" void isr_stubs();

struct page_table;
struct process_t;

class CPU
{
public:
	CPU() = default;

	void early_init(uint32_t cpu);
	
	void set_pagetable(page_table* pt_address);
	constexpr page_table* get_pagetable()
	{
		return pt;
	}

	void set_current_process(process_t* proc);
	constexpr process_t* get_current_process()
	{
		return current_process;
	}

	static uint64_t rdmsr(uint64_t msr)
	{
		uint32_t low, high;
		asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
		return (static_cast<uint64_t>(high) << 32) | low;
	}

	static void wrmsr(uint64_t msr, uint64_t value)
	{
		const uint32_t low = value & 0xFFFFFFFF;
		const uint32_t high = value >> 32;
		asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
	}

	static uint64_t read_gs_ptr(uint64_t offset)
	{
		uint64_t value;
		asm volatile("mov %%gs:%a[off], %[val]" : [val] "=r"(value) : [off] "ir"(offset));
		return value;
	}

	static CPU* get_current()
	{
		return reinterpret_cast<CPU*>(CPU::read_gs_ptr(__builtin_offsetof(CPU, self)));
	}

	static uint32_t get_current_id()
	{
		return CPU::read_gs_ptr(__builtin_offsetof(CPU, id));
	}

	static void disable_interrupts()
	{
		asm volatile("cli");
	}

	static void enable_interrupts()
	{
		asm volatile("sti");
	}

	[[noreturn]] static void halt()
	{
		for(;;)
		{
			asm volatile("cli; hlt");
		}
	}
private:
	CPU* self;
	uint32_t id;

	alignas(16) gdt_entry_t gdt_entries[8];

	gdtr_t gdtr;
	idtr_t idtr;

	tss_t tss;

	page_table* pt{nullptr};
	process_t* current_process{nullptr};
};
