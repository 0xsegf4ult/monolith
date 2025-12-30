#include <sys/syscall.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>
#include <fs/vfs.hpp>
#include <sys/process.hpp>
#include <sys/scheduler.hpp>

#include <lib/klog.hpp>

void syscall_handler(cpu_context_t* ctx)
{
	auto id = syscall_id(ctx->rax);
	auto* proc = CPU::get_current()->get_current_process();

	switch(id)
	{
	using enum syscall_id;
	case OPEN:
	{
		if(!ctx->rdi)
		{
			ctx->rax = -1;
			break;
		}	

		log::debug("sys_open: {:x} -> {}", ctx->rdi, (const char*)ctx->rdi);
		int v_fd = vfs::open((const char*)ctx->rdi, (int)ctx->rsi);
		if(v_fd < 0)
		{
			ctx->rax = -1;
			break;
		}
		log::debug("sys_open -> fd{}", uint32_t(v_fd));
			
		int fd = -1;
		for(int i = 0; i < 32; i++)
		{
			if(proc->open_files[i] == -1)
			{
				fd = i;
				break;
			}
		}

		log::debug("procfd {}", uint32_t(fd));
		if(fd < 0)
		{
			vfs::close(v_fd);
			ctx->rax = -1;
			break;
		}

		ctx->rax = fd;

		break;
	}
	case CLOSE:
	{
		auto fd = int(ctx->rdi);
		if(fd < 0)
		{
			ctx->rax = 1;
			break;
		}
		
		auto v_fd = proc->open_files[fd];
		
		auto res = vfs::close((int)ctx->rdi);
		ctx->rax = res;
	
		break;
	}
	case READ:
	{
		log::debug("sys_read fd{}", ctx->rdi);

		auto fd = int(ctx->rdi);
		auto* buf = reinterpret_cast<byte*>(ctx->rsi);
		auto len = size_t(ctx->rdx);

		if(fd < 0)
		{
			ctx->rax = -1;
			break;
		}

		// validate buf

		sched_block();
		ctx->rax = vfs::read(proc->open_files[fd], buf, len);

		break;
	}
	case WRITE:
	{
		auto fd = int(ctx->rdi);
		auto* buf = reinterpret_cast<const byte*>(ctx->rsi);
		auto len = size_t(ctx->rdx);

		if(fd < 0)
		{
			ctx->rax = -1;
			break;
		}

		// validate buf
		ctx->rax = vfs::write(proc->open_files[fd], buf, len);

		break;
	}
	case DEBUG_PRINT:
	{
		if(!ctx->rdi)
			break;

		log::debug("userdebug: {}", (const char*)ctx->rdi);
		break;
	}
	default:
		log::error("unknown syscall {:x}", uint64_t(id));
	}
}
