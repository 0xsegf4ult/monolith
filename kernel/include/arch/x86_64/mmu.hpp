#pragma once

#include <types.hpp>

enum PTEBits : uint64_t
{
	PTE_PRESENT = (1 << 0),
	PTE_WRITABLE = (1 << 1),
	PTE_USER = (1 << 2),
	PTE_WRITETHROUGH = (1 << 3),
	PTE_WRITECOMBINING = (1 << 3), // PWT maps to PAT1, which we set to WC
	PTE_CACHE_DISABLE = (1 << 4),
	PTE_ACCESSED = (1 << 5),
	PTE_DIRTY = (1 << 6),
	PTE_HUGE = (1 << 7),
	PTE_PAT = (1 << 7),
	PTE_GLOBAL = (1 << 8),
	PTE_OWNER = (1 << 9), // maps to VM_FLAG_OWNER
	PTE_NOEXEC = 0x8000000000000000
};

enum PFBits : uint64_t
{
	PF_PRESENT = 0x1,
	PF_WRITE = 0x2,
	PF_USER = 0x4,
	PF_RESERVED_WRITE = 0x8,
	PF_FETCH = 0x10
};

constexpr uint64_t page_mask_4K = 0xfff0fffffffff000;

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

page_table* get_pte(page_table* table, uint64_t entry);
page_table* create_pte(page_table* table, uint64_t entry, uint32_t prot, uint32_t flags);
page_table* get_or_create_pte(page_table* table, uint64_t entry, uint32_t prot, uint32_t flags);
