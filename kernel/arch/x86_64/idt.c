#include <idt.h>
#include <gdt.h>

extern void isr_stubs();
static idt_entry_t idt_entries[256];

void idt_setup()
{
	virtaddr_t isr_start = (virtaddr_t)&isr_stubs;

	for(int i = 0; i < 256; i++)
	{
		virtaddr_t addr = isr_start + (i * 16);

		idt_entries[i] = (idt_entry_t) 
		{
			.base16_low = addr & 0xFFFF,
			.segment = GDT_KERNEL_CS,
			.ist = 0,
			.flags = IDT_PRESENT | IDT_INTR_GATE | (i == 0x80 ? IDT_RING3 : IDT_RING0),
			.base16_high = (addr >> 16) & 0xFFFF,
			.base32_high = addr >> 32,
			.reserved = 0
		};
	}
}

void idt_load()
{
	idtr_t idtr;
	idtr.limit = 256 * sizeof(idt_entry_t) - 1;
	idtr.base = (virtaddr_t)&idt_entries;

	asm volatile("lidt %0" :: "m"(idtr) : "memory");
}

