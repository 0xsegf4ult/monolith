#include <mm/slab.hpp>
#include <mm/pmm.hpp>
#include <mm/layout.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>
#include <lib/klog.hpp>

namespace mm
{

static slab_cache km_caches[] =
{
	{"kmalloc-8", nullptr, 8, 0},
	{"kmalloc-16", nullptr, 16, 0},
	{"kmalloc-32", nullptr, 32, 0},
	{"kmalloc-64", nullptr, 64, 0},
	{"kmalloc-128", nullptr, 128, 0},
	{"kmalloc-256", nullptr, 256, 0},
	{"kmalloc-512", nullptr, 512, 0},
	{"kmalloc-1024", nullptr, 1024, 0}
};

static constexpr uint32_t km_cache_count = 8;

void slab_init()
{
	for(auto i = 0; i < km_cache_count; i++)
		km_caches[i].obj_per_slab = (4096 - sizeof(slab)) / km_caches[i].block_size;
}

byte* slab_alloc(slab_cache& cache)
{
	slab* prev = nullptr;
	slab* target_slab = cache.slabs;
	while(target_slab != nullptr)
	{
		if((virtaddr_t)target_slab < 0x7fffffffffff)
			panic("slab_{}: invalid slab address", cache.name);
		
		if(target_slab->in_use < cache.obj_per_slab)
		{
			if(target_slab->free >= cache.obj_per_slab)
				panic("slab: freelist corruption");
			
			byte* mem = target_slab->memory + (cache.block_size * target_slab->free);
			target_slab->free = *reinterpret_cast<uint32_t*>(mem);
			target_slab->in_use++;
			return mem;
		}

		prev = target_slab;
		target_slab = target_slab->next;
	}

	physaddr_t alloc_phys = pmm_allocate();
	auto* new_slab = reinterpret_cast<slab*>(alloc_phys + mm::direct_mapping_offset);
	new_slab->next = nullptr;

	if(prev)
		prev->next = new_slab;
	else
		cache.slabs = new_slab;

	new_slab->memory = reinterpret_cast<byte*>(new_slab) + sizeof(slab);
	new_slab->cache = &cache;
	new_slab->in_use = 1;
	new_slab->free = 1;

	for(uint32_t i = 1; i < cache.obj_per_slab; i++)
		*reinterpret_cast<uint32_t*>(new_slab->memory + (i * cache.block_size)) = i + 1;

	return new_slab->memory;
}

void slab_free(virtaddr_t addr)
{
	auto* o_slab = reinterpret_cast<slab*>(addr & ~(0xFFF));
	if(o_slab->in_use == 0)
		panic("slab: attempted free on empty slab");

	if(addr < ((addr & ~(0xFFF)) + sizeof(slab)))
		panic("slab: free on metadata pointer");

	slab_cache& cache = *(o_slab->cache);

	*reinterpret_cast<uint32_t*>(addr) = o_slab->free;

	auto obj_index = (addr - reinterpret_cast<virtaddr_t>(o_slab->memory)) / cache.block_size;
	o_slab->in_use--;
	o_slab->free = obj_index;
}

void slab_debug(size_t size)
{
	slab_cache* cache;
	for(int i = 0; i < km_cache_count; i++)
	{
		if(km_caches[i].block_size >= size)
		{
			cache = &km_caches[i];
			break;
		}
	}
	
	if(!cache)
		return;

	log::debug("SLAB_DEBUG_{} block {} objs {}", cache->name, cache->block_size, cache->obj_per_slab);

	slab* current = cache->slabs;
	while(current)
	{
		log::debug("SLAB {} used first_free {}", current->in_use, current->free);
		current = current->next;
	}

}

}

void* kmalloc(size_t size)
{
	using namespace mm;
	for(int i = 0; i < km_cache_count; i++)
	{
		if(km_caches[i].block_size >= size)
			return slab_alloc(km_caches[i]);
	}

	panic("kmalloc: allocation too large");
	return nullptr;
}

void kfree(void* addr)
{
	mm::slab_free(reinterpret_cast<virtaddr_t>(addr));
}
