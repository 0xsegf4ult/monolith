#pragma once

#include <mm/vm_object.hpp>
#include <lib/types.hpp>

struct page_table;

struct address_space
{
	vm_object* objects;
	virtaddr_t base_addr{0};

	page_table* root_pml4;

	void init(virtaddr_t base = 0x10000);
	void switch_to();
	virtaddr_t alloc(size_t length, uint64_t flags = 0, void* arg = nullptr);
	virtaddr_t alloc_placed(virtaddr_t base, size_t length, uint64_t flags = 0, void* arg = nullptr);
	physaddr_t get_mapping(virtaddr_t mapping);
	void free(virtaddr_t addr);
	int map(physaddr_t phys, virtaddr_t virt, uint64_t flags = 0);
	int map_range(physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags = 0);
	int reserve_range(virtaddr_t base, size_t length = 0x1000, uint64_t flags = 0);
};
