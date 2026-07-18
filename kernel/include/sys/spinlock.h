#pragma once

#include <cpu.h>
#include <irq.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct
{
	_Atomic uint16_t next_ticket;
	_Atomic uint16_t now_serving;
} spinlock_t;

static inline void spinlock_init(spinlock_t* spin)
{
	atomic_store(&spin->next_ticket, 0);
	atomic_store(&spin->now_serving, 0);
}

static inline void spinlock_acquire(spinlock_t* spin)
{
	uint16_t ticket = atomic_fetch_add_explicit(&spin->next_ticket, 1, memory_order_relaxed);
	while(atomic_load_explicit(&spin->now_serving, memory_order_relaxed) != ticket)
	{
		native_cpu_relax();
	}
	atomic_thread_fence(memory_order_acquire);
}

static inline void spinlock_release(spinlock_t* spin)
{
	atomic_fetch_add_explicit(&spin->now_serving, 1, memory_order_release);
}

static inline void spinlock_acquire_irqsave(spinlock_t* spin, uint64_t* flags)
{
	uint64_t old_flags = local_irq_save();
	spinlock_acquire(spin);
	*flags = old_flags;
}

static inline void spinlock_release_irqsave(spinlock_t* spin, uint64_t flags)
{
	spinlock_release(spin);
	local_irq_restore(flags);
}
