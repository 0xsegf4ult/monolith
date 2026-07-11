#pragma once

#include <sys/thread.hpp>

struct cpu_context_t;
void schedule();

struct thread_t;

void sched_init(uint32_t cpu_count);
void sched_start_bsp();
void sched_start_ap();
void sched_add_ready(thread_t* thr);
void sched_yield();
void sched_unblock(thread_t* thr);

void sched_dump_state();
