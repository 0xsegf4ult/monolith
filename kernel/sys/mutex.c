#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <sys/smp.h>
#include <sched/task.h>
#include <sched/scheduler.h>
#include <types.h>
#include <panic.h>

#include <stdatomic.h>

void mutex_init(mutex_t* mutex)
{
	spinlock_init(&mutex->spinlock);
	mutex->waitqueue = nullptr;
	mutex->locked = false;
};

void mutex_lock(mutex_t* mutex)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&mutex->spinlock, &flags);

	if(mutex->locked)
	{
		struct task* task = smp_current_task();
		task->next = mutex->waitqueue;
		mutex->waitqueue = task;

		spinlock_release_irqsave(&mutex->spinlock, flags);

		task_status exp_state = TASK_RUNNING;
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_SLEEPING))
		{
			if(exp_state != TASK_SLEEPING)
				panic("mutex lock in invalid state %s", get_status_name(exp_state));

		}
		
		sched_yield();
	}
	else
	{
		mutex->locked = true;
		spinlock_release_irqsave(&mutex->spinlock, flags);
	}
}

void mutex_unlock(mutex_t* mutex)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&mutex->spinlock, &flags);

	mutex->locked = false;
retry_awake:
	if(mutex->waitqueue)
	{
		struct task* task = mutex->waitqueue;
		mutex->waitqueue = task->next;
		task->next = nullptr;

		task_status exp_state = TASK_SLEEPING;
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING))
		{
			/* task died while waiting on mutex, try awaking someone else */
			if(exp_state == TASK_ZOMBIE)
				goto retry_awake;

			panic("mutex_unlock: task in waitqueue not sleeping %s", get_status_name(exp_state));
		}

		sched_add_ready(task);
	}

	spinlock_release_irqsave(&mutex->spinlock, flags);
}
