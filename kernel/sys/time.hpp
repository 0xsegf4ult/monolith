#pragma once

#include <lib/types.hpp>

void time_set_boottime(time_t time);
void time_uptime_increment();

time_t time_get_boottime();
time_t time_get_uptime();
time_t time_get_current();
