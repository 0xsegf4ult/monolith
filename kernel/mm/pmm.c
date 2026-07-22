#include <mm/pmm.h>
#include <mm/memory_map.h>
#include <mm/mmu.h>
#include <libk/string.h>
#include <sys/spinlock.h>
#include <config.h>
#include <types.h>
#include <panic.h>


static uint64_t* pmm_bitmap = nullptr;
static size_t pmm_bitmap_length = 0;
static size_t physmem_available = 0;
static size_t physmem_used = 0;
static spinlock_t lock;

static void pmm_mark_range_free(physaddr_t begin, physaddr_t end)
{
	auto rlen = end - begin;
	auto rbegin = begin;
	physmem_available += (rlen / CONFIG_PAGE_SIZE);

	while(rlen >= CONFIG_PAGE_SIZE)
	{
		auto bmp_offset = (rbegin / CONFIG_PAGE_SIZE) / 64;
		auto bit_offset = (rbegin / CONFIG_PAGE_SIZE) % 64;
		
		if(rlen >= 64 * CONFIG_PAGE_SIZE && bit_offset == 0)
		{
			pmm_bitmap[bmp_offset] = ~(0ull);
			rlen -= (64 * CONFIG_PAGE_SIZE);
			rbegin += (64 * CONFIG_PAGE_SIZE);
			continue;
		}
		
		pmm_bitmap[bmp_offset] |= (1ull << bit_offset);

		rlen -= CONFIG_PAGE_SIZE;
		rbegin += CONFIG_PAGE_SIZE;
	}
}

void pmm_init(memory_map_t* memmap)
{
	const auto required_bits = align_up(memmap->memory_top, CONFIG_PAGE_SIZE) / CONFIG_PAGE_SIZE;
	const auto required_u64 = (required_bits + 63) / 64;
	pmm_bitmap_length = required_u64;

	auto pmm_block = memmap_reserve(memmap, required_u64 * sizeof(uint64_t));
	if(!pmm_block)
		panic("failed to reserve memory for pmm_bitmap");

	pmm_bitmap = (uint64_t*)pmm_block;
	memset(pmm_bitmap, 0, pmm_bitmap_length * sizeof(uint64_t));
		
	for(size_t i = 0; i < memmap->num_regions; i++)
	{
		mem_region_t* region = memmap->regions + i;
		if(region->type == MEM_REGION_USABLE)
			pmm_mark_range_free(region->begin, region->end);
		else if(region->type == MEM_REGION_ALLOCATED)
			physmem_used += ((region->end - region->begin) / CONFIG_PAGE_SIZE);
	}

	spinlock_init(&lock);
}

physaddr_t pmm_allocate()
{
	uint64_t flags;
	spinlock_acquire_irqsave(&lock, &flags);

	for(size_t i = 0; i < pmm_bitmap_length; i++)
	{
		auto cur_word = pmm_bitmap[i];
		if(cur_word == 0)
			continue;

		int index = __builtin_ctzll(cur_word);
		pmm_bitmap[i] &= ~(1ull << index);
		physmem_available--;
		physmem_used++;

		spinlock_release_irqsave(&lock, flags);
		return (i * 64ull + index) * CONFIG_PAGE_SIZE;
	}

	spinlock_release_irqsave(&lock, flags);
	panic("out of physical memory %d kB used / %d kB free", physmem_used * 4, physmem_available * 4);
	return ~(0ull);
}

void pmm_free(physaddr_t addr)
{
	auto bmp_offset = (addr / CONFIG_PAGE_SIZE) / 64;
	auto bit_offset = (addr / CONFIG_PAGE_SIZE) % 64;

	uint64_t flags;
	spinlock_acquire_irqsave(&lock, &flags);
	pmm_bitmap[bmp_offset] |= (1ull << bit_offset);
	physmem_used--;;
	physmem_available++;
	spinlock_release_irqsave(&lock, flags);
}

size_t pmm_free_pages_count()
{
	return physmem_available;
}

size_t pmm_used_pages_count()
{
	return physmem_used;
}
