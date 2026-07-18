#pragma once

#include <stdint.h>

struct task;

void sched_init(uint32_t cpu_count);
void sched_start_cpu();

void sched_add_ready(struct task* task);
void sched_yield();
void schedule();
