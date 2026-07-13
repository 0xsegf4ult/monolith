#include <mm/vmm.hpp>
#include <mm/vm_space.hpp>
#include <mm/memory_map.hpp>
#include <mm/slab.hpp>
#include <mm/pmm.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/x86_64/smp.hpp>

#include <fs/vfs.hpp>
#include <fs/procfs/procfs.hpp>

#include <sys/task.hpp>

#include <klog.hpp>
#include <kstd.hpp>
#include <list.hpp>
#include <types.hpp>
#include <panic.hpp>

static vm_space* kernel_address_space = nullptr;
static physaddr_t zero_cowmem;

extern "C"
{
	byte _text_start;
	byte _text_end;
	byte _rodata_start;
	byte _rodata_end;
	byte _data_start;
	byte _bss_end;
}

void vmm_init_kpages(mm::memory_map& memmap, physaddr_t kload_addr)
{	
	kernel_address_space = (vm_space*)kmalloc(sizeof(vm_space));
	list_node_init(kernel_address_space->objects);
	kernel_address_space->start_address = VM_VMALLOC_BASE;
	kernel_address_space->end_address = VM_KERNEL_BASE;

	kernel_address_space->mmu_root = mmu_new_pgdir();
	mutex_init(kernel_address_space->lock);

	kernel_address_space->mapped_anon = 0;
	kernel_address_space->mapped_file = 0;
	kernel_address_space->resident_anon = 0;
	kernel_address_space->resident_file = 0;

	auto text_start = page_align_down(reinterpret_cast<virtaddr_t>(&_text_start));
	auto text_length = page_align(reinterpret_cast<virtaddr_t>(&_text_end) - reinterpret_cast<virtaddr_t>(&_text_start));
	auto rodata_start = page_align_down(reinterpret_cast<virtaddr_t>(&_rodata_start));
	auto rodata_length = page_align(reinterpret_cast<virtaddr_t>(&_rodata_end) - reinterpret_cast<virtaddr_t>(&_rodata_start));
	auto databss_start = page_align_down(reinterpret_cast<virtaddr_t>(&_data_start));
	auto databss_length = page_align(reinterpret_cast<virtaddr_t>(&_bss_end) - reinterpret_cast<virtaddr_t>(&_data_start));

	mmu_map_range(kernel_address_space->mmu_root, text_start - VM_KERNEL_BASE + kload_addr, text_start, text_length, PROT_EXEC | PROT_READ, 0);
	mmu_map_range(kernel_address_space->mmu_root, rodata_start - VM_KERNEL_BASE + kload_addr, rodata_start, rodata_length, PROT_READ, 0);
	mmu_map_range(kernel_address_space->mmu_root, databss_start - VM_KERNEL_BASE + kload_addr, databss_start, databss_length, PROT_READ | PROT_WRITE, 0);

	for(size_t i = 0; i < memmap.num_regions; i++)
	{
		const auto& region = memmap.regions[i];

		if(region.type != mm::mem_region::RegionType::Usable && region.type != mm::mem_region::RegionType::Allocated && region.type != mm::mem_region::RegionType::ACPI_Table)
			continue;

		auto begin = page_align_down(region.begin);
		auto length = page_align(region.end) - begin;
		mmu_map_range(kernel_address_space->mmu_root, begin, begin + VM_DMAP_BASE, length, PROT_READ | PROT_WRITE, 0);
	}

	smp_current_cpu()->set_pagetable(kernel_address_space->mmu_root);

	zero_cowmem = pmm_allocate();
	memset((void*)(zero_cowmem + VM_DMAP_BASE), 0, MMU_PAGE_SIZE);
}

static ssize_t vmstat_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	format_to(string_span{(char*)buffer, length}, "free_pages: {}\nused_pages: {}", pmm_free_pages_count(), pmm_used_pages_count());
	return length;
}

static vfs::fs_file_ops vmstat_fops = 
{
	.read = vmstat_read
};

void vmm_late_init()
{
	procfs_create("vmstat", &vmstat_fops);
}

static bool space_check_range(vm_space* space, vm_object* range)
{
	vm_object* cur;

	if(range->base < space->start_address)
		return false;

	if(range->base + range->length >= space->end_address)
		return false;

	mutex_lock(space->lock);
	list_for_each_entry(cur, space->objects, list_node)
	{
		if(cur->base <= range->base)
		{
			if(cur->base + cur->length > range->base)
			{
				mutex_unlock(space->lock);
				return false;
			}
		}
		else
		{
			if(cur->base < range->base + range->length)
			{
				mutex_unlock(space->lock);
				return false;
			}

			break;
		}
	}

	if(list_empty(space->objects))
		list_add(space->objects, range->list_node);
	else
		list_add_tail(cur->list_node, range->list_node);

	mutex_unlock(space->lock);
	return true;
}

static bool space_find_free_range(vm_space* space, vm_object* range)
{
	mutex_lock(space->lock);

	virtaddr_t alloc_base = 0;
	vm_object* prev = nullptr;

	if(list_empty(space->objects))
	{
		alloc_base = space->start_address;
	}
	else
	{
		vm_object* cur;
		list_for_each_entry(cur, space->objects, list_node)
		{
			auto prev_end = prev ? prev->base + prev->length : space->start_address;
			if(prev_end + range->length <= cur->base)
			{
				alloc_base = prev_end;
				break;
			}

			if(cur->list_node.next == &space->objects)
				alloc_base = cur->base + cur->length;

			prev = cur;
		}
	}

	if(!alloc_base)
	{
		mutex_unlock(space->lock);
		return false;
	}

	range->base = alloc_base;
	if(!prev)
		list_add(space->objects, range->list_node);
	else
		list_add(prev->list_node, range->list_node);	

	mutex_unlock(space->lock);
	return true;
}

static void vm_free_range(vm_space* space, vm_object* range)
{
	size_t len = range->length;
	size_t addr = range->base;
	while(len)
	{
		mmu_unmap(space->mmu_root, addr);
		if(range->flags & VM_FLAG_FILE)
		{
		}
		else if(range->flags & VM_FLAG_DEVICE)
		{
		}
		else
		{
			auto mapping = vm_space_get_mapping(space, addr);
			if(mapping.base && mapping.base != zero_cowmem && mapping.flags & VM_FLAG_OWNER)
			{
				pmm_free(mapping.base);
			}
		}

		addr += MMU_PAGE_SIZE;
		len -= (len < MMU_PAGE_SIZE) ? len : MMU_PAGE_SIZE;
	}
	mmu_invalidate(space->mmu_root, range->base, range->length);
	kfree(range);
}

virtaddr_t vm_space_map(vm_space* space, const vm_mapping_info& info)
{
	size_t length = page_align(info.length);
	
	const bool is_device = info.flags & VM_FLAG_DEVICE;
	if(is_device)
	{
		if(!info.phys_base)
			panic("vm_space_map: DEVICE mapping requires physical memory backing");
	}
	else
	{
		if(info.phys_base)
			panic("vm_space_map: physical base allowed only for DEVICE mappings");
	}
	
	uint32_t prot = info.prot;
	uint32_t vmflags = info.flags;
	const bool prefault = vmflags & VM_FLAG_ALLOCATED; 
	const bool read_access = prot & PROT_READ;
	const bool write_access = prot & PROT_WRITE;
	
	bool cow = (write_access && !prefault && !is_device);
	if(cow)
	{
		vmflags |= VM_FLAG_COW;
	}
	
	vm_object* range = (vm_object*)kmalloc(sizeof(vm_object));
	range->base = page_align_down(info.virt_base);
	range->length = length;
	range->prot = vm_prot_t(info.prot);
	range->flags = vm_flags_t(vmflags);
	list_node_init(range->list_node);

	bool valid = false;

	if(info.virt_base)
	{
		valid = space_check_range(space, range);
	}
	else
	{
		valid = space_find_free_range(space, range);
	}

	if(!valid)
	{
		kfree(range);
		return 0;
	}

	/*
	 *  COW pages have PROT_WRITE set in VMM bookkeeping structures
	 *  but we have to remove it on the actual page tables
	 */  
	if(cow)	
		prot &= (~PROT_WRITE);

	physaddr_t phys = is_device ? page_align_down(info.phys_base) : zero_cowmem;
	mutex_lock(space->lock);
	uint32_t mmuflags = 0;
	virtaddr_t alloc_base = range->base;
	while(length)
	{
		if(!is_device)
		{
			if(prefault)
			{
				space->resident_anon++;
				phys = pmm_allocate();
				mmuflags = VM_FLAG_OWNER;
			}
			else
				phys = zero_cowmem;
		}

		if(prot & PROT_READ)
			mmu_map(space->mmu_root, phys, alloc_base, prot, mmuflags);
		
		if(is_device)
			phys += MMU_PAGE_SIZE;
			
		space->mapped_anon++;
		alloc_base += MMU_PAGE_SIZE;
		length -= (length < MMU_PAGE_SIZE) ? length : MMU_PAGE_SIZE;
	}
	mutex_unlock(space->lock);

	return range->base;
}

void vm_space_unmap(vm_space* space, virtaddr_t addr, size_t length)
{
	mutex_lock(space->lock);
	vm_object* tmp;
	vm_object* cur;

	list_for_each_entry_safe(cur, tmp, space->objects, list_node)
	{
		if(cur->base == addr)
		{
			if(length && length != cur->length)
				log::warn("vm_space_unmap: partial unmap unsupported!");

			list_del(cur->list_node);

			vm_free_range(space, cur);
			goto finish;
		}
	}

	log::warn("vm_space_unmap: invalid range");
finish:
	mutex_unlock(space->lock);
}

vm_object* vm_space_get_range(vm_space* space, virtaddr_t base)
{
	vm_object* cur;
	mutex_lock(space->lock);
	list_for_each_entry(cur, space->objects, list_node)
	{
		if(cur->base == base)
		{
			mutex_unlock(space->lock);
			return cur;
		}
	}
	mutex_unlock(space->lock);
	return nullptr;
}

vm_mapping vm_space_get_mapping(vm_space* space, virtaddr_t base)
{
	return mmu_get_phys(space->mmu_root, base);
}

void vm_clone_kernel(vm_space* dest)
{
	for(int i = 256; i < 512; i++)
		dest->mmu_root[i] = kernel_address_space->mmu_root[i];
}

vm_space* vm_get_kernel_space()
{
	return kernel_address_space;
}

vm_space* vm_userspace_new()
{
	auto* space = (vm_space*)kmalloc(sizeof(vm_space));
	list_node_init(space->objects);
	space->start_address = VM_USERSPACE_BASE;
	space->end_address = VM_USERSPACE_END;
	
	space->mmu_root = mmu_new_pgdir();
	mutex_init(space->lock);

	space->mapped_anon = 0;
	space->mapped_file = 0;
	space->resident_anon = 0;
	space->resident_file = 0;

	vm_clone_kernel(space);
	return space;
}

void vm_space_destroy(vm_space* space)
{
	mutex_lock(space->lock);
	
	vm_object* tmp;
	vm_object* cur;
	list_for_each_entry_safe(cur, tmp, space->objects, list_node)
	{
		vm_free_range(space, cur);
	}	

	mmu_destroy(space->mmu_root);
	mutex_unlock(space->lock);
	kfree(space);
}

bool vm_page_fault(virtaddr_t addr, uint32_t flags)
{
	if(addr < VM_USERSPACE_BASE || addr > VM_USERSPACE_END)
		return false;

	auto* task = smp_current_cpu()->get_current_task();
	if(!task)
		return false;

	vm_space* space = task->current_vm_space;
	vm_object* range = vm_space_get_range(space, addr);
	if(!range)
		return false;

	if((flags & VM_FAULT_WRITE) && !(range->prot & PROT_WRITE))
		return false;
	if((flags & VM_FAULT_USER) && !(range->prot & PROT_USER))
		return false;
	if((flags & VM_FAULT_FETCH) && !(range->prot & PROT_EXEC))
		return false;

	auto mapping = vm_space_get_mapping(space, addr);

	bool present = flags & VM_FAULT_PRESENT;
	if(present)
	{
		if((flags & VM_FAULT_WRITE) && (range->prot & (PROT_READ | PROT_WRITE)) && (range->flags & VM_FLAG_COW))
		{
			physaddr_t new_phys = pmm_allocate();
			if(!new_phys)
				return false;

			if(mapping.base == zero_cowmem) 
			{
				memset((void*)(new_phys + VM_DMAP_BASE), 0, MMU_PAGE_SIZE);
			}
		       	else
			{
				memcpy((void*)(new_phys + VM_DMAP_BASE), (void*)(mapping.base + VM_DMAP_BASE), MMU_PAGE_SIZE);
				//FIXME: need some mechanism to release orphaned COW pages	
			}

			mutex_lock(space->lock);
			mmu_map(space->mmu_root, new_phys, addr, range->prot | PROT_WRITE, range->flags | VM_FLAG_OWNER);
			mmu_invalidate(space->mmu_root, addr, MMU_PAGE_SIZE);
			space->resident_anon++;
			mutex_unlock(space->lock);
			return true;	
		}

		return false;
	}
	else
	{
		return false;
	}
}
