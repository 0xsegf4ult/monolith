#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <sys/smp.hpp>
#include <sys/task.hpp>
#include <klog.hpp>
#include <kfmt.hpp>

struct stack_frame
{
	stack_frame* rbp;
	uint64_t rip;
};

void dump_registers(cpu_context_t* ctx, uint32_t flags)
{
	generic_log_nolock("RAX: {:x} RBX: {:x} RCX: {:x}\n", ctx->rax, ctx->rbx, ctx->rcx);
	generic_log_nolock("RDX: {:x} RSI: {:x} RDI: {:x}\n",  ctx->rdx, ctx->rsi, ctx->rdi);
	generic_log_nolock("RBP: {:x} R08: {:x} R09: {:x}\n", ctx->rbp, ctx->r8, ctx->r9);
	generic_log_nolock("R10: {:x} R11: {:x} R12: {:x}\n", ctx->r10, ctx->r11, ctx->r12);
	generic_log_nolock("R13: {:x} R14: {:x} R15: {:x}\n", ctx->r13, ctx->r14, ctx->r15);
	generic_log_nolock("RIP: {:#x}:{:#x}\nRSP: {:#x}:{:#x}\nRFLAGS: {:x}\n", ctx->cs, ctx->rip, ctx->ss, ctx->rsp, ctx->rflags);
}

bool valid_frame(uint64_t addr)
{
	auto* task = smp_current_task();
	if(addr >= 0x7fffffffffff)
	{
		if(addr + 8 >= task->rsp0)
			return false;
	}
	else
		if(addr + 8 >= task->rsp)
			return false;

	return true;
}

void stacktrace(uint64_t frame, uint32_t flags)
{
	stack_frame* stk = reinterpret_cast<stack_frame*>(frame > 0 ? (void*)frame : __builtin_frame_address(0));
	for(uint32_t sf = 0; stk && sf < 32; sf++)
	{
		if((uint64_t)stk & 0x7)
		{
			generic_log_nolock("??? unaligned stackframe {:#x}", stk);
			break;
		}

		if(!valid_frame(uint64_t(stk)))
		{
			generic_log_nolock("<reached top of stack>");
			break;
		}	
		
		if(stk->rip == 0x0)
			break;

		generic_log_nolock("{:#x} {}\n", stk->rip, "?");

		stk = stk->rbp;
	}
}

void arch_set_tls(virtaddr_t base)
{
	wrmsr(FS_BASE, base);
}
