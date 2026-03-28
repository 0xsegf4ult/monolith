#pragma once

#include <lib/types.hpp>

constexpr size_t arch_max_cpus = 64;
struct cpu_t;

void smp_start_bsp();
void smp_init();

void smp_discover_cpu(uint32_t lapic_id);

cpu_t* smp_get_cpu(uint32_t id);
cpu_t* smp_current_cpu();
