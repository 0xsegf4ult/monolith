#include <gdt.h>
#include <cpu.h>
#include <types.h>

static gdt_segment_t gdt_segment_new(uint8_t access, uint8_t flags)
{
	gdt_segment_t seg;

	seg.limit_low = 0;
	seg.base_low = 0;
	seg.base_mid = 0;	
	seg.access = (access | GDT_PRESENT | GDT_ACCESSED);
	seg.flags_limithigh = (flags << 4);
	seg.base_high = 0;

	return seg;
}

static gdt_system_segment_t gdt_system_segment_new(uint8_t access, uint8_t flags, virtaddr_t base, size_t limit)
{
	gdt_system_segment_t seg;

	seg.limit_low = limit & 0xFFFF;
	seg.base_low = base & 0xFFFF;
	seg.base_midL = (base >> 16) & 0xFF;
	seg.access = (access | GDT_PRESENT);
	seg.flags_limithigh = ((flags << 4) & 0x0F) | ((limit >> 16) & 0xF);
	seg.base_midU = (base >> 24) & 0xFF;
	seg.base_high = (base >> 32) & 0xFFFFFFFF;
	seg.reserved = 0;

	return seg;
}

extern void gdt_load(gdtr_t* gdtr);

void gdt_init(cpu_t* cpu)
{
	virtaddr_t tss_base = (virtaddr_t)&cpu->tss;

	cpu->gdt.null = gdt_segment_new(0, 0);
	cpu->gdt.kernel_cs = gdt_segment_new(GDT_PRIV_RING0 | GDT_CODE | GDT_EXEC | GDT_READ, GDT_FLAG_LONG_MODE | GDT_FLAG_4K);
	cpu->gdt.kernel_ds = gdt_segment_new(GDT_PRIV_RING0 | GDT_DATA | GDT_WRITE, GDT_FLAG_LONG_MODE | GDT_FLAG_4K);
	cpu->gdt.user_cs = gdt_segment_new(GDT_PRIV_RING3 | GDT_CODE | GDT_EXEC | GDT_READ, GDT_FLAG_LONG_MODE | GDT_FLAG_4K);
	cpu->gdt.user_ds = gdt_segment_new(GDT_PRIV_RING3 | GDT_DATA | GDT_WRITE, GDT_FLAG_LONG_MODE | GDT_FLAG_4K);
	cpu->gdt.tss = gdt_system_segment_new(GDT_PRIV_RING0 | GDT_SYSTEM | GDT_TSS64_AVL, 0, tss_base, sizeof(tss_t) - 1);

	gdtr_t gdtr;
	gdtr.limit = sizeof(gdt_t) - 1;
	gdtr.base = (virtaddr_t)&cpu->gdt;	

	gdt_load(&gdtr);

	asm volatile("movq %%rsp, %0" : "=g"(cpu->tss.rsp0) :: "memory");
	cpu->tss.iomap_base = sizeof(tss_t);
	asm volatile("mov %0, %%ax; ltr %%ax" :: "g"(GDT_TSS) : "memory");
}
