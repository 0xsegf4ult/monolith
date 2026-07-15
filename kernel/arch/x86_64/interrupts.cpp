#include <arch/x86_64/interrupts.hpp>
#include <arch/x86_64/lapic.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/serial.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/generic.hpp>

#include <mm/vmm.hpp>
#include <klog.hpp>
#include <kstd.hpp>
#include <panic.hpp>
#include <types.hpp>

#include <sys/task.hpp>
#include <sys/syscall.hpp>
#include <sys/scheduler.hpp>
#include <sys/signal.hpp>
#include <sys/smp.hpp>

extern "C" void isr_stubs();
static idt_entry_t idt_entries[256];

static dev_irq_handler_t irq_handlers[32]{nullptr};
static uint32_t irq_handler_counter = 0;

void load_idt()
{
	idtr_t idtr;
	idtr.limit = 256 * sizeof(idt_entry_t) - 1;
	idtr.base = idt_entries;

	asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

void setup_idt()
{
	virtaddr_t isr_start = reinterpret_cast<virtaddr_t>(&isr_stubs);

	for(int i = 0; i < 256; i++)
	{
		auto dpl = DPL_KERNEL;

		//syscall
		if(i == 0x80)
			dpl = DPL_USER;

		idt_entries[i] = idt_entry_t(isr_start + (i * 16), KERNEL_CS, IDT_INTR_GATE, dpl);
	}

	load_idt();
}

uint32_t allocate_irq()
{
	return (irq_handler_counter++) + 32;
}

void install_irq_handler(uint8_t irq, dev_irq_handler_t handler)
{
	irq_handlers[irq - 32] = handler;
}

void remove_irq_handler(uint8_t irq)
{
	irq_handlers[irq - 32] = nullptr;
}

static void handle_pagefault(cpu_context_t* ctx)
{
	uint64_t cr2;
	asm volatile("movq %%cr2, %0" : "=r"(cr2));
	
	auto* task = smp_current_task();
	
	uint64_t pflags = 0;
	if(ctx->error_code & PF_PRESENT)
		pflags |= VM_FAULT_PRESENT;
	if(ctx->error_code & PF_WRITE)
		pflags |= VM_FAULT_WRITE;
	if(ctx->error_code & PF_USER)
		pflags |= VM_FAULT_USER;
	if(ctx->error_code & PF_FETCH)
		pflags |= VM_FAULT_FETCH;

	if(vm_page_fault(align_down(cr2, ARCH_PAGE_SIZE), pflags))
		return;
		
	if(task && task->rsp && ctx->rip <= 0x7fffffffffff)
	{
		dump_registers(ctx, 0);
		stacktrace(ctx->rbp, 0);
		log::error("{}[{}]: segfault on cpu{} at {:x} ip {:x} sp {:x} error {}", task->name, task->pid, smp_current_cpu(), cr2, ctx->rip, ctx->rsp, ctx->error_code);

		send_signal(task, SIGSEGV);
		return;
	}
	
	panic_prepare();

	generic_log_nolock("\n\033[31mkernel panic:\033[0m unhandled page fault at memory {:#x} [{:b}]\n", cr2, ctx->error_code);
	generic_log_nolock("task: {:#x}\n", task);
	generic_log_nolock("CPU: {} PID: {} [{}] {}\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");

	dump_registers(ctx, 0);
	stacktrace(ctx->rbp, 0);
	
	panic_complete();
}

static void signal_handle(task_t* task)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(task->sig_lock, rflags);
	while(task->sig_pending & (~task->sig_blocked))
	{
		int sigidx = __builtin_ctz(task->sig_pending & (~task->sig_blocked));
		
		task->sig_pending &= ~(1u << sigidx);

		if(sigidx + 1 == SIGCHLD)
			continue;

		task->return_signal = sigidx + 1;

		spinlock_release_irqsave(task->sig_lock, rflags);
		sys_exit(0);
		__builtin_unreachable();
	}
	spinlock_release_irqsave(task->sig_lock, rflags);
}

static void handle_gpf(cpu_context_t* ctx)
{
	auto* task = smp_current_task();

	if(task && task->rsp && ctx->rip <= 0x7fffffffffff)
	{
		dump_registers(ctx, 0);
		stacktrace(ctx->rbp, 0);
		log::error("{}[{}]: segfault on cpu{} ip {:x} sp {:x} error {}", task->name, task->pid, smp_current_cpu(), ctx->rip, ctx->rsp, ctx->error_code);
		send_signal(task, SIGSEGV);
		return ;
	}
	
	panic_prepare();

	generic_log_nolock("\n\033[31mkernel panic:\033[0m unhandled general protection fault RIP {:#x} [{:b}]\n", ctx->rip, ctx->error_code);
	generic_log_nolock("task: {:#x}\n", task);
	generic_log_nolock("CPU: {} PID: {} [{}] {}\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");
	dump_registers(ctx, 0);
	stacktrace(ctx->rbp, 0);

	panic_complete();
}

extern "C" void interrupt_handler(cpu_context_t* ctx)
{
	if(ctx->interrupt_id == 0x80)
	{
		syscall_handler(ctx);
		
	}
	else if(ctx->interrupt_id == 0xFF)
	{
		log::warn("interrupts: received spurious interrupt");
	}
	else if(ctx->interrupt_id >= 32)
	{
		if(ctx->interrupt_id < 64)
		{
			if(irq_handlers[ctx->interrupt_id - 32])
				irq_handlers[ctx->interrupt_id - 32]();
		}
	}
	else
		log::warn("unhandled interrupt {}", ctx->interrupt_id);

	lapic_eoi();
		
	auto* task = smp_current_task();
	if(task && task->rsp && task->sig_pending)
	{
		signal_handle(task);
	}	
}

constexpr int exception_to_signal(uint64_t exception)
{
	switch(exception)
	{
	case DivisionError:
		return SIGFPE;
	case Debug:
		return SIGTRAP;
	case Breakpoint:
		return SIGTRAP;
	case Overflow:
		return SIGSEGV;
	case BoundExceeded:
		return SIGSEGV;
	case InvalidOpcode:
		return SIGILL;
	case DeviceUnavailable:
		return SIGFPE;
	case SegmentNotPresent:
		return SIGBUS;
	case StackSegmentFault:
		return SIGBUS;
	case GPFault:
		return SIGSEGV;
	case PageFault:
		return SIGSEGV;
	case FPUException:
		return SIGFPE;
	case AlignmentCheck:
		return SIGBUS;
	case MachineCheck:
		return SIGBUS;
	case SIMDFPException:
		return SIGFPE;
	default:
		return 0;
	}
}

extern "C" void exception_handler(cpu_context_t* ctx)
{
	auto* task = smp_current_task();

	if(ctx->interrupt_id == InterruptID::PageFault)
	{
		handle_pagefault(ctx);
	}
	else if(ctx->interrupt_id == InterruptID::GPFault)
	{
		handle_gpf(ctx);
	}
	else if(ctx->interrupt_id == InterruptID::NMI)
	{
		for(;;)
			asm volatile("cli; hlt");
	}
	else
	{
		if(task && task->rsp && ctx->rip <= 0x7fffffffffff)
		{
			dump_registers(ctx, 0);
			stacktrace(ctx->rbp, 0);
			auto sig = exception_to_signal(ctx->interrupt_id);
			log::error("{}[{}]: deadlysignal {} (arch exc {}) on cpu{} ip {:x} sp {:x}", task->name, task->pid, sig, ctx->interrupt_id, smp_current_cpu(), ctx->rip, ctx->rsp);
			if(sig)
				send_signal(task, sig);
		}
		else
		{
			panic_prepare();

			generic_log_nolock("\n\033[31mkernel panic:\033[0m unhandled exception {:x}\n", ctx->interrupt_id);
			
			generic_log_nolock("CPU: {} PID: {} [{}] {}\n", smp_current_cpu(), task ? task->pid : 0, task ? task->name : "kernel", task ? get_status_name(task->status) : "R");
			
			dump_registers(ctx, 0);
			stacktrace(ctx->rbp, 0);
			panic_complete();
		}
	}

	if(task && task->rsp && task->sig_pending)
		signal_handle(task);
}

