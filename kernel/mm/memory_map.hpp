#pragma once

#include <lib/types.hpp>
struct limine_memmap_entry;

namespace mm
{

struct mem_region
{
	physaddr_t begin;
	physaddr_t end;

	enum class RegionType
	{
		Usable,
		Kernel,
		ACPI,
		Reserved,
		Allocated
	} type;
};

struct memory_map
{
	constexpr static size_t max_regions = 64;

	mem_region regions[max_regions];
	uint32_t num_regions{0};
	physaddr_t memory_top{0};

	void* reserve(size_t size);
};

memory_map parse_memmap(limine_memmap_entry** entries, size_t entry_count);

}
