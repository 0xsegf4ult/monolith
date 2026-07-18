#pragma once

#include <types.h>

struct memory_map;

void pmm_init(struct memory_map* memmap);

physaddr_t pmm_allocate();
void pmm_free(physaddr_t addr);

size_t pmm_free_pages_count();
size_t pmm_used_pages_count();
