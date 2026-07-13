#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/x86_64/cpu.hpp>
#include <mm/vmm.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>
#include <panic.hpp>

namespace lapic
{

static virtaddr_t base_address = 0;

void set_base(physaddr_t base)
{
	base_address = base + VM_DMAP_BASE;
	mmu_map(vm_get_kernel_space()->mmu_root, base, base_address, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);
}

void enable()
{
	wrmsr(LAPIC_BASE, (base_address - VM_DMAP_BASE) | 0x800);
	*reinterpret_cast<volatile uint32_t*>(base_address + 0xf0) = 0x1ff;
}

void eoi()
{
	*reinterpret_cast<volatile uint32_t*>(base_address + 0xb0) = 0;
}

void send_ipi(uint32_t id, uint32_t ipi)
{
	*reinterpret_cast<volatile uint32_t*>(base_address + 0x310) = id << 24;
	*reinterpret_cast<volatile uint32_t*>(base_address + 0x300) = ipi;

	while(read(0x300) & 0x1000)
	{
		asm volatile("pause");
	}
}

void write(uint32_t address, uint32_t data)
{
	*reinterpret_cast<volatile uint32_t*>(base_address + address) = data;
}

uint32_t read(uint32_t address)
{
	uint32_t data = *reinterpret_cast<volatile uint32_t*>(base_address + address);
	return data;
}

}

namespace ioapic
{

void ioapic_instance::enable(physaddr_t address, uint32_t gsi)
{
	base_address = address + VM_DMAP_BASE;
	gsi_base = gsi;

	mmu_map(vm_get_kernel_space()->mmu_root, address, base_address, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);

	id = (mmio_read(0x00) >> 24) * 0xf0;
	ver = mmio_read(0x01);
	redir_entry_count = (mmio_read(0x01) >> 16) + 1;

	log::info("x86: IOAPIC[{}] ver {} GSI[{}:{}]", id, ver, gsi_base, gsi_base + redir_entry_count);
}

uint64_t ioapic_instance::read_redirection_entry(uint8_t id)
{
	if(id >= redir_entry_count)
		panic("IOAPIC: redirection table index out of range");

	uint32_t lower = mmio_read(0x10 + 2 * id);
	uint32_t upper = mmio_read(0x10 + 2 * id + 1);

	return lower | (static_cast<uint64_t>(upper) << 32);
}

void ioapic_instance::write_redirection_entry(uint8_t id, uint64_t entry)
{
	if(id >= redir_entry_count)
		panic("IOAPIC: redirection table index out of range");

	mmio_write(0x10 + 2 * id, static_cast<uint32_t>(entry));
	mmio_write(0x10 + 2 * id + 1, static_cast<uint32_t>(entry >> 32));
}

constexpr uint32_t ioapic_instance::mmio_read(uint8_t offset)
{
	*reinterpret_cast<volatile uint32_t*>(base_address) = offset;
	return *reinterpret_cast<const volatile uint32_t*>(base_address + 0x10);
}

constexpr void ioapic_instance::mmio_write(uint8_t offset, uint32_t value)
{
	*reinterpret_cast<volatile uint32_t*>(base_address) = offset;
	*reinterpret_cast<volatile uint32_t*>(base_address + 0x10) = value;
}

static ioapic_instance instances[2];
constexpr static uint32_t max_ioapic = 2;

ioapic_instance& get(uint32_t id)
{
	if(id >= max_ioapic)
		panic("IOAPIC index out of range");

	return instances[id];
}

}
