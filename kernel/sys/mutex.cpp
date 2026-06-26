#include <sys/mutex.hpp>
#include <sys/spinlock.hpp>
#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/cpu.hpp>
#include <types.hpp>

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
		sched_block(thread_status::sleeping);
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

	if(mutex.waitqueue)
	{
		auto* thr = mutex.waitqueue;
		mutex.waitqueue = mutex.waitqueue->next;
		thr->next = nullptr;

		sched_unblock(thr);
	}

	mutex.locked = false;
	spinlock_release_irqsave(mutex.spinlock, rflags);
}
