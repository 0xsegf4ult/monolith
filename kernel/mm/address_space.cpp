#include <mm/address_space.hpp>
#include <mm/vm_object.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/layout.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/mmu.hpp>

#include <lib/types.hpp>
#include <lib/klog.hpp>
#include <lib/kstd.hpp>

void address_space::init(virtaddr_t base)
{
	base_addr = base;

	root_pml4 = reinterpret_cast<page_table*>(pmm_allocate() + mm::direct_mapping_offset);
	for(int i = 0; i < 512; i++)
		root_pml4[i] = page_table(0);

	objects = nullptr;

	mutex_init(this->lock);
}

void address_space::destroy()
{
	mutex_lock(lock);

	while(objects)
	{
		auto last = objects;
		objects = objects->next;

		virtaddr_t base = last->base;
		size_t length = last->length;
		while(length)
		{
			mmu_unmap(root_pml4, base, last->flags & vm_mmio ? false : true);
			base += 0x1000;
			if(length >= 0x1000)
				length -= 0x1000;
			else
				length = 0;
		}

		kfree(last);
	}

	mmu_destroy(root_pml4);

	mutex_unlock(lock);
}

void address_space::switch_to()
{
	smp_current_cpu()->set_pagetable(root_pml4);
}

virtaddr_t address_space::alloc(size_t length, uint64_t flags, void* arg)
{
	length = mm::page_align(length);

	mutex_lock(lock);
	
	//log::debug("vmalloc {:#x} bytes", length);
	vm_object* prev = nullptr;
	vm_object* cur = objects;
	
	virtaddr_t alloc_base = 0;
	
	if(!objects)
	{
		alloc_base = base_addr;
	}

	while(cur)
	{
		auto prev_end = prev ? prev->base + prev->length : base_addr;
		if(prev_end + length <= cur->base)
		{
			alloc_base = prev_end;
			break;
		}

		if(cur->next == nullptr)
		{
			alloc_base = cur->base + cur->length;
		}

		prev = cur;
		cur = cur->next;	
	}

	if(!alloc_base)
	{
		mutex_unlock(lock);
		return 0;
	}

	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
	range->base = alloc_base;
	range->length = length;
	range->flags = vm_flags(flags);
	range->next = nullptr;

	if(!prev)
	{
		range->next = objects;
		objects = range;
	}
	else
	{
		range->next = prev->next;
		prev->next = range;		
	}

	physaddr_t phys = vm_allocate;
	if(flags & vm_mmio)
	{
		if(!arg)
			return 0;
		phys = *(physaddr_t*)arg;
	}

	while(length)
	{
		if(!(flags & vm_mmio) && (flags & vm_present))
			phys = pmm_allocate();

		mmu_map(root_pml4, phys, alloc_base, vm_flags_to_x86(flags));
		phys += 0x1000;
		alloc_base += 0x1000;	

		if(length < 0x1000)
			length = 0;
		else
			length -= 0x1000;
	}	

	mutex_unlock(lock);
	return range->base;
}

virtaddr_t address_space::alloc_placed(virtaddr_t base, size_t length, uint64_t flags, void* arg)
{
	length = mm::page_align(length);
	
	//log::debug("vmalloc_placed {:#x}", base);
	if(base < 0x1000)
		return 0;

	if(reserve_range(base, length, flags) != 0)
		return 0;

	mutex_lock(lock);	
	while(length)
	{
		physaddr_t phys = vm_allocate;
		if(flags & vm_present)
			phys = pmm_allocate();
		mmu_map(root_pml4, phys, base, vm_flags_to_x86(flags));	

		base += 0x1000;

		if(length < 0x1000)
			length = 0;
		else
			length -= 0x1000;
	}
	mutex_unlock(lock);

	return base;
}

physaddr_t address_space::get_mapping(virtaddr_t virt, uint64_t* flags)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);
	auto pt_index = get_pagetable_index(virt, 1);

	mutex_lock(lock);
	page_table* pdpt = get_pte(root_pml4, pml4_index);
	if(!pdpt)
	{
		mutex_unlock(lock);
		return 0;
	}
	page_table* pd = get_pte(pdpt, pdpt_index);
	if(!pd)
	{
		mutex_unlock(lock);
		return 0;
	}
	page_table* pt = get_pte(pd, pd_index);
	if(!pt)
	{
		mutex_unlock(lock);
		return 0;
	}

	auto entry = pt[pt_index].get_raw();
	if(flags)
	{
		if(entry & PTE_PRESENT)
			*flags |= vm_present;
		else if(entry)
			*flags |= vm_swapped;

		if(entry & PTE_WRITABLE)
			*flags |= vm_write;
		if(entry & PTE_USER)
			*flags |= vm_user;
		if(!(entry & PTE_NOEXEC))
			*flags |= vm_exec;
	}	

	mutex_unlock(lock);
	return pt[pt_index].get();
}

void address_space::free(virtaddr_t addr)
{
	//log::debug("vm_free: {:#x}", addr);
	mutex_lock(lock);
	vm_object* prev = nullptr;
	vm_object* cur = objects;
	while(cur)
	{
		if(cur->base == addr)
		{
			size_t len = cur->length;
			if(prev)
				prev->next = cur->next;
			else
				objects = cur->next;

			while(len)
			{
				mmu_unmap(root_pml4, addr, cur->flags & vm_mmio ? false : true); 

				addr += 0x1000;
				if(len >= 0x1000)
					len -= 0x1000;
				else
					len = 0;
			}			

			kfree(cur);
			mutex_unlock(lock);
			return;	
		}

		prev = cur;
		cur = cur->next;
	}

	mutex_unlock(lock);
	panic("vm_free: invalid address {:x}", addr);
}

int address_space::map(physaddr_t phys, virtaddr_t virt, uint64_t flags)
{
	//log::debug("vm_map {:#x}", virt);
	flags |= vm_present;
	if(reserve_range(virt, 0x1000, flags) == 0)
	{
		mutex_lock(lock);
		mmu_map(root_pml4, phys, virt, vm_flags_to_x86(flags));
		mutex_unlock(lock);
		return 0;
	}

	return -1;
}

int address_space::map_range(physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags)
{
	length = mm::page_align(length);
//	log::debug("vm_map_range {:#x} - {:#x}", virt, virt + length);
	
	flags |= vm_present;
	if(reserve_range(virt, length, flags) == 0)
	{
		mutex_lock(lock);
		mmu_map_range(root_pml4, phys, virt, length, vm_flags_to_x86(flags));
		mutex_unlock(lock);
		return 0;
	}

	return -1;
}

int address_space::reserve_range(virtaddr_t base, size_t length, uint64_t flags)
{
	length = mm::page_align(length);
	mutex_lock(lock);
	vm_object* prev = nullptr;
	vm_object* cur = objects;
	
	while(cur)
	{
		if(cur->base <= base)	
		{
			if(cur->base + cur->length > base)
			{
				mutex_unlock(lock);
				return -1;
			}
		}
		else
		{
			if(cur->base < base + length)
			{
				mutex_unlock(lock);
				return -1;
			}
		}

		prev = cur;
		cur = cur->next;
	}

	flags |= vm_present;
	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
	range->base = base;
	range->length = length;
	range->flags = vm_flags(flags);
	range->next = nullptr;

	if(objects == nullptr || objects->base >= base)
	{
		range->next = objects;
		objects = range;
	}
	else
	{
		vm_object* current = objects;
		while(current->next && current->next->base < base)
			current = current->next;

		range->next = current->next;
		current->next = range;
	}

	mutex_unlock(lock);
	return 0;
}

void address_space::dump_objects()
{
	vm_object* cur = objects;
	while(cur)
	{
		log::debug("vm_object [{:#x} - {:#x}] {:b}", cur->base, cur->length, cur->flags);
	       	cur = cur->next;
	}	
}
