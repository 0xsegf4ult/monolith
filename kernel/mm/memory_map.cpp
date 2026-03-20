#include <mm/memory_map.hpp>
#include <mm/layout.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <limine.h>

constexpr const char* region_type_strings[6] =
{
	"usable",
	"kernel",
	"ACPI tables",
	"ACPI NVS",
	"reserved",
	"allocated"
};

namespace mm
{

void* memory_map::reserve(size_t size)
{
	for(mem_region* region = regions; region != regions + num_regions; region++)
	{
		if(region->type != mem_region::RegionType::Usable)
			continue;

		physaddr_t begin = region->begin;
		physaddr_t end = region->end;

		if(begin < end && end - begin >= size)
		{
			if(num_regions == max_regions)
				return nullptr;
			log::info("memmap: update region [{:#x} - {:#x}] -> [{:#x} - {:#x}]", region->begin, region->end, mm::page_align(region->begin + size), region->end);
			region->begin = mm::page_align(begin + size);

			mem_region* new_region = regions + num_regions;

			new_region->begin = begin;
			new_region->end = mm::page_align(begin + size);
			new_region->type = mem_region::RegionType::Allocated;

			log::info("memmap: region [{:#x} - {:#x}] -> allocated", new_region->begin, new_region->end);

			num_regions++;
			return reinterpret_cast<void*>(begin + mm::direct_mapping_offset);
		}
	}

	return nullptr;
}

memory_map parse_memmap(limine_memmap_entry** entries, size_t entry_count)
{
	memory_map mem_map;

	if(entry_count > memory_map::max_regions)
		panic("failed to parse memory map, too many entries");

	for(limine_memmap_entry* entry = entries[0]; entry < entries[0] + entry_count; entry++)
	{
		auto& region = mem_map.regions[mem_map.num_regions];

		region.begin = entry->base;
		region.end = entry->base + entry->length;

		if(region.begin == 0)
			region.begin = 0x1000;

		switch(entry->type)
		{
		case LIMINE_MEMMAP_USABLE:
			if(region.end > mem_map.memory_top)
				mem_map.memory_top = region.end;

			region.type = mem_region::RegionType::Usable;
			break;
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
			region.type = mem_region::RegionType::ACPI_Table;
			break;
		case LIMINE_MEMMAP_ACPI_NVS:
			region.type = mem_region::RegionType::ACPI_NVS;
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

		log::info("memmap: [mem {:#x} - {:#x}] {}", region.begin, region.end, region_type_strings[static_cast<int>(region.type)]);
		mem_map.num_regions++;
	}

	return mem_map;
}

}
