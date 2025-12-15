#pragma once

#include <lib/types.hpp>

enum MSRRegisters
{
	LAPIC_BASE = 0x1b,
	IA32_PAT_MSR = 0x277,
	FS_BASE = 0xc0000100,
	GS_BASE = 0xc0000101,
	KERNEL_GS_BASE = 0xc0000102
};

enum Selector
{
	KERNEL_CS = (1 << 3),
	KERNEL_DS = (2 << 3),
	USER_CS = (3 << 3),
	USER_DS = (4 << 3)
};

enum GDTFlags
{
	GDT_ACCESSED = 0b1,
	GDT_WRITABLE = 0b10,
	GDT_READABLE = 0b10,
	GDT_EXECUTABLE = 0b1000,
	GDT_CS = 0b10000,
	GDT_DS = 0b10000,
	GDT_PRESENT = 0b10000000
};

enum DPL
{
	DPL_KERNEL = 0x0,
	DPL_USER = 0x3
};

struct __attribute__((packed)) GDTEntry
{
	GDTEntry()
	{
		limit_low = 0;
		base_low = 0;
		base_mid = 0;
		flags = 0;
		granularity = 0;
		base_high = 0;
	}

	GDTEntry(uint8_t flags_sel, uint8_t priv)
	{
		limit_low = 0;
		base_low = 0;
		base_mid = 0;
		flags = flags_sel | (priv << 6) | GDT_PRESENT | GDT_ACCESSED;
		granularity = 0b10100000;
		base_high = 0;
	}

	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t flags;
	uint8_t granularity;
	uint8_t base_high;
};
static_assert(sizeof(GDTEntry) == 8);

struct __attribute__((packed)) GDTDescriptor
{
	uint16_t limit;
	GDTEntry* base;
};
static_assert(sizeof(GDTDescriptor) == 10);

enum IDTType
{
	IDT_INTR_GATE = 0x8e,
	IDT_TRAP_GATE = 0x8f
};

struct __attribute__((packed)) IDTEntry
{
	IDTEntry() = default;

	IDTEntry(uint64_t base, uint16_t sel, uint8_t gate_type, uint8_t priv)
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
static_assert(sizeof(IDTEntry) == 16);

struct __attribute__((packed)) IDTDescriptor
{
	uint16_t limit;
	IDTEntry* base;
};
static_assert(sizeof(IDTDescriptor) == 10);

extern "C" void reload_segments();
extern "C" void isr_stubs();

struct PageTable;

class CPU
{
public:
	CPU() = default;

	void early_init(uint32_t cpu);
	
	void set_pagetable(PageTable* pt_address);
	constexpr PageTable* get_pagetable()
	{
		return page_table;
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

	alignas(16) GDTEntry gdt_entries[8];

	GDTDescriptor gdtr;
	IDTDescriptor idtr;

	PageTable* page_table;
};
