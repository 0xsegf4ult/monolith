#include <mm/vmm.hpp>
#include <mm/address_space.hpp>
#include <mm/memory_map.hpp>
#include <mm/layout.hpp>
#include <mm/slab.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

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
	kernel_address_space->init(0xffff800000000000);

	auto text_start = mm::page_align_down(reinterpret_cast<virtaddr_t>(&_text_start));
	auto text_length = mm::page_align(reinterpret_cast<virtaddr_t>(&_text_end) - reinterpret_cast<virtaddr_t>(&_text_start));
	auto rodata_start = mm::page_align_down(reinterpret_cast<virtaddr_t>(&_rodata_start));
	auto rodata_length = mm::page_align(reinterpret_cast<virtaddr_t>(&_rodata_end) - reinterpret_cast<virtaddr_t>(&_rodata_start));
	auto databss_start = mm::page_align_down(reinterpret_cast<virtaddr_t>(&_data_start));
	auto databss_length = mm::page_align(reinterpret_cast<virtaddr_t>(&_bss_end) - reinterpret_cast<virtaddr_t>(&_data_start));

	log::info("mm: mapping kernel .text [{:x} - {:x}] R-X", text_start, text_start + text_length);
	kernel_address_space->map_range(text_start - mm::kernel_mapping_offset + kload_addr, text_start, text_length, vm_exec);
	log::info("mm: mapping kernel .rodata [{:x} - {:x}] R--", rodata_start, rodata_start + rodata_length);
	kernel_address_space->map_range(rodata_start - mm::kernel_mapping_offset + kload_addr, rodata_start, rodata_length);
	log::info("mm: mapping kernel .data .bss [{:x} - {:x}] RW-", databss_start, databss_start + databss_length);
	kernel_address_space->map_range(databss_start - mm::kernel_mapping_offset + kload_addr, databss_start, databss_length, vm_write);

	log::info("mm: HHDM mapping physical memory RW-");
	for(size_t i = 0; i < memmap.num_regions; i++)
	{
		const auto& region = memmap.regions[i];

		if(region.type != mm::mem_region::RegionType::Usable && region.type != mm::mem_region::RegionType::Allocated)
			continue;

		auto begin = mm::page_align_down(region.begin);
		auto length = mm::page_align(region.end) - begin;
		kernel_address_space->map_range(begin, begin + mm::direct_mapping_offset, length, vm_write);
	}

	kernel_address_space->switch_to();
}

virtaddr_t vmalloc(size_t length, vm_flags flags)
{
	return kernel_address_space->alloc(length, flags);
}

void vfree(virtaddr_t addr)
{
	kernel_address_space->free(addr);
}

void vm_map(physaddr_t phys, virtaddr_t virt, vm_flags flags)
{
	kernel_address_space->map(phys, virt, flags);
}

void vm_map_range(physaddr_t phys, virtaddr_t virt, size_t length, vm_flags flags)
{
	kernel_address_space->map_range(phys, virt, length, flags);
}
