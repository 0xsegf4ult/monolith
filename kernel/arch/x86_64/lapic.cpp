#include <arch/x86_64/lapic.hpp>
#include <arch/x86_64/acpi.hpp>
#include <arch/x86_64/cpu.hpp>
#include <mm/vmm.hpp>
#include <sys/timer.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>
#include <panic.hpp>

#include <arch/generic.hpp>
#include <arch/x86_64/interrupts.hpp>
#include <arch/x86_64/pit.hpp>

static virtaddr_t base_address = 0;
static uint8_t nmi_lint = 1;
static uint8_t nmi_cpu = 0xFF;
static uint64_t timer_interval = 0;
static uint64_t timer_irq = 0;

static void lapic_get_base()
{
	auto* madt = acpi_get_tables().madt;

	auto* raw = (const byte*)madt;
	raw += sizeof(acpi::madt);

	auto len = madt->length - sizeof(acpi::madt);
	while(len)
	{
		auto* entry = (const acpi::madt_entry*)raw;

		if(entry->entry_type == acpi::MADTEntryType::LAPIC_NMI)
		{
			auto* data = (const acpi::madt_lapic_nmi_entry*)entry;
			log::info("acpi: LAPIC NMI lapic[{:#x}] lint{} (flags: {:b})", data->acpi_cpuid, data->lint, data->flags);
			if(data->acpi_cpuid != 0xFF)
				log::warn("acpi: percpu LAPIC NMI unsupported!");

			nmi_lint = data->lint;
			nmi_cpu = data->acpi_cpuid;
		}
		else if(entry->entry_type == acpi::MADTEntryType::LAPIC_AO)
		{
			auto* data = (const acpi::madt_lapic_override_entry*)entry;
			log::info("acpi: LAPIC address override {:#x}", data->lapic_address64);
			base_address = data->lapic_address64;
		}

		raw += entry->entry_length;
		len -= entry->entry_length;
	}

	if(!base_address)
		base_address = madt->lapic_address;
	
	mmu_map(vm_get_kernel_space()->mmu_root, base_address, base_address + VM_DMAP_BASE, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);
	base_address += VM_DMAP_BASE;
}

static void lapic_write(uint32_t address, uint32_t data)
{
	*reinterpret_cast<volatile uint32_t*>(base_address + address) = data;
}

static uint32_t lapic_read(uint32_t address)
{
	uint32_t data = *reinterpret_cast<volatile uint32_t*>(base_address + address);
	return data;
}


static void lapic_timer_calibrate()
{
	lapic_write(LAPIC_REG_TIMER_DIVIDER, 0xb);

	lapic_write(LAPIC_REG_TIMER_INITCNT, 0xFFFFFFFF);
	arch_enable_interrupts();
	pit_sleep(10);
	arch_disable_interrupts();
	lapic_write(LAPIC_REG_LVT_TIMER, 1 << 16);

	auto ticks = 0xFFFFFFFF - lapic_read(LAPIC_REG_TIMER_CURRCNT);
	timer_interval = ticks;
}

void lapic_init()
{
	lapic_get_base();
	lapic_enable();
	lapic_timer_calibrate();
	lapic_timer_enable();
}

void lapic_enable()
{
	wrmsr(LAPIC_BASE, (base_address - VM_DMAP_BASE) | APIC_BASE_MSR_ENABLE);
	lapic_write(LAPIC_REG_SPURIOUS_INT, LAPIC_SPURIOUS_ENABLE | LAPIC_SPURIOUS_INT);
}	

void lapic_eoi()
{
	lapic_write(LAPIC_REG_EOI, 0);
}

void lapic_send_ipi(uint32_t id, uint32_t ipi)
{
	lapic_write(LAPIC_REG_ICR + 0x10, id << 24);
	lapic_write(LAPIC_REG_ICR, ipi);

	while(lapic_read(LAPIC_REG_ICR) & LAPIC_ICR_STATUS)
	{
		arch_pause();
	}
}

static void lapic_timer_irq()
{
	timer_interrupt();
}

void lapic_timer_enable()
{
	if(!timer_irq)
	{
		timer_irq = allocate_irq();
		install_irq_handler(timer_irq, lapic_timer_irq);
	}

	lapic_write(LAPIC_REG_LVT_TIMER, timer_irq | 0x20000);
	lapic_write(LAPIC_REG_TIMER_DIVIDER, 0xb);
	lapic_write(LAPIC_REG_TIMER_INITCNT, timer_interval);
}
