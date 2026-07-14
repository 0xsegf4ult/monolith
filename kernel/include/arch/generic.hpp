#pragma once

#include <config.hpp>
#include <types.hpp>
#include ARCH_INCLUDE_PATH

struct cpu_context_t;
struct task_t;

extern "C" void arch_context_switch(task_t* prev, task_t* next);
extern "C" void arch_switch_to_usermode(virtaddr_t rsp, virtaddr_t rip);

enum trace_flags : uint32_t
{
	TRACE_PANIC = 1
};

void dump_registers(cpu_context_t* ctx, uint32_t flags);
void stacktrace(uint64_t frame, uint32_t flags);

extern "C" uint64_t arch_disable_interrupts();
extern "C" void arch_restore_flags(uint64_t flags);
