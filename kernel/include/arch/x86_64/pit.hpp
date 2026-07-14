#pragma once

#include <stdint.h>
namespace timer
{
	uint64_t get_ticks();
}

void pit_init();
void pit_sleep(uint32_t ms);
