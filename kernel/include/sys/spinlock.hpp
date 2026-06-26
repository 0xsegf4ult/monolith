#pragma once

#include <types.hpp>
#include <stdatomic.h>

extern "C" uint64_t disable_interrupts();
extern "C" void restore_flags(uint64_t rflags);

using atomic_uint16_t = _Atomic(uint16_t);

struct spinlock_t
{
	atomic_uint16_t next_ticket;
	atomic_uint16_t now_serving;
};

inline void spinlock_init(spinlock_t& spin)
{
	spin.next_ticket = 0;
	spin.now_serving = 0;
}

inline void spinlock_acquire(spinlock_t& spin)
{
	uint16_t ticket = atomic_fetch_add_explicit(&spin.next_ticket, 1, memory_order_relaxed);
	while(atomic_load_explicit(&spin.now_serving, memory_order_relaxed) != ticket)
	{
		asm volatile("pause");
	}
	atomic_thread_fence(memory_order_acquire);	
}

inline void spinlock_release(spinlock_t& spin)
{
	atomic_fetch_add_explicit(&spin.now_serving, 1, memory_order_release);
}

inline void spinlock_acquire_irqsave(spinlock_t& spin, uint64_t& rflags)
{
	uint64_t old_rflags = disable_interrupts();
	spinlock_acquire(spin);
	rflags = old_rflags;
}

inline void spinlock_release_irqsave(spinlock_t& spin, uint64_t& rflags)
{
	uint64_t old_rflags = rflags;
	spinlock_release(spin);
	restore_flags(old_rflags);
}
