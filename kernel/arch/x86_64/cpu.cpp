#include <arch/x86_64/cpu.hpp>
#include <mm/layout.hpp>
#include <lib/types.hpp>
#include <lib/klog.hpp>
#include <sys/process.hpp>

static idt_entry_t idt_entries[256];

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

void CPU::early_init(uint32_t cpu)
{
	disable_interrupts();
	self = this;
	id = cpu;

	virtaddr_t tss_base = reinterpret_cast<virtaddr_t>(&tss);

	gdt_entries[0] = 0;
	gdt_entries[1] = create_gdt_entry(GDT_CS, GDT_EXECUTABLE | GDT_READABLE, DPL_KERNEL);
	gdt_entries[2] = create_gdt_entry(GDT_DS, GDT_WRITABLE, DPL_KERNEL);
	gdt_entries[3] = create_gdt_entry(GDT_CS, GDT_EXECUTABLE | GDT_READABLE, DPL_USER);
	gdt_entries[4] = create_gdt_entry(GDT_DS, GDT_WRITABLE, DPL_USER);
	gdt_entries[5] = create_gdt_entry_tss0(GDT_TSS, DPL_USER, tss_base, sizeof(tss_t) - 1);
	gdt_entries[6] = create_gdt_entry_tss1(tss_base);

	gdtr.limit = 7 * sizeof(gdt_entry_t) - 1;
	gdtr.base = gdt_entries;

	asm volatile("lgdt %0" :: "m"(gdtr) : "memory");
	reload_segments();

	asm volatile("movq %%rsp, %0" : "=g" (tss.rsp0));
	tss.iomap_base = sizeof(tss_t);	
	asm volatile("mov %0, %%ax; ltr %%ax" : : "g"(TSS_SELECTOR));

	wrmsr(GS_BASE, reinterpret_cast<virtaddr_t>(this));

	if(cpu == 0)
	{
		virtaddr_t isr_start = reinterpret_cast<virtaddr_t>(&isr_stubs);

		for(int i = 0; i < 32; i++)
			idt_entries[i] = idt_entry_t(isr_start + (i * 16), KERNEL_CS, IDT_TRAP_GATE, DPL_KERNEL);
		for(int i = 32; i < 256; i++)
		{
			auto dpl = DPL_KERNEL;
			// syscall
			if(i == 0x80)
				dpl = DPL_USER;
			
			idt_entries[i] = idt_entry_t(isr_start + (i * 16), KERNEL_CS, IDT_INTR_GATE, dpl);
		}
	}

	idtr.limit = 256 * sizeof(idt_entry_t) - 1;
	idtr.base = idt_entries;

	asm volatile("lidt %0" : : "m"(idtr) : "memory");

	if(cpu == 0)	
		log::info("x86: PAT configured: WB WC UC- UC WB WP UC- WT");

	// WB WC UC- UC WB WP UC- WT
	wrmsr(IA32_PAT_MSR, 0x0407050600070106);
}

void CPU::set_pagetable(page_table* pt_address)
{
	pt = pt_address;
	asm volatile("movq %0, %%cr3" : : "r"(reinterpret_cast<uint64_t>(pt_address) - mm::direct_mapping_offset));
}

void CPU::set_current_process(process_t* process)
{
	tss.rsp0 = process->rsp0;
	current_process = process;
}
