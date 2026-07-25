#include <mm/memory_map.h>
#include <arch/x86_64/limine.h>
#include <klog.h>
#include <types.h>

const char* region_type_strings[] =
{
	"usable",
	"ACPI tables",
	"ACPI NVS",
	"reserved",
	"allocated"
};

void parse_memory_map(memory_map_t* out, virtaddr_t source, size_t src_entry_count)
{
	struct limine_memmap_entry** entries = (struct limine_memmap_entry**)source;

	for(struct limine_memmap_entry* entry = entries[0]; entry < entries[0] + src_entry_count; entry++)
	{
		mem_region_t* region = &out->regions[out->num_regions];

		region->begin = entry->base;
		region->end = entry->base + entry->length;

		if(region->begin == 0)
			region->begin = 0x2000;

		switch(entry->type)
		{
		case LIMINE_MEMMAP_USABLE:
			if(region->end > out->memory_top)
				out->memory_top = region->end;

			region->type = MEM_REGION_USABLE;
			break;
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
			region->type = MEM_REGION_ACPI_TABLES;
			break;
		case LIMINE_MEMMAP_ACPI_NVS:
			region->type = MEM_REGION_ACPI_NVS;
			break;
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
		case LIMINE_MEMMAP_FRAMEBUFFER:
			region->type = MEM_REGION_ALLOCATED;
			break;
		case LIMINE_MEMMAP_BAD_MEMORY:
		case LIMINE_MEMMAP_RESERVED:
		default:
			region->type = MEM_REGION_RESERVED;
			break;
		}

		klog("memmap: [mem %016p - %016p] %s\n", region->begin, region->end, region_type_strings[region->type]);
		out->num_regions++;

		if(out->num_regions >= max_memmap_regions)
		{
			klog("memmap: too many memory regions %d\n", out->num_regions);
			break;
		}
	}
}
