#include <arch/x86_64/mmu.hpp>

#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/vm_object.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>

page_table* get_pte(page_table* table, uint64_t entry)
{
	if(table[entry].get_raw() & PTE_PRESENT && table[entry].get())
		return reinterpret_cast<page_table*>(table[entry].get() + mm::direct_mapping_offset);

	return nullptr;
}

page_table* create_pte(page_table* table, uint64_t entry, uint64_t flags)
{
	auto pt_phys = pmm_allocate();

	page_table* pt = reinterpret_cast<page_table*>(pt_phys + mm::direct_mapping_offset);
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

void mmu_map(page_table* table, physaddr_t phys, virtaddr_t virt, uint64_t flags)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);
	auto pt_index = get_pagetable_index(virt, 1);

	page_table* pdpt = get_or_create_pte(table, pml4_index, secflags);
	page_table* pd = get_or_create_pte(pdpt, pdpt_index, secflags);
	page_table* pt = get_or_create_pte(pd, pd_index, secflags);
	pt[pt_index] = page_table(phys & page_mask_4K);
	pt[pt_index].set_flags(flags | PTE_PRESENT);
}

void mmu_map_MB(page_table* table, physaddr_t phys, virtaddr_t virt, uint64_t flags)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);
	auto pd_index = get_pagetable_index(virt, 2);

	page_table* pdpt = get_or_create_pte(table, pml4_index, secflags);
	page_table* pd = get_or_create_pte(pdpt, pdpt_index, secflags);
	pd[pd_index] = page_table(phys & page_mask_2M);
	pd[pd_index].set_flags(flags | PTE_PRESENT | PTE_HUGE);
}

void mmu_map_GB(page_table* table, physaddr_t phys, virtaddr_t virt, uint64_t flags)
{
	auto pml4_index = get_pagetable_index(virt, 4);
	auto pdpt_index = get_pagetable_index(virt, 3);

	page_table* pdpt = get_or_create_pte(table, pml4_index, secflags);
	pdpt[pdpt_index] = page_table(phys & page_mask_1G);
	pdpt[pdpt_index].set_flags(flags | PTE_PRESENT | PTE_HUGE);
}

void mmu_map_range(page_table* table, physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags, bool allow_huge)
{
	size_t cur_pgsz = 0;
	
	while(length > 0)
	{
		if(allow_huge && length >= page_size_1G)
		{
			mmu_map_GB(table, phys, virt, flags);
			cur_pgsz = page_size_1G;
		}
		else if(allow_huge && length >= page_size_2M)
		{
			mmu_map_MB(table, phys, virt, flags);
			cur_pgsz = page_size_2M;
		}
		else
		{
			mmu_map(table, phys, virt, flags);
			cur_pgsz = page_size_4K;
		}

		phys += cur_pgsz;
		virt += cur_pgsz;

		if(length <= cur_pgsz)
			break;

		length -= cur_pgsz;
	}
}
