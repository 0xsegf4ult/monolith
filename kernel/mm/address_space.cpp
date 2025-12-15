#include <mm/address_space.hpp>
#include <mm/vm_object.hpp>
#include <mm/pmm.hpp>
#include <mm/layout.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/mmu.hpp>

#include <lib/types.hpp>

void address_space::init(virtaddr_t base)
{
	base_addr = base;

	root_pml4 = reinterpret_cast<page_table*>(pmm_allocate() + mm::direct_mapping_offset);
	for(int i = 0; i < 512; i++)
		root_pml4[i] = page_table(0);
}

void address_space::switch_to()
{
	CPU::get_current()->set_pagetable(root_pml4);
}

virtaddr_t address_space::alloc(size_t length, vm_flags flags, void* arg)
{

}

void address_space::free(virtaddr_t addr)
{

}

void address_space::map(physaddr_t phys, virtaddr_t virt, vm_flags flags)
{
	mmu_map(root_pml4, phys, virt, vm_flags_to_x86(flags));
}

void address_space::map_range(physaddr_t phys, virtaddr_t virt, size_t length, vm_flags flags)
{
	mmu_map_range(root_pml4, phys, virt, length, vm_flags_to_x86(flags));
}

void address_space::reserve_range(virtaddr_t virt, size_t length, vm_flags flags)
{

}
