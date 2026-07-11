#pragma once

#include <types.hpp>

constexpr size_t arch_page_size = 0x1000;

constexpr uint16_t native_be16_to_arch(uint16_t data)
{
	return __builtin_bswap16(data);
}

constexpr uint16_t native_arch_to_be16(uint16_t data)
{
	return __builtin_bswap16(data);
}

constexpr uint32_t native_be32_to_arch(uint32_t data)
{
	return __builtin_bswap32(data);
}

constexpr uint32_t native_arch_to_be32(uint32_t data)
{
	return __builtin_bswap32(data);
}
