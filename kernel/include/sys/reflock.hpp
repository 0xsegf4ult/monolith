#pragma once

#include <sys/spinlock.hpp>
#include <types.hpp>
#include <stdatomic.h>

struct reflock_t
{
	union
	{
		atomic_ullong lock_count;
		struct
		{
			spinlock_t lock;
			atomic_uint count;
		};
	};
};

inline void reflock_init(reflock_t& ref)
{
	ref.count = 0;
	spinlock_init(ref.lock);
}

inline void reflock_acquire(reflock_t& ref)
{
	atomic_fetch_add_explicit(&ref.count, 1, memory_order_relaxed);
}

inline bool reflock_release_or_lock(reflock_t& ref)
{
	uint32_t count = atomic_fetch_sub_explicit(&ref.count, 1, memory_order_release);
	if(count > 1)
		return false;

	atomic_thread_fence(memory_order_acquire);
	spinlock_acquire(ref.lock);
	return true;
}
