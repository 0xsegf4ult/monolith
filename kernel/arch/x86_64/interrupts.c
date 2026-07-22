#include <arch/x86_64/irq.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/mmu.h>
#include <arch/x86_64/syscall.h>
#include <mm/vmm.h>
#include <sched/task.h>
#include <sched/signal.h>
#include <sys/irq.h>
#include <sys/timer.h>
#include <sys/smp.h>
#include <config.h>
#include <types.h>
#include <klog.h>
#include <panic.h>

void page_fault_handler(struct interrupt_frame* frame)
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
		send_signal(task, SIGSEGV);
		return;
	}

	panic_prepare();

	klog_nolock("\n\033[31mkernel panic:\033[0m unhandled page fault at %p %u\n", fault_addr, frame->error_code);
	klog_nolock("CPU: %d PID: %d [%s] %s\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");

	dump_registers(frame);
	stacktrace(frame->rbp);

	panic_complete();
}

void gpf_handler(struct interrupt_frame* frame)
{
	struct task* task = smp_current_task();

	if(task && task->rsp && frame->rip <= 0x7fffffffffff)
        {
		dump_registers(frame);
		stacktrace(frame->rbp);
		klog("%s[%d]: segfault on cpu%u ip %p sp %p error %u\n", task->name, task->pid, smp_current_cpu(), frame->rip, frame->rsp, frame->error_code);
		send_signal(task, SIGSEGV);
		return;
	}

	panic_prepare();

	klog_nolock("\n\033[31mkernel panic:\033[0m unhandled general protection fault RIP %p [%d]\n", frame->rip, frame->error_code);
	klog_nolock("CPU: %d PID: %d [%s] %s\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");

	dump_registers(frame);
	stacktrace(frame->rbp);
	
	panic_complete();
}

static uint32_t exception_to_signal(uint64_t vector)
{
	switch(vector)
	{
	case INTERRUPT_VECTOR_DIVISION_ERROR:
	case INTERRUPT_VECTOR_DEVICE_UNAVAILABLE:
	case INTERRUPT_VECTOR_FPU_EXCEPTION:
	case INTERRUPT_VECTOR_SIMD_EXCEPTION:
		return SIGFPE;
	case INTERRUPT_VECTOR_DEBUG:
	case INTERRUPT_VECTOR_BREAKPOINT:
		return SIGTRAP;
	case INTERRUPT_VECTOR_INVALID_OPCODE:
		return SIGILL;
	case INTERRUPT_VECTOR_SEGMENT_NOT_PRESENT:
	case INTERRUPT_VECTOR_STACK_SEGMENT_FAULT:
	case INTERRUPT_VECTOR_ALIGNMENT_CHECK:
	case INTERRUPT_VECTOR_MACHINE_CHECK:
		return SIGBUS;
	case INTERRUPT_VECTOR_OVERFLOW:
	case INTERRUPT_VECTOR_BOUNDS_EXCEEDED:
	case INTERRUPT_VECTOR_INVALID_TSS:
	case INTERRUPT_VECTOR_GPF:
	case INTERRUPT_VECTOR_PAGE_FAULT:
		return SIGSEGV;
	case INTERRUPT_VECTOR_DOUBLE_FAULT:
	default:
		return SIGABRT;
	}
}

void exception_handler(struct interrupt_frame* frame)
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
			uint32_t sig = exception_to_signal(frame->vector);

			dump_registers(frame);
			stacktrace(frame->rbp);
			klog("%s[%d]: deadlysignal %x (vector %x) on cpu%d ip %p sp %p\n", task->name, task->pid, sig, frame->vector, smp_current_cpu(), frame->rip, frame->rsp);
			send_signal(task, sig);
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

void interrupt_handler(struct interrupt_frame* frame)
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

	signal_try_handle();
}
