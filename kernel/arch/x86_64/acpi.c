#include <arch/x86_64/acpi.h>
#include <mm/vmm.h>
#include <libk/string.h>
#include <klog.h>
#include <types.h>
#include <panic.h>

static const sdt_header* find_table(const sdt_header* root_table, const char* id)
{
	int num_entries = (root_table->length - sizeof(sdt_header)) / 8;
	byte* hdr = (byte*)(root_table) + sizeof(sdt_header);

	for(int i = 0; i < num_entries; i++)
	{
		const physaddr_t* address = (const physaddr_t*)(hdr + (i * sizeof(physaddr_t)));
		const sdt_header* header = (const sdt_header*)(*address + VM_DMAP_BASE);
		if(!strncmp(header->signature, id, 4))
			return header;
	}

	return nullptr;
}

static acpi_tables g_tables;

void acpi_parse_rsdp(const rsdp_v1* rsdp)
{
	if(rsdp->revision < 2)
		panic("RSDP version invalid");

	g_tables.xsdp = (const rsdp_v2*)(rsdp);
	klog("acpi: RSDP %p %p\n", g_tables.xsdp, g_tables.xsdp->length);
	if(!g_tables.xsdp->xsdt_address)
		panic("XSDT pointer invalid");

	g_tables.xsdt = (const sdt_header*)(g_tables.xsdp->xsdt_address + VM_DMAP_BASE);
	klog("acpi: XSDT %p %p\n", g_tables.xsdt, g_tables.xsdt->length);

	g_tables.fadt = (const fadt*)(find_table(g_tables.xsdt, "FACP"));
	if(!g_tables.fadt)
		panic("failed to locate FADT");
	klog("acpi: FACP %p %p\n", g_tables.fadt, g_tables.fadt->header.length);

	g_tables.madt = (const madt*)(find_table(g_tables.xsdt, "APIC"));
	if(!g_tables.madt)
		panic("failed to locate MADT");
	klog("acpi: APIC %p %p\n", g_tables.madt, g_tables.madt->header.length);

	g_tables.mcfg = (const mcfg*)(find_table(g_tables.xsdt, "MCFG"));
	if(!g_tables.mcfg)
		panic("failed to locate MCFG");	
	klog("acpi: MCFG %p %p\n", g_tables.mcfg, g_tables.mcfg->header.length);

	g_tables.hpet = (const hpet*)(find_table(g_tables.xsdt, "HPET"));
	if(g_tables.hpet)
		klog("acpi: HPET %p %p\n", g_tables.hpet, g_tables.hpet->header.length);
}

acpi_tables* acpi_get_tables()
{
	return &g_tables;
}
