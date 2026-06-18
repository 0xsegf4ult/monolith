#pragma once

#include <lib/types.hpp>

enum vm_flags : uint64_t
{
	vm_none = 0,
	vm_present = 1,
	vm_write = 2,
	vm_exec = 4,
	vm_user = 8,
	vm_mmio = 16,
	vm_swapped = 32
};

enum pf_flags : uint64_t
{
	pf_present = 1,
	pf_write = 2,
	pf_user = 4,
	pf_fetch = 8
};

constexpr physaddr_t vm_allocate = 0x20000000;

struct vm_object
{
	virtaddr_t base;
	size_t length;
	vm_flags flags;
	vm_object* next;
};
