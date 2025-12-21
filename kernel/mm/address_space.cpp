#include <mm/address_space.hpp>
#include <mm/vm_object.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
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

	objects = nullptr;
}

void address_space::switch_to()
{
	CPU::get_current()->set_pagetable(root_pml4);
}

virtaddr_t address_space::alloc(size_t length, uint64_t flags, void* arg)
{
	return 0;
}

virtaddr_t address_space::alloc_placed(virtaddr_t base, size_t length, uint64_t flags, void* arg)
{
	if(base < 0x1000)
		return 0;
	
	vm_object* prev = nullptr;
	vm_object* cur = objects; 
	while(cur)
	{
		if(cur->base <= base)
		{
			if(cur->base + cur->length >= base)
				return 0;
		}
		else
		{
			if(cur->base < base + length)
				return 0;
		}

		prev = cur;
		cur = cur->next;
	}

	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
	range->base = base;
	range->length = length;
	range->flags = vm_flags(flags);
	range->next = nullptr;

	if(!prev)
		objects = range;
	else
		prev->next = range;

	while(length)
	{
		auto phys = pmm_allocate();
		mmu_map(root_pml4, phys, base, vm_flags_to_x86(flags));		
		base += 4096;

		if(length < 4096)
			length = 0;
		else
			length -= 4096;
	}

	return base;
}

physaddr_t address_space::get_mapping(virtaddr_t virt)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);
	auto pt_index = get_pagetable_index(virt, 1);

	page_table* pdpt = get_pte(root_pml4, pml4_index);
	if(!pdpt)
		return 0;
	page_table* pd = get_pte(pdpt, pdpt_index);
	if(!pd)
		return 0;
	page_table* pt = get_pte(pd, pd_index);
	if(!pt)
		return 0;

	return pt[pt_index].get();
}

void address_space::free(virtaddr_t addr)
{

}

void address_space::map(physaddr_t phys, virtaddr_t virt, uint64_t flags)
{
	mmu_map(root_pml4, phys, virt, vm_flags_to_x86(flags));
/*	
	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
       	range->base = virt;
	range->length = 0x1000;
	range->flags = vm_flags(flags);
	range->next = nullptr;
*/
	
}

void address_space::map_range(physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags)
{
	mmu_map_range(root_pml4, phys, virt, length, vm_flags_to_x86(flags));

	/*
	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
	range->base = virt;
	range->length = length;
	range->flags = vm_flags(flags);
	range->next = nullptr;
	*/
}

void address_space::reserve_range(virtaddr_t virt, size_t length, uint64_t flags)
{
	/*
	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
	range->base = virt;
	range->length = length;
	range->flags = vm_flags(flags);
	range->next = nullptr;
	*/
}
