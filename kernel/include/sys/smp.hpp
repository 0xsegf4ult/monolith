#pragma once

#include <types.hpp>

struct task_t;
class page_table;

void smp_start_bsp();
void smp_init();

uint32_t smp_current_cpu();
task_t* smp_current_task();
void smp_set_current_pagetable(page_table* table);

void smp_stop_cpus();

