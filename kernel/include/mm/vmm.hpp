#pragma once

#include <mm/vm_object.hpp>
#include <types.hpp>

namespace mm
{

struct memory_map;

}

void vmm_init_kpages(mm::memory_map& memmap, physaddr_t kload_addr);
void vmm_late_init();

virtaddr_t vmalloc(size_t length, uint64_t flags = 0, void* arg = 0);
void vmfree(virtaddr_t addr);
int vm_map(physaddr_t phys, virtaddr_t virt, uint64_t flags = 0);
int vm_map_range(physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags = 0);

struct address_space;
void clone_kernel_vm(address_space* dest);
address_space* get_kernel_vmspace();

bool vm_page_fault(virtaddr_t address, uint64_t status);
