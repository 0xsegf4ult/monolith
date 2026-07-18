#pragma once

#include <types.h>

enum mem_region_type : uint32_t
{
	MEM_REGION_USABLE,
	MEM_REGION_ACPI_TABLES,
	MEM_REGION_ACPI_NVS,
	MEM_REGION_RESERVED,
	MEM_REGION_ALLOCATED
};

typedef struct 
{
	physaddr_t begin;
	physaddr_t end;
	uint32_t type;
} mem_region_t;

constexpr size_t max_memmap_regions = 64;

typedef struct memory_map memory_map_t;
struct memory_map
{
	mem_region_t regions[max_memmap_regions];
	physaddr_t memory_top;
	uint32_t num_regions;
};

void parse_memory_map(memory_map_t* out, virtaddr_t source, size_t src_entry_count);
void* memmap_reserve(memory_map_t* memmap, size_t size);
