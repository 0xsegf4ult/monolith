#include <sys/mutex.hpp>
#include <sys/spinlock.hpp>
#include <sys/task.hpp>
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
		auto* task = smp_current_cpu()->get_current_task();
		task->next = mutex.waitqueue;
		mutex.waitqueue = task;

		spinlock_release_irqsave(mutex.spinlock, rflags);

		task_status exp_state = TASK_RUNNING;
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_SLEEPING))
		{
			if(exp_state != TASK_SLEEPING)
				panic("mutex_lock in invalid task state");
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
		auto* task = mutex.waitqueue;
		mutex.waitqueue = mutex.waitqueue->next;
		task->next = nullptr;

		task_status exp_state = TASK_SLEEPING;
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING))
		{
			// task died while waiting for mutex, try awaking someone else
			if(exp_state == TASK_ZOMBIE)
				goto retry_awake;

			panic("mutex_unlock task in waitqueue not sleeping {}", get_status_name(exp_state));
		}

		sched_add_ready(task);
	}

	spinlock_release_irqsave(mutex.spinlock, rflags);
}
