#include <sched/signal.h>
#include <sched/scheduler.h>
#include <sched/task.h>
#include <sched/task_sys.h>
#include <sys/spinlock.h>
#include <sys/smp.h>
#include <errno.h>
#include <types.h>
#include <stdatomic.h>

bool signal_pending(struct task* task)
{
	return (task->sig_pending & (~task->sig_blocked)) > 0;
}

int send_signal(struct task* task, uint32_t signal)
{
	uint32_t idx = signal - 1;
	
	uint64_t flags;
	spinlock_acquire_irqsave(&task->sig_lock, &flags);
	task->sig_pending |= (1 << idx);
	spinlock_release_irqsave(&task->sig_lock, flags);

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

	uint64_t flags;
	spinlock_acquire_irqsave(&g_task_list_lock, &flags);

	struct task* cur;
	list_for_each_entry(cur, &g_task_list, list_node)
	{
		if(cur->pid == pid)
		{
			spinlock_release_irqsave(&g_task_list_lock, flags);
			send_signal(cur, signal);
			return 0;
		}
	}
	spinlock_release_irqsave(&g_task_list_lock, flags);
	return -ESRCH;
}

int pgrp_send_signal(pid_t pgrp, uint32_t signal)
{
	int result = -ESRCH;

	if(signal < 1 || signal >= NSIG)
		return -EINVAL;

	uint64_t flags;
	spinlock_acquire_irqsave(&g_task_list_lock, &flags);

	struct task* cur;
	list_for_each_entry(cur, &g_task_list, list_node)
	{
		if(cur->pgid == pgrp)
		{
			send_signal(cur, signal);
			result = 0;
		}
	}

	spinlock_release_irqsave(&g_task_list_lock, flags);
	return result;
}

void signal_try_handle()
{
	struct task* task = smp_current_task();
	if(!task || !task->rsp || !task->sig_pending)
		return;

	uint64_t flags;
	spinlock_acquire_irqsave(&task->sig_lock, &flags);

	while(task->sig_pending & ~task->sig_blocked)
	{
		int sigidx = __builtin_ctz(task->sig_pending & ~task->sig_blocked);

		task->sig_pending &= ~(1u << sigidx);

		if(sigidx + 1 == SIGCHLD)
			continue;

		task->return_signal = sigidx + 1;

		spinlock_release_irqsave(&task->sig_lock, flags);
		sys_exit(0);
	}
	spinlock_release_irqsave(&task->sig_lock, flags);
}
