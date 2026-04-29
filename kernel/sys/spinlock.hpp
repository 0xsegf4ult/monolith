#pragma once

#include <lib/types.hpp>
#include <stdatomic.h>

extern "C" uint64_t disable_interrupts();
extern "C" void restore_flags(uint64_t rflags);

struct spinlock_t
{
	atomic_flag internal;
	uint64_t rflags;
};

inline void spinlock_init(spinlock_t& spin)
{
	spin.internal = ATOMIC_FLAG_INIT;
	spin.rflags = 0;
}

inline void spinlock_acquire(spinlock_t& spin)
{
	uint64_t old_rflags = disable_interrupts();

	while(atomic_flag_test_and_set(&spin.internal))
	{
		asm volatile("pause");
	}

	spin.rflags = old_rflags;
}

inline void spinlock_release(spinlock_t& spin)
{
	uint64_t old_rflags = spin.rflags;
	atomic_flag_clear(&spin.internal);
	restore_flags(old_rflags);
}
