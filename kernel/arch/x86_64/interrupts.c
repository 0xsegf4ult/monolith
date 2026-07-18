#include <arch/x86_64/irq.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/mmu.h>
#include <mm/vmm.h>
#include <sched/task.h>
#include <sys/irq.h>
#include <sys/timer.h>
#include <sys/smp.h>
#include <config.h>
#include <types.h>
#include <klog.h>
#include <panic.h>

void page_fault_handler(interrupt_frame* frame)
{
	uint64_t fault_addr;
	asm volatile("movq %%cr2, %0" : "=r"(fault_addr));

	struct task* task = smp_current_task();

	uint64_t pflags = 0;
	if(frame->error_code & PF_PRESENT)
		pflags |= VM_FAULT_PRESENT;
	if(frame->error_code & PF_WRITE)
		pflags |= VM_FAULT_WRITE;
	if(frame->error_code & PF_USER)
		pflags |= VM_FAULT_USER;
	if(frame->error_code & PF_FETCH)
		pflags |= VM_FAULT_FETCH;

	if(vm_page_fault(align_down(fault_addr, CONFIG_PAGE_SIZE), pflags))
		return;

	if(task && task->rsp && frame->rip <=  0x7fffffffffff)
        {
		dump_registers(frame);
		stacktrace(frame->rbp);
		klog("%s[%d]: segfault on cpu%u at %p ip %p sp %p error %u\n", task->name, task->pid, smp_current_cpu(), fault_addr, frame->rip, frame->rsp, frame->error_code);
		return;
	}

	panic_prepare();

	klog_nolock("\n\033[31mkernel panic:\033[0m unhandled page fault at %p %u\n", fault_addr, frame->error_code);
	klog_nolock("CPU: %d PID: %d [%s] %s\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");

	dump_registers(frame);
	stacktrace(frame->rbp);

	panic_complete();
}

void gpf_handler(interrupt_frame* frame)
{
	struct task* task = smp_current_task();

	if(task && task->rsp && frame->rip <= 0x7fffffffffff)
        {
		dump_registers(frame);
		stacktrace(frame->rbp);
		klog("%s[%d]: segfault on cpu%u ip %p sp %p error %u\n", task->name, task->pid, smp_current_cpu(), frame->rip, frame->rsp, frame->error_code);
		return;
	}

	panic_prepare();

	klog_nolock("\n\033[31mkernel panic:\033[0m unhandled general protection fault RIP %p [%d]\n", frame->rip, frame->error_code);
	klog_nolock("CPU: %d PID: %d [%s] %s\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");

	dump_registers(frame);
	stacktrace(frame->rbp);
	
	panic_complete();
}

void exception_handler(interrupt_frame* frame)
{
	if(frame->vector == INTERRUPT_VECTOR_PAGE_FAULT)
		page_fault_handler(frame);
	else if(frame->vector == INTERRUPT_VECTOR_GPF)
		gpf_handler(frame);
	else if(frame->vector == INTERRUPT_VECTOR_NMI)
		native_cpu_halt();
	else
	{
		struct task* task = smp_current_task();
		if(task && task->rsp && frame->rip <= 0x7fffffffffff)
        	{
			dump_registers(frame);
			stacktrace(frame->rbp);
			klog("%s[%d]: deadlysignal %x (vector %x) on cpu%d ip %p sp %p\n", task->name, task->pid, 0, frame->vector, smp_current_cpu(), frame->rip, frame->rsp);
			return;
		}

		panic_prepare();

		klog_nolock("\n\033[31mkernel panic:\033[0m unhandled exception %x\n", frame->vector);
		klog_nolock("CPU: %d PID: %d [%s] %s\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");

		dump_registers(frame);
		stacktrace(frame->rbp);
		
		panic_complete();
	}
}

void syscall_handler(interrupt_frame* frame)
{
}

void interrupt_handler(interrupt_frame* frame)
{
	if(frame->vector < INTERRUPT_VECTOR_EXCEPTION_END)
		exception_handler(frame);
	else if(frame->vector >= INTERRUPT_VECTOR_HW_START && frame->vector < INTERRUPT_VECTOR_HW_END)
		irq_dispatch(frame->vector);
	else if(frame->vector == INTERRUPT_VECTOR_SYSCALL)
		syscall_handler(frame);
	else if(frame->vector = INTERRUPT_VECTOR_TIMER)
		timer_interrupt();
	else if(frame->vector == INTERRUPT_VECTOR_SPURIOUS)
	{
	}
}
