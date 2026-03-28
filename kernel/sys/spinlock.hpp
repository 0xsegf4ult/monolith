#pragma once

#include <lib/types.hpp>
#include <stdatomic.h>

struct spinlock_t
{
	atomic_flag internal;
};

inline void spinlock_init(spinlock_t& spin)
{
	spin.internal = ATOMIC_FLAG_INIT;
}

inline void spinlock_acquire(spinlock_t& spin)
{
	while(atomic_flag_test_and_set(&spin.internal))
	{
		asm volatile("pause");
	}
}

inline void spinlock_release(spinlock_t& spin)
{
	atomic_flag_clear(&spin.internal);
}
