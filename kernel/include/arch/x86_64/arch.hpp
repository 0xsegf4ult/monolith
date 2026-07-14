#pragma once

#include <types.hpp>

constexpr uint16_t be16_to_native(uint16_t data)
{
	return __builtin_bswap16(data);
}

constexpr uint16_t native_to_be16(uint16_t data)
{
	return __builtin_bswap16(data);
}

constexpr uint32_t be32_to_native(uint32_t data)
{
	return __builtin_bswap32(data);
}

constexpr uint32_t native_to_be32(uint32_t data)
{
	return __builtin_bswap32(data);
}

inline void arch_enable_interrupts()
{
	asm volatile("sti");
}

inline void arch_pause()
{
	asm volatile("pause");
}

inline void arch_halt()
{
	while(1)
		asm volatile("cli; hlt");
}

inline void arch_idle()
{
	asm volatile("hlt");
}
