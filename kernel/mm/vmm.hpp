#pragma once

#include <mm/vm_object.hpp>
#include <lib/types.hpp>

namespace mm
{

struct memory_map;

}

void vmm_init_kpages(mm::memory_map& memmap, physaddr_t kload_addr);

virtaddr_t vmalloc(size_t length, vm_flags flags = vm_none);
void vfree(virtaddr_t addr);
void vm_map(physaddr_t phys, virtaddr_t virt, vm_flags flags = vm_none);
void vm_map_range(physaddr_t phys, virtaddr_t virt, size_t length, vm_flags flags = vm_none);
