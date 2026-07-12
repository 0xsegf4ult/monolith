#include <sys/signal.hpp>
#include <sys/task.hpp>
#include <sys/spinlock.hpp>
#include <sys/scheduler.hpp>
#include <sys/err.hpp>
#include <types.hpp>
#include <klog.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

bool signal_pending(task_t* task)
{
	return (task->sig_pending & (~task->sig_blocked)) > 0;
}

int send_signal(task_t* task, uint32_t signal)
{
	auto idx = signal - 1;
	
	uint64_t rflags;
	spinlock_acquire_irqsave(task->sig_lock, rflags);
	task->sig_pending |= (1 << idx);
	spinlock_release_irqsave(task->sig_lock, rflags);

	task_status expected = TASK_INTR_SLEEPING;
	if(atomic_compare_exchange_strong(&task->status, &expected, TASK_RUNNING))
	{
		sched_add_ready(task);
	}

	return 0;
}

int pid_send_signal(pid_t pid, uint32_t signal)
{
	if(signal < 1 || signal >= NSIG)
		return -EINVAL;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_task_list_lock, rflags);

	task_t* cur;
	list_for_each_entry(cur, g_task_list, list_node)
	{
		if(cur->pid == pid)
		{
			spinlock_release_irqsave(g_task_list_lock, rflags);
			send_signal(cur, signal);
			return 0;
		}
	}
	spinlock_release_irqsave(g_task_list_lock, rflags);
	return -ESRCH;
}

int pgrp_send_signal(pid_t pgrp, uint32_t signal)
{
	int result = -ESRCH;

	if(signal < 1 || signal >= NSIG)
		return -EINVAL;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_task_list_lock, rflags);

	task_t* cur;
	list_for_each_entry(cur, g_task_list, list_node)
	{
		if(cur->pgid == pgrp)
		{
			send_signal(cur, signal);
			result = 0;
		}
	}

	spinlock_release_irqsave(g_task_list_lock, rflags);
	return result;
}
