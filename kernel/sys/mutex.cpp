#include <sys/mutex.hpp>
#include <sys/spinlock.hpp>
#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/cpu.hpp>
#include <lib/types.hpp>

void mutex_init(mutex_t& mutex)
{
	spinlock_init(mutex.spinlock);
	mutex.waitqueue = nullptr;
	mutex.locked = false;
};

void mutex_lock(mutex_t& mutex)
{
	spinlock_acquire(mutex.spinlock);

	if(mutex.locked)
	{
		auto* thr = smp_current_cpu()->get_current_thread();
		thr->next = mutex.waitqueue;
		mutex.waitqueue = thr;

		spinlock_release(mutex.spinlock);
		sched_block(thread_status::sleeping);
	}
	else
	{
		mutex.locked = true;
		spinlock_release(mutex.spinlock);
	}
}

void mutex_unlock(mutex_t& mutex)
{
	spinlock_acquire(mutex.spinlock);

	if(mutex.waitqueue)
	{
		auto* thr = mutex.waitqueue;
		mutex.waitqueue = mutex.waitqueue->next;
		thr->next = nullptr;

		sched_unblock(thr);
	}

	mutex.locked = false;
	spinlock_release(mutex.spinlock);
}
