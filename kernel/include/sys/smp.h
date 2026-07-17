#pragma once

struct cpu_t;
struct task_t;

void smp_start_bsp();
void smp_init();

cpu_t* smp_get_cpu(uint32_t id);
uint32_t smp_current_cpu();
task_t smp_current_task();
