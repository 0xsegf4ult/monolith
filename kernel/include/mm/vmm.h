#pragma once

struct memory_map;

#include <mm/mmu.h>
#include <mm/vm_space.h>
#include <types.h>

constexpr size_t VM_USERSPACE_BASE	= 0x0000000000010000;
constexpr size_t VM_USERSPACE_END 	= 0x00007fffffffffff;
constexpr size_t VM_DMAP_BASE 		= 0xffff800000000000;
constexpr size_t VM_VMALLOC_BASE	= 0xffffc00000000000;
constexpr size_t VM_KERNEL_BASE  	= 0xffffffff80000000;

void vmm_init_kpages(struct memory_map* memmap, physaddr_t kload_addr);
void vmm_late_init();

typedef struct 
{
	size_t length;
	uint32_t prot;
	uint32_t flags;

	physaddr_t phys_base;
	virtaddr_t virt_base;

	off_t offset;	
	int fd;
} vm_mapping_info;

virtaddr_t vm_space_map(struct vm_space* space, vm_mapping_info info);
void vm_space_unmap(struct vm_space* space, virtaddr_t addr, size_t length);
struct vm_object* vm_space_get_range(struct vm_space* space, virtaddr_t base);
vm_mapping vm_space_get_mapping(struct vm_space* space, virtaddr_t base);

struct vm_space* vm_get_kernel_space();
struct vm_space* vm_userspace_new();
void vm_space_destroy(struct vm_space* space);

bool vm_page_fault(virtaddr_t address, uint32_t status);
bool vm_validate_ptr(const void* ptr, size_t size);

static inline void* vmalloc(size_t length)
{
	return (void*)vm_space_map(vm_get_kernel_space(),
	(vm_mapping_info)
	{
		.length = length,
		.prot = PROT_READ | PROT_WRITE,
		.flags = VM_FLAG_ALLOCATE,
	});
}

static inline void vfree(void* addr)
{
	vm_space_unmap(vm_get_kernel_space(), (virtaddr_t)addr, 0);
}
