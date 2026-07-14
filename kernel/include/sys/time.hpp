#pragma once

#include <types.hpp>

using clockid_t = int;

enum clock_ids : clockid_t
{
	CLOCK_REALTIME = 1
};

struct timespec
{
	time_t tv_sec;
	uint32_t tv_nsec;
};

void time_set_boottime(time_t time);
void time_uptime_add(uint64_t nsec);

time_t time_get_boottime();
timespec time_get_uptime();
timespec time_get_current();
