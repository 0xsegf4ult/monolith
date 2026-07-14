#pragma once

#include <config.hpp>
#include <types.hpp>

class page_table;

struct vm_mapping
{
	physaddr_t base;
	uint32_t prot;
	uint32_t flags;
	bool present;
};

void mmu_map(page_table* table, physaddr_t phys, virtaddr_t, uint32_t prot, uint32_t flags);
void mmu_map_range(page_table* table, physaddr_t phys, virtaddr_t virt, size_t length, uint32_t prot, uint32_t flags);
void mmu_unmap(page_table* table, virtaddr_t virt);
void mmu_invalidate(page_table* table, virtaddr_t virt, size_t length);
vm_mapping mmu_get_phys(page_table* table, virtaddr_t virt);

page_table* mmu_new_pgdir();
void mmu_destroy(page_table* root);

/*
 * PROT_USER - clone entire root directory
 * ~PROT_USER - clone higher half
 */
void mmu_clone(page_table* source, page_table* dest, uint32_t prot); 
