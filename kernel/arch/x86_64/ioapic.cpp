#include <arch/x86_64/ioapic.hpp>
#include <arch/x86_64/acpi.hpp>

#include <mm/vmm.hpp>
#include <klog.hpp>
#include <types.hpp>

static virtaddr_t base_address = 0;
static uint32_t gsi_base = 0;

static uint8_t isa_irq_override[16] = {0};
static uint8_t redir_entry_count = 0;

static void ioapic_get_base()
{
	auto* madt = acpi_get_tables().madt;
	auto* raw = (const byte*)madt;
	raw += sizeof(acpi::madt);

	auto len = madt->length - sizeof(acpi::madt);
	while(len)
	{
		auto* entry = (const acpi::madt_entry*)raw;

		if(entry->entry_type == acpi::MADTEntryType::IOAPIC)
		{
			auto* data = (const acpi::madt_ioapic_entry*)entry;	
		
			if(base_address)
				log::warn("acpi: multiple IOAPIC instances not supported!");
			else
			{
				base_address = data->ioapic_address;
				gsi_base = data->gsi_base;
			}
		}
		else if(entry->entry_type == acpi::MADTEntryType::IOAPIC_ISO)
		{
			auto* data = (const acpi::madt_ioapic_iso_entry*)entry;
			log::info("acpi: interrupt override (bus {} irq {} GSI {} (flags {:b})", data->bus_source, data->irq_source, data->gsi, data->flags);
			isa_irq_override[data->irq_source] = data->gsi;
		}
		else if(entry->entry_type == acpi::MADTEntryType::IOAPIC_NMI_SOURCE)
		{
			auto* data = (const acpi::madt_ioapic_nmi_source_entry*)entry;
			log::info("acpi NMI source {} GSI {} (flags: {:b})", data->nmi_source, data->gsi, data->flags);
		}

		raw += entry->entry_length;
		len -= entry->entry_length;
	}

	mmu_map(vm_get_kernel_space()->mmu_root, base_address, base_address + VM_DMAP_BASE, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);
	base_address += VM_DMAP_BASE;
}

static uint32_t ioapic_read(uint8_t offset)
{
	*(volatile uint32_t*)(base_address) = offset;
	return *(const volatile uint32_t*)(base_address + 0x10);
}

static void ioapic_write(uint8_t offset, uint32_t value)
{
	*(volatile uint32_t*)(base_address) = offset;
	*(volatile uint32_t*)(base_address + 0x10) = value;
}

void ioapic_init()
{
	ioapic_get_base();

	uint8_t id = (ioapic_read(0x00) >> 24) * 0xf0;
	uint8_t ver = ioapic_read(0x01);
	redir_entry_count = (ioapic_read(0x01) >> 16) + 1;

	log::info("x86: IOAPIC[{}] ver {} GSI[{}:{}]", id, ver, gsi_base, gsi_base + redir_entry_count);
}

void ioapic_write_redirection_entry(uint8_t id, uint64_t entry)
{
	if(id >= redir_entry_count)
	{
		log::error("IOAPIC: redirection table index {} out of range", id);
		return;
	}

	if(isa_irq_override[id] != 0)
		id = isa_irq_override[id];

	ioapic_write(0x10 + 2 * id, uint32_t(entry));
	ioapic_write(0x10 + 2 * id + 1, uint32_t(entry >> 32));
}
