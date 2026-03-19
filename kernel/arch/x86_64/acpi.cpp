#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/apic.hpp>
#include <dev/pcie.hpp>
#include <mm/layout.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

namespace acpi
{

const sdt_header* find_table(const sdt_header* root_table, const char* id)
{
	int num_entries = (root_table->length - sizeof(sdt_header)) / 8;
	auto* hdr = reinterpret_cast<const byte*>(root_table) + sizeof(sdt_header);

	for(int i = 0; i < num_entries; i++)
	{
		auto* address = reinterpret_cast<const physaddr_t*>(hdr + (i * sizeof(physaddr_t)));
		auto* header = reinterpret_cast<const sdt_header*>(*address + mm::direct_mapping_offset);
		if(!strncmp(header->signature, id, 4))
			return header;
	}

	return nullptr;
}

void parse_madt(const madt* table)
{
	auto* raw = reinterpret_cast<const byte*>(table);
	raw += sizeof(madt);

	auto len = table->length - sizeof(madt);
	while(len)
	{
		auto* entry = reinterpret_cast<const madt_entry*>(raw);

		switch(entry->entry_type)
		{
		case MADTEntryType::LAPIC:
		{
			break;
		}
		case MADTEntryType::IOAPIC:
		{
			auto* data = reinterpret_cast<const madt_ioapic_entry*>(entry);
			ioapic::get(data->ioapic_id).enable(data->ioapic_address, data->gsi_base);
			break;
		}
		case MADTEntryType::IOAPIC_ISO:
		{
			auto* data = reinterpret_cast<const madt_ioapic_iso_entry*>(entry);
			log::info("acpi: interrupt override (bus {} irq {} GSI {} (flags {:b})", data->bus_source, data->irq_source, data->gsi, data->flags);
			break;
		}
		case MADTEntryType::IOAPIC_NMI_SOURCE:
		{
			auto* data = reinterpret_cast<const madt_ioapic_nmi_source_entry*>(entry);
			log::info("acpi: NMI source {} GSI {} (flags: {:b})", data->nmi_source, data->gsi, data->flags);
			break;
		}
		case MADTEntryType::LAPIC_NMI:
		{
			auto* data = reinterpret_cast<const madt_lapic_nmi_entry*>(entry);
			log::info("acpi: LAPIC NMI lapic[{:x}] lint{} (flags: {:b})", data->acpi_cpuid, data->lint, data->flags);
			break;
		}
 		case MADTEntryType::LAPIC_AO:
		{
			auto* data = reinterpret_cast<const madt_lapic_override_entry*>(entry);
			log::info("LAPIC address override {:x}", data->lapic_address64);
			break;
		}
		case MADTEntryType::LOCAL_X2APIC:
		{
			auto* data = reinterpret_cast<const madt_x2apic_entry*>(entry);
			log::info("x2APIC id {:x} CPU{} (flags {:x})", data->x2apic_id, data->acpi_cpuid, data->flags);
			break;
		}
		default:
			log::error("parse_madt: invalid entry type");
		}

		raw += entry->entry_length;
		len -= entry->entry_length;
	}
}

void parse_mcfg(const mcfg* table)
{
	auto* raw = reinterpret_cast<const byte*>(table);
	raw += sizeof(mcfg);

	auto len = table->length - sizeof(mcfg);
	auto ecam_count = len / sizeof(mcfg_ecam);
	
	if(ecam_count != 1)
		log::warn("acpi: MCFG has {} ECAM entries", ecam_count);

	auto* ecam = reinterpret_cast<const mcfg_ecam*>(raw);
	log::info("acpi: ECAM base {:x} segment {} bus {}-{}", ecam->base_address, ecam->segment_group, ecam->start_bus, ecam->end_bus);
	pcie::set_base(ecam->base_address);
}

acpi_tables parse_tables(const acpi::rsdp_v1* rsdp)
{
	acpi_tables tables;

	if(rsdp->revision < 2)
		panic("RSDP version invalid");

	tables.xsdp = reinterpret_cast<const acpi::rsdp_v2*>(rsdp);
	log::info("acpi: RSDP {:x} {:x}", tables.xsdp, tables.xsdp->length);
	if(!tables.xsdp->xsdt_address)
		panic("XSDT pointer invalid");

	tables.xsdt = reinterpret_cast<const acpi::sdt_header*>(tables.xsdp->xsdt_address + mm::direct_mapping_offset);
	log::info("acpi: XSDT {:x} {:x}", tables.xsdt, tables.xsdt->length);

	tables.fadt = reinterpret_cast<const acpi::fadt*>(acpi::find_table(tables.xsdt, "FACP"));
	if(!tables.fadt)
		panic("failed to locate FADT");
	log::info("acpi: FACP {:x} {:x}", tables.fadt, tables.fadt->length);

	tables.madt = reinterpret_cast<const acpi::madt*>(acpi::find_table(tables.xsdt, "APIC"));
	if(!tables.madt)
		panic("failed to locate MADT");
	log::info("acpi: APIC {:x} {:x}", tables.madt, tables.madt->length);

	tables.mcfg = reinterpret_cast<const acpi::mcfg*>(acpi::find_table(tables.xsdt, "MCFG"));
	if(!tables.mcfg)
		panic("failed to locate MCFG");	
	log::info("acpi: MCFG {:x} {:x}", tables.mcfg, tables.mcfg->length);

	parse_madt(tables.madt);
	parse_mcfg(tables.mcfg);

	return tables;
}

}
