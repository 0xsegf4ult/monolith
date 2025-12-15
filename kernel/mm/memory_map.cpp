#include <mm/memory_map.hpp>
#include <mm/layout.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <limine.h>

constexpr const char* region_type_strings[5] =
{
	"usable",
	"kernel",
	"ACPI",
	"reserved",
	"allocated"
};

namespace mm
{

void* MemoryMap::reserve(size_t size)
{
	for(mem_region* region = regions; region != regions + num_regions; region++)
	{
		if(region->type != mem_region::RegionType::Usable)
			continue;

		physaddr_t start = region->start;
		physaddr_t end = region->end;

		if(start < end && end - start >= size)
		{
			if(num_regions == max_regions)
				return nullptr;

			log::info("memmap: update region [{:x} - {:x}] -> [{:x} - {:x}]", region->start, region->end, mm::page_align(region->start + size), region->end);
			region->start = mm::page_align(start + size);

			mem_region* new_region = regions + num_regions;

			new_region->start = start;
			new_region->end = mm::page_align(start + size);
			new_region->type = mem_region::RegionType::Allocated;

			log::info("memmap: region [{:x} - {:x}] -> allocated", new_region->start, new_region->end);

			num_regions++;
			return reinterpret_cast<void*>(start + mm::direct_mapping_offset);
		}
	}

	return nullptr;
}

MemoryMap parse_memmap(limine_memmap_entry** entries, size_t entry_count)
{
	MemoryMap mem_map;

	if(entry_count > MemoryMap::max_regions)
		panic("failed to parse memory map, too many entries");

	for(limine_memmap_entry* entry = entries[0]; entry < entries[0] + entry_count; entry++)
	{
		auto& region = mem_map.regions[mem_map.num_regions];

		region.start = entry->base;
		region.end = entry->base + entry->length;

		switch(entry->type)
		{
		case LIMINE_MEMMAP_USABLE:
			if(region.end > mem_map.memory_top)
				mem_map.memory_top = region.end;

			region.type = mem_region::RegionType::Usable;
			break;
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		case LIMINE_MEMMAP_ACPI_NVS:
			region.type = mem_region::RegionType::ACPI;
			break;
		case LIMINE_MEMMAP_BAD_MEMORY:
		case LIMINE_MEMMAP_RESERVED:
		default:
			region.type = mem_region::RegionType::Reserved;
			break;
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
		case LIMINE_MEMMAP_FRAMEBUFFER:
			if(region.end > mem_map.memory_top)
				mem_map.memory_top = region.end;
			region.type = mem_region::RegionType::Allocated;
			break;
		}

		log::info("memmap: [mem {:x} - {:x}] {}", region.start, region.end, region_type_strings[static_cast<int>(region.type)]);
		mem_map.num_regions++;
	}

	return mem_map;
}

}
