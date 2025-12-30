#pragma once

#include <lib/types.hpp>

struct dev_t
{
public:
	constexpr dev_t(uint32_t raw) : _raw{raw} {}
	constexpr dev_t(uint32_t major, uint16_t minor) : _raw{major << 20 | (minor & 0xFFF)} {}

	constexpr uint32_t major() const
	{
		return _raw >> 12;
	}

	constexpr uint16_t minor() const
	{
		return _raw & 0xFFF;
	}

	constexpr bool operator==(const dev_t& other) const
	{
		return _raw == other._raw;
	}
private:
	uint32_t _raw;
};
