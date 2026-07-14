#include <arch/x86_64/acpi.hpp>
#include <mm/vmm.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>
#include <panic.hpp>

using namespace acpi;

static const sdt_header* find_table(const sdt_header* root_table, const char* id)
{
	int num_entries = (root_table->length - sizeof(sdt_header)) / 8;
	auto* hdr = reinterpret_cast<const byte*>(root_table) + sizeof(sdt_header);

	for(int i = 0; i < num_entries; i++)
	{
		auto* address = reinterpret_cast<const physaddr_t*>(hdr + (i * sizeof(physaddr_t)));
		auto* header = reinterpret_cast<const sdt_header*>(*address + VM_DMAP_BASE);
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

	g_tables.xsdp = reinterpret_cast<const rsdp_v2*>(rsdp);
	log::info("acpi: RSDP {:#x} {:#x}", g_tables.xsdp, g_tables.xsdp->length);
	if(!g_tables.xsdp->xsdt_address)
		panic("XSDT pointer invalid");

	g_tables.xsdt = reinterpret_cast<const sdt_header*>(g_tables.xsdp->xsdt_address + VM_DMAP_BASE);
	log::info("acpi: XSDT {:#x} {:#x}", g_tables.xsdt, g_tables.xsdt->length);

	g_tables.fadt = reinterpret_cast<const fadt*>(find_table(g_tables.xsdt, "FACP"));
	if(!g_tables.fadt)
		panic("failed to locate FADT");
	log::info("acpi: FACP {:#x} {:#x}", g_tables.fadt, g_tables.fadt->length);

	g_tables.madt = reinterpret_cast<const madt*>(find_table(g_tables.xsdt, "APIC"));
	if(!g_tables.madt)
		panic("failed to locate MADT");
	log::info("acpi: APIC {:#x} {:#x}", g_tables.madt, g_tables.madt->length);

	g_tables.mcfg = reinterpret_cast<const mcfg*>(find_table(g_tables.xsdt, "MCFG"));
	if(!g_tables.mcfg)
		panic("failed to locate MCFG");	
	log::info("acpi: MCFG {:#x} {:#x}", g_tables.mcfg, g_tables.mcfg->length);

	g_tables.hpet = reinterpret_cast<const hpet*>(find_table(g_tables.xsdt, "HPET"));
	if(g_tables.hpet)
		log::info("acpi: HPET {:#x} {:#x}", g_tables.hpet, g_tables.hpet->length);
}

acpi_tables& acpi_get_tables()
{
	return g_tables;
}
