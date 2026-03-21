#include <arch/x86_64/apic.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/interrupts.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/serial.hpp>
#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>
#include <sys/process.hpp>
#include <sys/syscall.hpp>

static dev_irq_handler_t irq_handlers[32]{nullptr};

void install_irq_handler(uint8_t irq, dev_irq_handler_t handler)
{
	irq_handlers[irq - 32] = handler;
}

void remove_irq_handler(uint8_t irq)
{
	irq_handlers[irq - 32] = nullptr;
}

static int pf_counter = 0;
static char trace_buf[64];

void handle_pagefault(cpu_context_t* ctx)
{
	pf_counter++;
	uint64_t cr2;
	asm volatile("movq %%cr2, %0" : "=r"(cr2));
	
	format_to(string_span{&trace_buf[0], 64}, "RAX: {:#x}", ctx->rax);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, " RBX: {:#x}", ctx->rbx);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, " RCX: {:#x}", ctx->rcx);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, " RDX: {:#x}\n", ctx->rdx);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, "RDI: {:#x}", ctx->rdi);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, " RSI: {:#x}\n", ctx->rsi);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, "RSP: {:#x}", ctx->rsp);
	early_serial_write(trace_buf);
	format_to(string_span{&trace_buf[0], 64}, " RBP: {:#x}\n", ctx->rbp);
	early_serial_write(trace_buf);
	
	struct stack_frame
	{
		stack_frame* rbp;
		uint64_t rip;
	};

	early_serial_write("Stack trace:\n");

	stack_frame* stk = reinterpret_cast<stack_frame*>(ctx->rbp);
	for(uint32_t frame = 0; stk && frame < 32; frame++)
	{
		format_to(string_span{&trace_buf[0], 64}, "{:#x}\n", stk->rip);
		early_serial_write(trace_buf);
		if(stk->rip == 0x0)
			break;

		stk = stk->rbp;
	}

	auto* proc = CPU::get_current()->get_current_process();
	panic("unhandled page fault in [{}] at RIP {:#x} memory access {:#x} {:b}", proc->name, ctx->rip, cr2, ctx->error_code);
}

void handle_gpf(cpu_context_t* ctx)
{
	panic("general protection fault at RIP {:#x} {:b}", ctx->rip, ctx->error_code);
}

extern "C" cpu_context_t* interrupt_handler(cpu_context_t* ctx)
{
	if(pf_counter >= 2)
	{
		panic("error while handling page fault");
	}

	if(ctx->interrupt_id == InterruptID::PageFault)
	{
		handle_pagefault(ctx);
	}
	else if(ctx->interrupt_id == InterruptID::GPFault)
	{
		handle_gpf(ctx);
	}
	else if(ctx->interrupt_id == 0x80)
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

	lapic::eoi();
	return ctx;
}
