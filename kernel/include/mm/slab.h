#pragma once

#include <sys/spinlock.h>
#include <types.h>

struct slab_cache;

struct slab
{
	struct slab* next;
	byte* memory;
	struct slab_cache* cache;
	uint32_t in_use;
	uint32_t free;
};

struct slab_cache
{
	const char* name;
	struct slab* slabs;
	uint16_t block_size;
	uint16_t obj_per_slab;
	spinlock_t lock;
};

void slab_init();
void* slab_alloc(struct slab_cache* cache);
void slab_free(virtaddr_t addr);

void* kmalloc(size_t size);
void kfree(void* addr);
