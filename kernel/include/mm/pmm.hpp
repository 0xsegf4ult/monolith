#pragma once

#include <types.hpp>

namespace mm
{

struct memory_map;

}

void pmm_initialize(mm::memory_map& memmap);

physaddr_t pmm_allocate();
void pmm_free(physaddr_t addr);

size_t pmm_free_pages_count();
size_t pmm_used_pages_count();
