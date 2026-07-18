#include <arch/x86_64/mmu.h>

#include <mm/mmu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#include <libk/string.h>
#include <types.h>

static struct page_table* get_pte(struct page_table* table, uint64_t entry)
{
	if(table[entry].raw & PTE_PRESENT && pte_address(table[entry]))
		return (struct page_table*)(pte_address(table[entry]) + VM_DMAP_BASE);

	return nullptr;
}

static struct page_table* create_pte(struct page_table* table, uint64_t entry, uint64_t flags)
{
	physaddr_t pt_phys = pmm_allocate();

	struct page_table* pt = (struct page_table*)(pt_phys + VM_DMAP_BASE);
	memset(pt, 0, sizeof(uint64_t) * 512);

	table[entry].raw = (pt_phys & page_mask_4K) | flags | PTE_PRESENT;
	return pt;
}

static struct page_table* get_or_create_pte(struct page_table* table, uint64_t entry, uint64_t flags)
{
	struct page_table* pte = get_pte(table, entry);
	if(pte)
		return pte;

	return create_pte(table, entry, flags);
}

constexpr static uint64_t secflags = PTE_WRITABLE | PTE_USER;

static uint64_t convert_vm_params(uint32_t prot, uint32_t flags)
{
	uint64_t pte_flags = 0;

	if(prot & PROT_READ)
		pte_flags |= PTE_PRESENT;
	if(prot & PROT_WRITE)
		pte_flags |= PTE_WRITABLE;
	if(!(prot & PROT_EXEC))
		pte_flags |= PTE_NOEXEC;
	if(prot & PROT_USER)
		pte_flags |= PTE_USER;
	if(prot & PROT_UNCACHED)
		pte_flags |= PTE_CACHE_DISABLE;
	if(prot & PROT_WRITECOMBINE)
		pte_flags |= PTE_WRITECOMBINING;

	if(flags & VM_FLAG_OWNER)
		pte_flags |= PTE_OWNER;

	return pte_flags;
}

void mmu_map(struct page_table* table, physaddr_t phys, virtaddr_t virt, uint32_t prot, uint32_t flags)
{
	size_t pml4_index = get_pagetable_index(virt, 4);
	size_t pdpt_index = get_pagetable_index(virt, 3);
	size_t pd_index = get_pagetable_index(virt, 2);
	size_t pt_index = get_pagetable_index(virt, 1);

	struct page_table* pdpt = get_or_create_pte(table, pml4_index, secflags);
	struct page_table* pd = get_or_create_pte(pdpt, pdpt_index, secflags);
	struct page_table* pt = get_or_create_pte(pd, pd_index, secflags);
	pt[pt_index].raw = phys & page_mask_4K | convert_vm_params(prot, flags);
}

void mmu_map_range(struct page_table* table, physaddr_t phys, virtaddr_t virt, size_t length, uint32_t prot, uint32_t flags)
{
	while(length > 0)
	{
		mmu_map(table, phys, virt, prot, flags);

		phys += CONFIG_PAGE_SIZE;
		virt += CONFIG_PAGE_SIZE;

		if(length <= CONFIG_PAGE_SIZE)
			break;

		length -= CONFIG_PAGE_SIZE;
	}
}

void mmu_unmap(struct page_table* table, virtaddr_t virt)
{
	size_t pml4_index = get_pagetable_index(virt, 4);
	size_t pdpt_index = get_pagetable_index(virt, 3);
	size_t pd_index = get_pagetable_index(virt, 2);
	size_t pt_index = get_pagetable_index(virt, 1);

	struct page_table* pdpt = get_pte(table, pml4_index);
	struct page_table* pd = get_pte(pdpt, pdpt_index);
	struct page_table* pt = get_pte(pd, pd_index);

	pt[pt_index].raw = 0;
}

void mmu_invalidate(struct page_table* table, virtaddr_t virt, size_t length)
{
	while(length)
	{
		asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
		virt += CONFIG_PAGE_SIZE;
		length -= CONFIG_PAGE_SIZE;
	}
}

vm_mapping mmu_get_phys(struct page_table* table, virtaddr_t virt)
{
	size_t pml4_index = get_pagetable_index(virt, 4);
	size_t pdpt_index = get_pagetable_index(virt, 3);
	size_t pd_index = get_pagetable_index(virt, 2);
	size_t pt_index = get_pagetable_index(virt, 1);

	vm_mapping mapping;
	mapping.base = 0;
	mapping.prot = 0;
	mapping.flags = 0;
	mapping.present = false;

	struct page_table* pdpt = get_pte(table, pml4_index);
	if(!pdpt)
		return mapping;

	struct page_table* pd = get_pte(pdpt, pdpt_index);
	if(!pd)
		return mapping;

	struct page_table* pt = get_pte(pd, pd_index);
	if(!pt)
		return mapping;

	uint64_t entry = pt[pt_index].raw;
	mapping.base = pte_address(pt[pt_index]);

	if(entry & PTE_PRESENT)
	{
		mapping.prot |= PROT_READ;
		mapping.present = true;
	}
		
	if(entry & PTE_WRITABLE)
		mapping.prot |= PROT_WRITE;
	if(!(entry & PTE_NOEXEC))
		mapping.prot |= PROT_EXEC;
	if(entry & PTE_USER)
		mapping.prot |= PROT_USER;
	if(entry & PTE_CACHE_DISABLE)
		mapping.prot |= PROT_UNCACHED;
	if(entry & PTE_WRITECOMBINING)
		mapping.prot |= PROT_WRITECOMBINE;

	if(entry & PTE_OWNER)
		mapping.flags |= VM_FLAG_OWNER;
	
	return mapping;
}

struct page_table* mmu_new_pgdir()
{
	struct page_table* pgdir = (struct page_table*)(pmm_allocate() + VM_DMAP_BASE);
	memset(pgdir, 0, CONFIG_PAGE_SIZE);
	return pgdir;
}

void mmu_destroy(struct page_table* root)
{
	// higher half is cloned into all spaces
	for(int i = 0; i < 256; i++)
	{
		struct page_table* pml_entry = &root[i];
		if(!(pml_entry->raw & PTE_PRESENT))
			continue;
		
		struct page_table* pdpt = (struct page_table*)(pte_address(*pml_entry) + VM_DMAP_BASE);

		for(int j = 0; j < 512; j++)
		{
			struct page_table* pdpt_entry = &pdpt[j];
			if(!(pdpt_entry->raw & PTE_PRESENT))
				continue;

			struct page_table* pd = (struct page_table*)(pte_address(*pdpt_entry) + VM_DMAP_BASE);
			for(int k = 0; k < 512; k++)
			{
				struct page_table* pd_entry = &pd[k];
				if(!(pd_entry->raw & PTE_PRESENT))
					continue;

				pmm_free(pte_address(*pd_entry));
				pd_entry->raw = 0;
			}

			pmm_free(pte_address(*pdpt_entry));
			pdpt_entry->raw = 0;	
		}

		pmm_free(pte_address(*pml_entry));
		pml_entry->raw = 0;
	}

	pmm_free((physaddr_t)(root) - VM_DMAP_BASE);
}

void mmu_clone(struct page_table* source, struct page_table* dest, uint32_t prot)
{
	int start = (prot & PROT_USER) ? 0 : 256;
	for(int i = start; i < 512; i++)
		dest[i] = source[i];
}
