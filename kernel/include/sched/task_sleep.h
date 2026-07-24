#pragma once

#include <types.h>

int sys_clock_nanosleep(clockid_t clock, int flags, const struct timespec* tv, struct timespec* rem);

void sleep_queue_tick(uint64_t ns);
void sleep_queue_init(uint32_t num_cpus);
