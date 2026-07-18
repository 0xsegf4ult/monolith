#pragma once

struct cpu;
struct task;

void smp_start_bsp();
void smp_init();
void smp_stop_cpus();

struct cpu* smp_get_cpu(uint32_t id);
uint32_t smp_current_cpu();
struct task* smp_current_task();


