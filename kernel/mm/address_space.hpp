#pragma once

#include <mm/vm_object.hpp>
#include <lib/types.hpp>

struct page_table;

struct address_space
{
	vm_object* objects;
	virtaddr_t base_addr{0};

	page_table* root_pml4;

	void init(virtaddr_t base = 0x1000);
	void switch_to();
	virtaddr_t alloc(size_t length, vm_flags flags = vm_none, void* arg = nullptr);
	void free(virtaddr_t addr);
	void map(physaddr_t phys, virtaddr_t virt, vm_flags flags = vm_none);
	void map_range(physaddr_t phys, virtaddr_t virt, size_t length, vm_flags flags = vm_none);
	void reserve_range(virtaddr_t virt, size_t length = 0x1000, vm_flags flags = vm_none);
};
