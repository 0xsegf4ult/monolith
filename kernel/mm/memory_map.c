#include <mm/memory_map.h>
#include <mm/vmm.h>

#include <config.h>
#include <klog.h>
#include <types.h>
#include <panic.h>

void* memmap_reserve(memory_map_t* memmap, size_t size)
{
	for(mem_region_t* region = memmap->regions; region != memmap->regions + memmap->num_regions; region++)
	{
		if(region->type != MEM_REGION_USABLE)
			continue;

		physaddr_t begin = region->begin;
		physaddr_t end = region->end;

		if(begin < end && end - begin >= size)
		{
			if(memmap->num_regions == max_memmap_regions)
			{
				klog("memmap: cannot split region\n");
				return nullptr;
			}
			
			region->begin = align_up(begin + size, CONFIG_PAGE_SIZE);

			mem_region_t* new_region = memmap->regions + memmap->num_regions;

			new_region->begin = begin;
			new_region->end = align_up(begin + size, CONFIG_PAGE_SIZE);
			new_region->type = MEM_REGION_ALLOCATED;

			klog("memmap: region [%p - %p] -> allocated\n", new_region->begin, new_region->end);

			memmap->num_regions++;
			return (void*)(begin + VM_DMAP_BASE);
		}
	}

	return nullptr;
}
