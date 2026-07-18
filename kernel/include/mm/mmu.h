#pragma once

#include <config.h>
#include <types.h>

struct page_table;

typedef struct 
{
	physaddr_t base;
	uint32_t prot;
	uint32_t flags;
	bool present;
} vm_mapping;

void mmu_map(struct page_table* table, physaddr_t phys, virtaddr_t, uint32_t prot, uint32_t flags);
void mmu_map_range(struct page_table* table, physaddr_t phys, virtaddr_t virt, size_t length, uint32_t prot, uint32_t flags);
void mmu_unmap(struct page_table* table, virtaddr_t virt);
void mmu_invalidate(struct page_table* table, virtaddr_t virt, size_t length);
vm_mapping mmu_get_phys(struct page_table* table, virtaddr_t virt);

struct page_table* mmu_new_pgdir();
void mmu_destroy(struct page_table* root);

/*
 * PROT_USER - clone entire root directory
 * ~PROT_USER - clone higher half
 */
void mmu_clone(struct page_table* source, struct page_table* dest, uint32_t prot);
