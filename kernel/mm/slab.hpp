#pragma once

#include <lib/types.hpp>

namespace mm
{

struct slab_cache;

struct slab
{
	slab* next;
	byte* memory;
	slab_cache* cache;
	uint32_t in_use;
	uint32_t free;
};

struct slab_cache
{
	const char* name{nullptr};
	slab* slabs{nullptr};
	uint16_t block_size{0};
	uint16_t obj_per_slab{0};
};

void slab_init();
byte* slab_alloc(slab_cache& cache);
void slab_free(virtaddr_t addr);

}

void* kmalloc(size_t size);
void kfree(void* addr);
