#pragma once

#include <sys/spinlock.h>
#include <types.h>
#include <stdatomic.h>

typedef struct reflock
{
	union
	{
		_Atomic uint64_t lock_count;
		struct
		{
			spinlock_t lock;
			_Atomic uint32_t count;
		};
	};
} reflock_t;

static inline void reflock_init(reflock_t* ref)
{
	atomic_store(&ref->count, 0);
	spinlock_init(&ref->lock);
}

static inline void reflock_acquire(reflock_t* ref)
{
	atomic_fetch_add_explicit(&ref->count, 1, memory_order_relaxed);
}

static inline bool reflock_release_or_lock(reflock_t* ref)
{
	uint32_t count = atomic_fetch_sub_explicit(&ref->count, 1, memory_order_release);
	if(count > 1)
		return false;

	atomic_thread_fence(memory_order_acquire);
	spinlock_acquire(&ref->lock);
	return true;
}
