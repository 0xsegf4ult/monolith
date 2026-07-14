#pragma once

#include <mm/mmu.hpp>
#include <mm/vm_space.hpp>
#include <types.hpp>

namespace mm
{

struct memory_map;

}

constexpr size_t VM_USERSPACE_BASE	= 0x0000000000010000;
constexpr size_t VM_USERSPACE_END 	= 0x00007fffffffffff;
constexpr size_t VM_DMAP_BASE 		= 0xffff800000000000;
constexpr size_t VM_VMALLOC_BASE	= 0xffffc00000000000;
constexpr size_t VM_KERNEL_BASE  	= 0xffffffff80000000;

void vmm_init_kpages(mm::memory_map& memmap, physaddr_t kload_addr);
void vmm_late_init();

struct vm_mapping_info
{
	size_t length = 0;
	uint32_t prot = PROT_NONE;
	uint32_t flags = VM_FLAG_NONE;

	physaddr_t phys_base = 0;
	virtaddr_t virt_base = 0;

	off_t offset = 0;	
	int fd = -1;
};

virtaddr_t vm_space_map(vm_space* space, const vm_mapping_info& info);
void vm_space_unmap(vm_space* space, virtaddr_t addr, size_t length);
vm_object* vm_space_get_range(vm_space* space, virtaddr_t base);
vm_mapping vm_space_get_mapping(vm_space* space, virtaddr_t base);

vm_space* vm_get_kernel_space();
vm_space* vm_userspace_new();
void vm_space_destroy(vm_space* space);

bool vm_page_fault(virtaddr_t address, uint32_t status);

constexpr virtaddr_t vmalloc(size_t length)
{
	return vm_space_map(vm_get_kernel_space(),
	{
		.length = length,
		.prot = PROT_READ | PROT_WRITE,
		.flags = VM_FLAG_ALLOCATED,
	});
}

constexpr void vfree(virtaddr_t addr)
{
	vm_space_unmap(vm_get_kernel_space(), addr, 0);
}
