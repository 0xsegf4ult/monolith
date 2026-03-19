#pragma once

#include <sys/process.hpp>

struct cpu_context_t;
void schedule();

struct process_t;

void sched_init();
[[noreturn]] void sched_start();
void sched_add_ready(process_t* proc);
void sched_block(process_status status);
void sched_unblock(process_t* proc);
void sched_kill();

void sched_dump_state();
