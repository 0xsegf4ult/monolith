#pragma once

#include <lib/types.hpp>

namespace timer
{

uint64_t get_ticks();

}

namespace pit
{

void init(uint16_t freq);

}
