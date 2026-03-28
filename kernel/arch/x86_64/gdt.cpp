#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/cpu.hpp>

extern "C" void reload_segments();

gdt_entry_t create_gdt_entry(uint8_t seg, uint8_t flags, uint8_t priv)
{
	gdt_entry_t entry = 0;
	const uint8_t granularity = 0b10100000;
	const uint8_t flags_sel = flags | seg | (priv << 5) | GDT_PRESENT | GDT_ACCESSED;
	
	entry |= (uint64_t(flags_sel) << 40);
	entry |= (uint64_t(granularity) << 48);
	return entry;
}

gdt_entry_t create_gdt_entry_tss0(uint8_t flags, uint8_t priv, virtaddr_t base, size_t limit)
{
	gdt_entry_t entry = limit & 0xFFFF;
	entry |= (base & 0xFFFF) << 16;
	entry |= ((base >> 16) & 0xFF) << 32;
	
	const uint8_t flags_sel = flags | (priv << 5) | GDT_PRESENT;
	entry |= (uint64_t(flags_sel) << 40);
	entry |= ((limit >> 16) & 0xF) << 48;
	entry |= 1ull << 52;
	entry |= ((base >> 24) & 0xFF) << 56;

	return entry;
}

gdt_entry_t create_gdt_entry_tss1(virtaddr_t base)
{
	return (base >> 32) & 0xFFFFFFFF;
}

void setup_gdt(cpu_t* cpu)
{
	virtaddr_t tss_base = reinterpret_cast<virtaddr_t>(&cpu->tss);

	cpu->gdt_entries[0] = 0;
	cpu->gdt_entries[1] = create_gdt_entry(GDT_CS, GDT_EXECUTABLE | GDT_READABLE, DPL_KERNEL);
	cpu->gdt_entries[2] = create_gdt_entry(GDT_DS, GDT_WRITABLE, DPL_KERNEL);
	cpu->gdt_entries[3] = create_gdt_entry(GDT_CS, GDT_EXECUTABLE | GDT_READABLE, DPL_USER);
	cpu->gdt_entries[4] = create_gdt_entry(GDT_DS, GDT_WRITABLE, DPL_USER);
	cpu->gdt_entries[5] = create_gdt_entry_tss0(GDT_TSS, DPL_USER, tss_base, sizeof(tss_t) - 1);
	cpu->gdt_entries[6] = create_gdt_entry_tss1(tss_base);

	gdtr_t gdtr;
	gdtr.limit = 7 * sizeof(gdt_entry_t) - 1;
	gdtr.base = cpu->gdt_entries;

	asm volatile("lgdt %0" :: "m"(gdtr) : "memory");
	reload_segments();

	asm volatile("movq %%rsp, %0" : "=g"(cpu->tss.rsp0));
	cpu->tss.iomap_base = sizeof(tss_t);
	asm volatile("mov %0, %%ax; ltr %%ax" : : "g"(TSS_SELECTOR));
}


