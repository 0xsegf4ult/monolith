#pragma once

#include <types.hpp>

constexpr size_t arch_page_size = 0x1000;

constexpr uint16_t native_be_to_arch(uint16_t data)
{
	return __builtin_bswap16(data);
}

constexpr uint16_t native_arch_to_be(uint16_t data)
{
	return __builtin_bswap16(data);
}
