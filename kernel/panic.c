#include <panic.h>
#include <irq.h>
#include <cpu.h>

#include <libk/vsprintf.h>
#include <sched/task.h>
#include <sys/smp.h>
#include <klog.h>

#include <stdarg.h>
#include <stdatomic.h>

static _Atomic int32_t panic_cpu = -1;

void panic_prepare()
{
	local_irq_disable();

	int32_t expected_cpu = -1;
	int32_t desired_cpu = smp_current_cpu();
	if(!atomic_compare_exchange_strong_explicit(&panic_cpu, &expected_cpu, desired_cpu, memory_order_release, memory_order_relaxed))
	{
		if(expected_cpu == desired_cpu)
			klog_write_nolock("\n\033[031mnested panic, halting...");

		native_cpu_halt();
	}

	smp_stop_cpus();
}	

void panic_complete()
{
	native_cpu_halt();
}

static char panic_buf[1024];

void panic(const char* fmt, ...)
{
	panic_prepare();

	klog_write_nolock("\n\033[31mkernel panic:\033[0m ");
	
	va_list args;
	va_start(args, fmt);
	ssize_t written = vsprintf(panic_buf, fmt, args);
	va_end(args);

	panic_buf[written] = '\n';
	klog_write_nolock(panic_buf);

	struct task* task = smp_current_task();
	klog_nolock("CPU: %u PID: %d [%s] %s\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");
	stacktrace(0);

	panic_complete();
}
