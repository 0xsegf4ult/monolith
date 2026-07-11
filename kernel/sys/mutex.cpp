#include <sys/mutex.hpp>
#include <sys/spinlock.hpp>
#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/cpu.hpp>
#include <types.hpp>
#include <panic.hpp>

void mutex_init(mutex_t& mutex)
{
	spinlock_init(mutex.spinlock);
	mutex.waitqueue = nullptr;
	mutex.locked = false;
};

void mutex_lock(mutex_t& mutex)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(mutex.spinlock, rflags);

	if(mutex.locked)
	{
		auto* thr = smp_current_cpu()->get_current_thread();
		thr->next = mutex.waitqueue;
		mutex.waitqueue = thr;

		spinlock_release_irqsave(mutex.spinlock, rflags);

		thread_status exp_state = THREAD_RUNNING;
		if(!atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_SLEEPING))
		{
			if(exp_state != THREAD_SLEEPING)
				panic("mutex_lock in invalid thread state");
		}

		sched_yield();
	}
	else
	{
		mutex.locked = true;
		spinlock_release_irqsave(mutex.spinlock, rflags);
	}
}

void mutex_unlock(mutex_t& mutex)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(mutex.spinlock, rflags);

	mutex.locked = false;
retry_awake:
	if(mutex.waitqueue)
	{
		auto* thr = mutex.waitqueue;
		mutex.waitqueue = mutex.waitqueue->next;
		thr->next = nullptr;

		thread_status exp_state = THREAD_SLEEPING;
		if(!atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_RUNNING))
		{
			// thread died while waiting for mutex, try awaking someone else
			if(exp_state == THREAD_ZOMBIE)
				goto retry_awake;

			panic("mutex_unlock thread in waitqueue not sleeping {}", get_status_name(exp_state));
		}

		sched_unblock(thr);
	}

	spinlock_release_irqsave(mutex.spinlock, rflags);
}
