#include <arch/x86_64/interrupts.hpp>
#include <arch/x86_64/context.hpp>
#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

dev_irq_handler_t irq_handlers[32]{nullptr};

void install_irq_handler(uint8_t irq, dev_irq_handler_t handler)
{
	irq_handlers[irq - 32] = handler;
}

void remove_irq_handler(uint8_t irq)
{
	irq_handlers[irq - 32] = nullptr;
}

extern "C" cpu_context_t* interrupt_handler(cpu_context_t* ctx)
{
	if(ctx->interrupt_id == InterruptID::PageFault)
	{
		uint64_t cr2;
		asm volatile("movq %%cr2, %0" : "=r"(cr2));
		panic("unhandled page fault at RIP {:x} memory access {:x} {:b}", ctx->rip, cr2, ctx->error_code);
	}
	else if(ctx->interrupt_id == InterruptID::GPFault)
	{
		panic("general protection fault at RIP {:x}", ctx->rip);
	}
	else if(ctx->interrupt_id == 0xFF)
	{
		log::warn("interrupts: received spurious interrupt");
		return ctx;
	}
	else if(ctx->interrupt_id >= 32)
	{
		if(ctx->interrupt_id < 64)
		{
			if(irq_handlers[ctx->interrupt_id - 32])
				irq_handlers[ctx->interrupt_id - 32]();
			else
				log::warn("no interrupt handler for irq {}", ctx->interrupt_id);
		}

		return ctx;
	}

	panic("unhandled interrupt {}", ctx->interrupt_id);
	return ctx;
}
