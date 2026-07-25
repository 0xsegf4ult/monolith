#include <mm/slab.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <libk/string.h>
#include <sys/spinlock.h>
#include <klog.h>
#include <panic.h>
#include <types.h>

static struct slab_cache km_caches[] =
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

constexpr uint32_t km_cache_count = 8;

void slab_init()
{
	for(auto i = 0; i < km_cache_count; i++)
	{
		km_caches[i].obj_per_slab = (4096 - align_up(sizeof(struct slab), km_caches[i].block_size)) / km_caches[i].block_size;
		spinlock_init(&km_caches[i].lock);
	}
}

void* slab_alloc(struct slab_cache* cache)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&cache->lock, &flags);

	struct slab* prev = nullptr;
	struct slab* target_slab = cache->slabs;
	while(target_slab != nullptr)
	{
		if(target_slab->in_use < cache->obj_per_slab)
		{
			if(target_slab->free >= cache->obj_per_slab)
				panic("slab: freelist corruption");
			
			byte* mem = target_slab->memory + (cache->block_size * target_slab->free);
			target_slab->free = *(uint32_t*)mem;
			target_slab->in_use++;

			spinlock_release_irqsave(&cache->lock, flags);
			return (void*)mem;
		}

		prev = target_slab;
		target_slab = target_slab->next;
	}

	physaddr_t alloc_phys = pmm_allocate();
	struct slab* new_slab = (struct slab*)(alloc_phys + VM_DMAP_BASE);
	new_slab->next = nullptr;

	if(prev)
		prev->next = new_slab;
	else
		cache->slabs = new_slab;

	new_slab->memory = (byte*)(new_slab) + align_up(sizeof(struct slab), cache->block_size);
	new_slab->cache = cache;
	new_slab->in_use = 1;
	new_slab->free = 1;

	for(uint32_t i = 1; i < cache->obj_per_slab; i++)
		*(uint32_t*)(new_slab->memory + (i * cache->block_size)) = i + 1;

	spinlock_release_irqsave(&cache->lock, flags);
	return new_slab->memory;
}

void slab_free(virtaddr_t addr)
{
	struct slab* o_slab = (struct slab*)(addr & ~(0xFFF));
	if(o_slab->in_use == 0)
		panic("slab: attempted free on empty slab");

	if(addr < ((addr & ~(0xFFF)) + sizeof(struct slab)))
		panic("slab: free on metadata pointer");

	struct slab_cache* cache = o_slab->cache;
	memset((void*)addr, 0x6b, cache->block_size);

	uint64_t flags;
	spinlock_acquire_irqsave(&cache->lock, &flags);

	*(uint32_t*)addr = o_slab->free;

	auto obj_index = (addr - (virtaddr_t)o_slab->memory) / cache->block_size;
	o_slab->in_use--;
	o_slab->free = obj_index;
	spinlock_release_irqsave(&cache->lock, flags);
}

void* kmalloc(size_t size)
{
	for(int i = 0; i < km_cache_count; i++)
	{
		if(km_caches[i].block_size >= size)
			return slab_alloc(&km_caches[i]);
	}

	panic("kmalloc: allocation %d too big", size);
	return nullptr;
}

void kfree(void* addr)
{
	slab_free((virtaddr_t)addr);
}
