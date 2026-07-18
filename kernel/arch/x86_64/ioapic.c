#include <arch/x86_64/ioapic.h>
#include <arch/x86_64/lapic.h>
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu.h>

#include <mm/vmm.h>

#include <sys/irq.h>
#include <sys/smp.h>

#include <klog.h>
#include <types.h>

static virtaddr_t base_address = 0;
static uint32_t gsi_base = 0;

static uint8_t isa_irq_override[16] = {0};
static uint8_t redir_entry_count = 0;

static void ioapic_get_base()
{
	const madt* tbl = acpi_get_tables()->madt;
	const byte* raw = (const byte*)tbl;
	raw += sizeof(madt);

	size_t len = tbl->header.length - sizeof(madt);
	while(len)
	{
		const madt_entry* entry = (const madt_entry*)raw;

		if(entry->type == MADT_IOAPIC)
		{
			const madt_ioapic_entry* data = (const madt_ioapic_entry*)entry;	
		
			if(base_address)
				klog("acpi: multiple IOAPIC instances not supported!\n");
			else
			{
				base_address = data->ioapic_address;
				gsi_base = data->gsi_base;
			}
		}
		else if(entry->type == MADT_IOAPIC_ISO)
		{
			const madt_ioapic_iso_entry* data = (const madt_ioapic_iso_entry*)entry;
			klog("acpi: interrupt override (bus %d irq %d GSI %d (flags {%d})\n", data->bus_source, data->irq_source, data->gsi, data->flags);
			isa_irq_override[data->irq_source] = data->gsi;
		}
		else if(entry->type == MADT_IOAPIC_NMI_SOURCE)
		{
			const madt_ioapic_nmi_source_entry* data = (const madt_ioapic_nmi_source_entry*)entry;
			klog("acpi: NMI source %d GSI %d (flags: {%d})\n", data->nmi_source, data->gsi, data->flags);
		}

		raw += entry->length;
		len -= entry->length;
	}

	mmu_map(vm_get_kernel_space()->mmu_root, base_address, base_address + VM_DMAP_BASE, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);
	base_address += VM_DMAP_BASE;
}

static uint32_t ioapic_read(uint8_t offset)
{
	*(volatile uint32_t*)(base_address + IOAPIC_ADDRESS_REGISTER) = offset;
	return *(const volatile uint32_t*)(base_address + IOAPIC_ADDRESS_DATA);
}

static void ioapic_write(uint8_t offset, uint32_t value)
{
	*(volatile uint32_t*)(base_address + IOAPIC_ADDRESS_REGISTER) = offset;
	*(volatile uint32_t*)(base_address + IOAPIC_ADDRESS_DATA) = value;
}

static void ioapic_write_redirection_entry(uint8_t id, ioapic_redir_entry_t entry)
{
	if(id >= redir_entry_count)
	{
		klog("IOAPIC: redirection table index %d out of range\n", id);
		return;
	}

	ioapic_write(IOAPIC_REGISTER_REDTBL + 2 * id, (uint32_t)entry.raw);
	ioapic_write(IOAPIC_REGISTER_REDTBL + 2 * id + 1, (uint32_t)(entry.raw >> 32));
}

static void ioapic_enable(virq_t* irq)
{
	hwirq_t dst = irq->hwirq;
	if(dst < 16 && isa_irq_override[dst] != 0)
		dst = isa_irq_override[dst];

	klog("IOAPIC: set hwirq %u ENABLED vector %u\n", dst, irq->id);

	ioapic_write_redirection_entry(dst, 
	(ioapic_redir_entry_t)
	{
		.vector = irq->id,
		.delivery_mode = IOAPIC_DELIVERY_MODE_FIXED,
		.destination_mode = IOAPIC_DESTINATION_PHYSICAL,
		.polarity = IOAPIC_POLARITY_ACTIVE_HIGH,
		.trigger_mode = IOAPIC_TRIGGER_MODE_EDGE,
		.destination = smp_get_cpu(smp_current_cpu())->lapic_id	
	});	
}

static void ioapic_disable(virq_t* irq)
{
	hwirq_t dst = irq->hwirq;
	if(dst < 16 && isa_irq_override[dst] != 0)
		dst = isa_irq_override[dst];

	ioapic_write_redirection_entry(dst,
	(ioapic_redir_entry_t)
	{
		.mask = 1
	});
}

static void ioapic_eoi(virq_t* irq)
{
	lapic_eoi();
}

static irq_chip_t ioapic_chip = 
{
	.name = "IOAPIC",
	.enable = ioapic_enable,
	.disable = ioapic_disable,
	.eoi = ioapic_eoi
};

static irq_domain_t ioapic_domain =
{
	.name = "ioapic0",
	.chip = &ioapic_chip,
};

void ioapic_init()
{
	ioapic_get_base();

	uint8_t id = (ioapic_read(IOAPIC_REGISTER_ID) >> 24) * 0xf0;
	uint8_t ver = ioapic_read(IOAPIC_REGISTER_VER);
	redir_entry_count = (ioapic_read(IOAPIC_REGISTER_VER) >> 16) + 1;
	ioapic_domain.end = redir_entry_count;

	irq_domain_register(&ioapic_domain);
	klog("x86: IOAPIC[%d] ver %d GSI[%d - %d]\n", id, ver, gsi_base, gsi_base + redir_entry_count - 1);
}

