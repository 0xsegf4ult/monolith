#include <sys/signal.hpp>
#include <sys/thread.hpp>
#include <sys/spinlock.hpp>
#include <sys/scheduler.hpp>
#include <sys/err.hpp>
#include <types.hpp>
#include <klog.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

bool signal_pending(thread_t* thr)
{
	return (thr->sig_pending & (~thr->sig_blocked)) > 0;
}

int send_signal(thread_t* thr, uint32_t signal)
{
	auto idx = signal - 1;
	
	uint64_t rflags;
	spinlock_acquire_irqsave(thr->sig_lock, rflags);
	thr->sig_pending |= (1 << idx);
	spinlock_release_irqsave(thr->sig_lock, rflags);

	thread_status expected = THREAD_INTR_SLEEPING;
	if(atomic_compare_exchange_strong(&thr->status, &expected, THREAD_RUNNING))
	{
		sched_add_ready(thr);
	}

	return 0;
}

int pid_send_signal(pid_t pid, uint32_t signal)
{
	if(signal < 1 || signal >= NSIG)
		return -EINVAL;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_thread_list_lock, rflags);

	thread_t* cur;
	list_for_each_entry(cur, g_thread_list, list_node)
	{
		if(cur->pid == pid)
		{
			spinlock_release_irqsave(g_thread_list_lock, rflags);
			send_signal(cur, signal);
			return 0;
		}
	}
	spinlock_release_irqsave(g_thread_list_lock, rflags);
	return -ESRCH;
}

int pgrp_send_signal(pid_t pgrp, uint32_t signal)
{
	int result = -ESRCH;

	if(signal < 1 || signal >= NSIG)
		return -EINVAL;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_thread_list_lock, rflags);

	thread_t* cur;
	list_for_each_entry(cur, g_thread_list, list_node)
	{
		if(cur->pgid == pgrp)
		{
			send_signal(cur, signal);
			result = 0;
		}
	}

	spinlock_release_irqsave(g_thread_list_lock, rflags);
	return result;
}
