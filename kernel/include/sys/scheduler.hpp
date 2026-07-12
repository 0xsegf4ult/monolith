#pragma once

#include <stdint.h>

struct task_t;

void sched_init(uint32_t cpu_count);
void sched_start_bsp();
void sched_start_ap();
void sched_add_ready(task_t* task);
void sched_yield();
void schedule();
