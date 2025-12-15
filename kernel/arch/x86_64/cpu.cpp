#include <arch/x86_64/cpu.hpp>
#include <mm/layout.hpp>
#include <lib/types.hpp>
#include <lib/klog.hpp>

static IDTEntry idt_entries[256];

void CPU::early_init(uint32_t cpu)
{
	disable_interrupts();
	self = this;
	id = cpu;

	gdt_entries[0] = GDTEntry();
	gdt_entries[1] = GDTEntry(GDT_CS | GDT_EXECUTABLE | GDT_READABLE, DPL_KERNEL);
	gdt_entries[2] = GDTEntry(GDT_DS | GDT_WRITABLE, DPL_KERNEL);
	gdt_entries[3] = GDTEntry(GDT_CS | GDT_EXECUTABLE | GDT_READABLE, DPL_USER);
	gdt_entries[4] = GDTEntry(GDT_DS | GDT_WRITABLE, DPL_USER);

	gdtr.limit = 5 * sizeof(GDTEntry) - 1;
	gdtr.base = gdt_entries;

	asm volatile("lgdt %0" :: "m"(gdtr) : "memory");
	reload_segments();

	wrmsr(GS_BASE, reinterpret_cast<virtaddr_t>(this));

	if(cpu == 0)
	{
		virtaddr_t isr_start = reinterpret_cast<virtaddr_t>(&isr_stubs);

		for(int i = 0; i < 32; i++)
			idt_entries[i] = IDTEntry(isr_start + (i * 16), KERNEL_CS, IDT_TRAP_GATE, DPL_KERNEL);
		for(int i = 32; i < 256; i++)
			idt_entries[i] = IDTEntry(isr_start + (i * 16), KERNEL_CS, IDT_INTR_GATE, DPL_KERNEL);
	}

	idtr.limit = 256 * sizeof(IDTEntry) - 1;
	idtr.base = idt_entries;

	asm volatile("lidt %0" : : "m"(idtr) : "memory");
	enable_interrupts();
	
	log::info("x86: PAT configured: WB WC UC- UC WB WP UC- WT");

	// WB WC UC- UC WB WP UC- WT
	wrmsr(MSRRegisters::IA32_PAT_MSR, 0x0407050600070106);
}

void CPU::set_pagetable(PageTable* pt_address)
{
	page_table = pt_address;
	asm volatile("movq %0, %%cr3" : : "a"(reinterpret_cast<uint64_t>(pt_address) - mm::direct_mapping_offset));
}
