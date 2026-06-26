#include <mm/vmm.hpp>
#include <mm/address_space.hpp>
#include <mm/memory_map.hpp>
#include <mm/layout.hpp>
#include <mm/slab.hpp>
#include <mm/pmm.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/x86_64/smp.hpp>

#include <fs/vfs.hpp>
#include <fs/procfs/procfs.hpp>

#include <sys/thread.hpp>

#include <klog.hpp>
#include <kstd.hpp>
#include <types.hpp>

static address_space* kernel_address_space = nullptr;

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
	kernel_address_space = (address_space*)kmalloc(sizeof(address_space));
	kernel_address_space->init(0xffffc00000000000);

	auto text_start = mm::page_align_down(reinterpret_cast<virtaddr_t>(&_text_start));
	auto text_length = mm::page_align(reinterpret_cast<virtaddr_t>(&_text_end) - reinterpret_cast<virtaddr_t>(&_text_start));
	auto rodata_start = mm::page_align_down(reinterpret_cast<virtaddr_t>(&_rodata_start));
	auto rodata_length = mm::page_align(reinterpret_cast<virtaddr_t>(&_rodata_end) - reinterpret_cast<virtaddr_t>(&_rodata_start));
	auto databss_start = mm::page_align_down(reinterpret_cast<virtaddr_t>(&_data_start));
	auto databss_length = mm::page_align(reinterpret_cast<virtaddr_t>(&_bss_end) - reinterpret_cast<virtaddr_t>(&_data_start));

	log::info("mm: mapping kernel .text [{:#x} - {:#x}] R-X", text_start, text_start + text_length);
	mmu_map_range(get_kernel_vmspace()->root_pml4, text_start - mm::kernel_mapping_offset + kload_addr, text_start, text_length, vm_flags_to_x86(vm_exec | vm_present));
	log::info("mm: mapping kernel .rodata [{:#x} - {:#x}] R--", rodata_start, rodata_start + rodata_length);
	mmu_map_range(get_kernel_vmspace()->root_pml4, rodata_start - mm::kernel_mapping_offset + kload_addr, rodata_start, rodata_length, vm_flags_to_x86(vm_present));
	log::info("mm: mapping kernel .data .bss [{:#x} - {:#x}] RW-", databss_start, databss_start + databss_length);
	mmu_map_range(get_kernel_vmspace()->root_pml4, databss_start - mm::kernel_mapping_offset + kload_addr, databss_start, databss_length, vm_flags_to_x86(vm_write | vm_present));

	log::info("mm: HHDM mapping physical memory RW-");
	for(size_t i = 0; i < memmap.num_regions; i++)
	{
		const auto& region = memmap.regions[i];

		if(region.type != mm::mem_region::RegionType::Usable && region.type != mm::mem_region::RegionType::Allocated && region.type != mm::mem_region::RegionType::ACPI_Table)
			continue;

		auto begin = mm::page_align_down(region.begin);
		auto length = mm::page_align(region.end) - begin;
		mmu_map_range(get_kernel_vmspace()->root_pml4, begin, begin + mm::direct_mapping_offset, length, vm_flags_to_x86(vm_write | vm_present));
	}

	kernel_address_space->switch_to();
}

ssize_t vmstat_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
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

virtaddr_t vmalloc(size_t length, uint64_t flags, void* arg)
{
	return kernel_address_space->alloc(length, flags | vm_present, arg);
}

void vmfree(virtaddr_t addr)
{
	kernel_address_space->free(addr);
}

int vm_map(physaddr_t phys, virtaddr_t virt, uint64_t flags)
{
	return kernel_address_space->map(phys, virt, flags);
}

int vm_map_range(physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags)
{
	return kernel_address_space->map_range(phys, virt, length, flags);
}

void clone_kernel_vm(address_space* dest)
{
	for(int i = 255; i < 512; i++)
		dest->root_pml4[i] = kernel_address_space->root_pml4[i];
}

address_space* get_kernel_vmspace()
{
	return kernel_address_space;
}

bool vm_page_fault(virtaddr_t addr, uint64_t flags)
{
	if(flags & pf_present)
		return false;

	auto* thr = smp_current_cpu()->get_current_thread();
	if(!thr)
		return false;

	address_space* as = thr->vm_space;
	
	uint64_t status = 0;
	physaddr_t phys = as->get_mapping(addr, &status);
	log::debug("mapped to phys {:x} flags {:b} fault {:b}", phys, status, flags);
	if(!phys)
		return false;

	if((flags & pf_write) && !(status & vm_write))
	{
		log::debug("tried write to readonly page");
		return false;
	}
	if((flags & pf_user) && !(status & vm_user))
	{
		log::debug("tried userspace write to kernel page");
		return false;
	}
	if((flags & pf_fetch) && !(status & vm_exec))
	{
		log::debug("tried instruction fetch from noexec page");
		return false;
	}

	if(status & vm_swapped)
	{
		log::debug("page is swapped out");
		if(phys == vm_allocate)
		{
			phys = pmm_allocate();
			if(!phys)
				return false;

			// FIXME: wont update flags in vm_object, but those are unused for now
			mutex_lock(as->lock);
			mmu_map(as->root_pml4, phys, addr, vm_flags_to_x86(status | vm_present));
			mutex_unlock(as->lock);
			log::debug("allocated lazy page {:x} for {:x}", phys, addr);
			return true;
		}
		return false;
	}

	return false;
}
