#include <panic.hpp>

#include <arch/generic.hpp>
#include <sys/smp.hpp>
#include <sys/task.hpp>

#include <klog.hpp>

#include <stdatomic.h>

static atomic_int panic_cpu = -1; 

void panic_prepare()
{
	arch_disable_interrupts();

	int expected_cpu = -1;
	int desired_cpu = smp_current_cpu();
	if(!atomic_compare_exchange_strong_explicit(&panic_cpu, &expected_cpu, desired_cpu, memory_order_release, memory_order_relaxed))
	{
		if(expected_cpu != desired_cpu)
			arch_halt();
	
		arch_halt();	
		generic_log_nolock("nested panic?");
	}

	generic_log_nolock("panic acquired on cpu{}", desired_cpu);
	smp_stop_cpus();
}

void panic_complete()
{
	arch_halt();
}

void panic(const char* string)
{
	panic_prepare();

	klog_internal_nolock("\n\033[31mkernel panic:\033[0m ");
        klog_internal_nolock(string);
	klog_internal_nolock("\n");
       
	auto* task = smp_current_task();
	generic_log_nolock("CPU: {} PID: {} [{}] {}\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "?");

	stacktrace(0, TRACE_PANIC);
	panic_complete();
}
