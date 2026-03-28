#pragma once

#include <lib/types.hpp>

namespace timer
{

uint64_t get_ticks();

}

namespace pit
{

constexpr uint8_t isa_irq = 0x0;
void set_gsi(uint8_t gsi);
void init(uint16_t freq);

}
