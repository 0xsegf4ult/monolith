#include <arch/x86_64/lapic.h>
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/irq.h>
#include <mm/vmm.h>

#include <libk/list.h>

#include <sys/clock.h>
#include <sys/timer.h>

#include <config.h>
#include <klog.h>
#include <types.h>
#include <panic.h>

static virtaddr_t base_address = 0;
static uint8_t nmi_lint = 1;
static uint8_t nmi_cpu = 0xFF;
static uint64_t timer_interval = 0;

static void lapic_get_base()
{
	const madt* tbl = acpi_get_tables()->madt;

	const byte* raw = (const byte*)tbl;
	raw += sizeof(madt);

	size_t len = tbl->header.length - sizeof(madt);
	while(len)
	{
		const madt_entry* entry = (const madt_entry*)raw;

		if(entry->type == MADT_LAPIC_NMI)
		{
			const madt_lapic_nmi_entry* data = (const madt_lapic_nmi_entry*)entry;
			klog("acpi: LAPIC NMI lapic[%x] lint%d (flags: %d)\n", data->acpi_cpuid, data->lint, data->flags);
			if(data->acpi_cpuid != 0xFF)
				klog("acpi: percpu LAPIC NMI unsupported!\n");

			nmi_lint = data->lint;
			nmi_cpu = data->acpi_cpuid;
		}
		else if(entry->type == MADT_LAPIC_AO)
		{
			const madt_lapic_override_entry* data = (const madt_lapic_override_entry*)entry;
			klog("acpi: LAPIC address override %p\n", data->lapic_address64);
			base_address = data->lapic_address64;
		}

		raw += entry->length;
		len -= entry->length;
	}

	if(!base_address)
		base_address = tbl->lapic_address;
	
	mmu_map(vm_get_kernel_space()->mmu_root, base_address, base_address + VM_DMAP_BASE, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);
	base_address += VM_DMAP_BASE;
}

static void lapic_write(uint32_t address, uint32_t data)
{
	*(volatile uint32_t*)(base_address + address) = data;
}

static uint32_t lapic_read(uint32_t address)
{
	return *(const volatile uint32_t*)(base_address + address);
}

static void lapic_timer_set_periodic(timer_device* timer)
{
	lapic_write(LAPIC_REG_LVT_TIMER, INTERRUPT_VECTOR_TIMER | 0x20000);
	lapic_write(LAPIC_REG_TIMER_DIVIDER, 0x3);
	lapic_write(LAPIC_REG_TIMER_INITCNT, timer_interval);
}

void lapic_eoi()
{
	lapic_write(LAPIC_REG_EOI, 0);
}

static timer_device lapic_timer =
{
	.name = "lapic_timer",
	.priority = 200,
	.shift = 32,
	.set_periodic = lapic_timer_set_periodic,
	.eoi = lapic_eoi,
	.list_node = {&lapic_timer.list_node, &lapic_timer.list_node}
};

static void lapic_timer_calibrate()
{
	lapic_write(LAPIC_REG_TIMER_DIVIDER, 0x3);

	lapic_write(LAPIC_REG_TIMER_INITCNT, 0xFFFFFFFF);
	local_irq_enable();
	
	clock_wait(1000000);

	local_irq_disable();
	lapic_write(LAPIC_REG_LVT_TIMER, 1 << 16);

	auto ticks = 0xFFFFFFFF - lapic_read(LAPIC_REG_TIMER_CURRCNT);
	timer_interval = ticks;
	klog("lapic_timer: %p ticks period\n", ticks); 
	lapic_timer.mult = ((uint64_t)(ticks) << 32) / 10000000;
}

void lapic_init()
{
	lapic_get_base();
	lapic_enable();
	
	lapic_timer_calibrate();	
	timer_device_register(&lapic_timer);
}

void lapic_enable()
{
	wrmsr(MSR_LAPIC_BASE, (base_address - VM_DMAP_BASE) | APIC_BASE_MSR_ENABLE);
	lapic_write(LAPIC_REG_SPURIOUS_INT, LAPIC_SPURIOUS_ENABLE | LAPIC_SPURIOUS_INT);
}	

void lapic_send_ipi(uint32_t id, uint32_t ipi)
{
	lapic_write(LAPIC_REG_ICR + 0x10, id << 24);
	lapic_write(LAPIC_REG_ICR, ipi);

	while(lapic_read(LAPIC_REG_ICR) & LAPIC_ICR_STATUS)
	{
		native_cpu_relax();
	}
}
