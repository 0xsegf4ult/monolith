#pragma once

#include <lib/types.hpp>

namespace mm
{

constexpr virtaddr_t kernel_mapping_offset = 0xffffffff80000000;
constexpr virtaddr_t direct_mapping_offset = 0xffff800000000000;
constexpr size_t arch_page_size = 0x1000;

constexpr uint64_t page_align(uint64_t address)
{
	return (address + arch_page_size - 1) & (~(arch_page_size - 1));
}

constexpr uint64_t page_align_down(uint64_t address)
{
	return address & ~(arch_page_size - 1);
}

}
