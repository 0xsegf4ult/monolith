#pragma once

#include <mm/vm_object.hpp>
#include <lib/types.hpp>

enum PTEBits : uint64_t
{
	PTE_PRESENT = (1 << 0),
	PTE_WRITABLE = (1 << 1),
	PTE_USER = (1 << 2),
	PTE_WRITETHROUGH = (1 << 3),
	PTE_CACHE_DISABLE = (1 << 4),
	PTE_ACCESSED = (1 << 5),
	PTE_DIRTY = (1 << 6),
	PTE_HUGE = (1 << 7),
	PTE_PAT = (1 << 7),
	PTE_GLOBAL = (1 << 8),
	PTE_NOEXEC = 0x8000000000000000
};

constexpr uint64_t page_mask_4K = 0xfff0fffffffff000;
constexpr uint64_t page_mask_2M = 0xfff0ffffffe00000;
constexpr uint64_t page_mask_1G = 0xfff0ffffe0000000;

constexpr uint64_t page_size_4K = 1 << 12;
constexpr uint64_t page_size_2M = 1 << 21;
constexpr uint64_t page_size_1G = 1 << 30;

constexpr uint64_t get_pagetable_index(uint64_t address, int8_t level = 1)
{
	int8_t shift = 12 + ((level - 1) * 9);
	return (address & (0x1ffzu << shift)) >> shift;
}

constexpr uint32_t addr_to_pagenum(uint64_t address)
{
	return address >> 12;
};

constexpr virtaddr_t pagenum_to_addr(uint32_t pagenum)
{
	return pagenum << 12;
}

class page_table
{
public:
	constexpr page_table() : raw{0} {}
	constexpr page_table(uint64_t value) : raw{value} {}
	constexpr uint64_t get_raw() const
	{
		return raw;
	}

	constexpr uint64_t get() const
	{
		return raw & 0x000ffffffffff000;
	}

	constexpr void set_flags(uint64_t flags)
	{
		raw |= flags;
	}

	constexpr void unset_flags(uint64_t flags)
	{
		raw &= ~flags;
	}
private:
	uint64_t raw;
};

constexpr PTEBits vm_flags_to_x86(uint64_t flags)
{
	uint64_t result{0};

	if(flags & vm_write)
		result |= PTE_WRITABLE;
	if(!(flags & vm_exec))
		result |= PTE_NOEXEC;
	if(flags & vm_user)
		result |= PTE_USER;
	if(flags & vm_mmio)
		result |= PTE_CACHE_DISABLE;

	return PTEBits{result};
}

page_table* get_pte(page_table* table, uint64_t entry);
page_table* create_pte(page_table* table, uint64_t entry, uint64_t flags = 0);
page_table* get_or_create_pte(page_table* table, uint64_t entry, uint64_t flags = 0);
void mmu_map(page_table* table, physaddr_t phys, virtaddr_t virt, uint64_t flags = 0);
void mmu_map_MB(page_table* table, physaddr_t phys, virtaddr_t virt, uint64_t flags = 0);
void mmu_map_GB(page_table* table, physaddr_t phys, virtaddr_t virt, uint64_t flags = 0);
void mmu_map_range(page_table* table, physaddr_t phys, virtaddr_t virt, size_t length, uint64_t flags = 0, bool allow_huge = false);
