#pragma once

#include <lib/types.hpp>

namespace mm
{

struct memory_map;

}

void pmm_initialize(mm::memory_map& memmap);

physaddr_t pmm_allocate();
void pmm_free(physaddr_t addr);

