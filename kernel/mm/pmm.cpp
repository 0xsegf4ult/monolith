#include <mm/pmm.hpp>
#include <mm/memory_map.hpp>
#include <mm/layout.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

static uint64_t* pmm_bitmap = nullptr;
static size_t pmm_bitmap_length = 0;
static size_t physmem_available = 0;
static size_t physmem_used = 0;

void pmm_mark_range_free(physaddr_t begin, physaddr_t end)
{
	auto rlen = end - begin;
	auto rbegin = begin;
	physmem_available += rbegin;

	while(rlen >= 4096)
	{
		auto bmp_offset = (rbegin / 4096) / 64;
		auto bit_offset = (rbegin / 4096) % 64;
		
		if(rlen >= 64 * 4096 && bit_offset == 0)
		{
			pmm_bitmap[bmp_offset] = ~(0ull);
			rlen -= (64 * 4096);
			rbegin += (64 * 4096);
			continue;
		}
		
		pmm_bitmap[bmp_offset] |= (1ull << bit_offset);

		rlen -= 4096;
		rbegin += 4096;
	}
}

void pmm_initialize(mm::memory_map& memmap)
{
	const auto required_bits = mm::page_align(memmap.memory_top / 8) / 4096;
	const auto required_u64 = (required_bits + 63) / 64;
	pmm_bitmap_length = required_u64;

	auto pmm_block = memmap.reserve(required_u64 * sizeof(uint64_t));
	if(!pmm_block)
		panic("failed to reserve memory for pmm_bitmap");

	pmm_bitmap = reinterpret_cast<uint64_t*>(pmm_block);
	memset(pmm_bitmap, 0, pmm_bitmap_length * sizeof(uint64_t));
		
	for(size_t i = 0; i < memmap.num_regions; i++)
	{
		const auto& region = memmap.regions[i];
		if(region.type == mm::mem_region::RegionType::Usable)
			pmm_mark_range_free(region.begin, region.end);
		else
			physmem_used += (region.end - region.begin);
	}
}

physaddr_t pmm_allocate()
{
	for(size_t i = 0; i < pmm_bitmap_length; i++)
	{
		auto cur_word = pmm_bitmap[i];
		if(cur_word == 0)
			continue;

		int index = __builtin_ctzll(cur_word);
		pmm_bitmap[i] &= ~(1ull << index);
		physmem_available -= 4096;
		physmem_used += 4096;

		return (i * 64ull + index) * 4096ull;
	}

	panic("out of physical memory");
	return ~(0ull);
}

void pmm_free(physaddr_t addr)
{
	auto bmp_offset = (addr / 4096) / 64;
	auto bit_offset = (addr / 4096) % 64;

	pmm_bitmap[bmp_offset] |= (1ull << bit_offset);
	physmem_used -= 4096;
	physmem_available += 4096;
}


