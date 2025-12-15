#pragma once

#include <lib/types.hpp>

enum vm_flags : uint64_t
{
	vm_none = 0,
	vm_write = 1,
	vm_exec = 2,
	vm_user = 4,
	vm_mmio = 8
};

struct vm_object
{
	virtaddr_t base;
	size_t length;
	vm_flags flags;
	vm_object* next;
};
