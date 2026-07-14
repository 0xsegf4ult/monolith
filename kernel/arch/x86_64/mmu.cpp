#include <arch/x86_64/mmu.hpp>

#include <mm/mmu.hpp>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>

#include <kstd.hpp>
#include <types.hpp>

page_table* get_pte(page_table* table, uint64_t entry)
{
	if(table[entry].get_raw() & PTE_PRESENT && table[entry].get())
		return reinterpret_cast<page_table*>(table[entry].get() + VM_DMAP_BASE);

	return nullptr;
}

page_table* create_pte(page_table* table, uint64_t entry, uint64_t flags)
{
	auto pt_phys = pmm_allocate();

	page_table* pt = reinterpret_cast<page_table*>(pt_phys + VM_DMAP_BASE);
	memset(pt, 0, sizeof(uint64_t) * 512);

	table[entry] = page_table(pt_phys & page_mask_4K);
	table[entry].set_flags(flags | PTE_PRESENT);
	return pt;
}

page_table* get_or_create_pte(page_table* table, uint64_t entry, uint64_t flags)
{
	auto* pte = get_pte(table, entry);
	if(pte)
		return pte;

	return create_pte(table, entry, flags);
}

constexpr static uint64_t secflags = PTE_WRITABLE | PTE_USER;

constexpr uint64_t convert_vm_params(uint32_t prot, uint32_t flags)
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

void mmu_map(page_table* table, physaddr_t phys, virtaddr_t virt, uint32_t prot, uint32_t flags)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);
	auto pt_index = get_pagetable_index(virt, 1);

	page_table* pdpt = get_or_create_pte(table, pml4_index, secflags);
	page_table* pd = get_or_create_pte(pdpt, pdpt_index, secflags);
	page_table* pt = get_or_create_pte(pd, pd_index, secflags);
	pt[pt_index] = page_table(phys & page_mask_4K);
	pt[pt_index].set_flags(convert_vm_params(prot, flags));
}

void mmu_map_range(page_table* table, physaddr_t phys, virtaddr_t virt, size_t length, uint32_t prot, uint32_t flags)
{
	while(length > 0)
	{
		mmu_map(table, phys, virt, prot, flags);

		phys += ARCH_PAGE_SIZE;
		virt += ARCH_PAGE_SIZE;

		if(length <= ARCH_PAGE_SIZE)
			break;

		length -= ARCH_PAGE_SIZE;
	}
}

void mmu_unmap(page_table* table, virtaddr_t virt)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);
	auto pt_index = get_pagetable_index(virt, 1);

	page_table* pdpt = get_pte(table, pml4_index);
	page_table* pd = get_pte(pdpt, pdpt_index);
	page_table* pt = get_pte(pd, pd_index);

	pt[pt_index] = 0;
}

void mmu_invalidate(page_table* table, virtaddr_t virt, size_t length)
{
	while(length)
	{
		asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
		virt += ARCH_PAGE_SIZE;
		length -= ARCH_PAGE_SIZE;
	}
}

vm_mapping mmu_get_phys(page_table* table, virtaddr_t virt)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);
	auto pt_index = get_pagetable_index(virt, 1);

	vm_mapping mapping;
	mapping.base = 0;
	mapping.prot = 0;
	mapping.flags = 0;
	mapping.present = false;

	auto* pdpt = get_pte(table, pml4_index);
	if(!pdpt)
		return mapping;

	auto* pd = get_pte(pdpt, pdpt_index);
	if(!pd)
		return mapping;

	auto* pt = get_pte(pd, pd_index);
	if(!pt)
		return mapping;

	auto entry = pt[pt_index].get_raw();
	mapping.base = pt[pt_index].get();

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

page_table* mmu_new_pgdir()
{
	page_table* pgdir = (page_table*)(pmm_allocate() + VM_DMAP_BASE);
	memset(pgdir, 0, ARCH_PAGE_SIZE);
	return pgdir;
}

void mmu_destroy(page_table* root)
{
	// higher half is cloned into all spaces
	for(int i = 0; i < 256; i++)
	{
		page_table* pml_entry = &root[i];
		if(!(pml_entry->get_raw() & PTE_PRESENT))
			continue;
		
		page_table* pdpt = (page_table*)(pml_entry->get() + VM_DMAP_BASE);

		for(int j = 0; j < 512; j++)
		{
			page_table* pdpt_entry = &pdpt[j];
			if(!(pdpt_entry->get_raw() & PTE_PRESENT))
				continue;

			page_table* pd = (page_table*)(pdpt_entry->get() + VM_DMAP_BASE);
			for(int k = 0; k < 512; k++)
			{
				page_table* pd_entry = &pd[k];
				if(!(pd_entry->get_raw() & PTE_PRESENT))
					continue;

				pmm_free(pd_entry->get());
				*pd_entry = 0;
			}

			pmm_free(pdpt_entry->get());
			*pdpt_entry = 0;	
		}

		pmm_free(pml_entry->get());
		*pml_entry = 0;
	}

	pmm_free((physaddr_t)(root) - VM_DMAP_BASE);
}

void mmu_clone(page_table* source, page_table* dest, uint32_t prot)
{
	int start = (prot & PROT_USER) ? 0 : 256;
	for(int i = start; i < 512; i++)
		dest[i] = source[i];
}
