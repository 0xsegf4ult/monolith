#pragma once

#include <stdint.h>

static inline void local_irq_disable()
{
	asm volatile("cli" ::: "memory");
}

static inline void local_irq_enable()
{
	asm volatile("sti" ::: "memory");
}

static inline uint64_t local_irq_save()
{
	uint64_t flags;
	asm volatile("pushfq; popq %0" : "=rm"(flags) :: "memory");
	local_irq_disable();
	return flags;
}

static inline void local_irq_restore(uint64_t flags)
{
	asm volatile("pushq %0; popfq" :: "g"(flags) : "memory", "cc");
}
