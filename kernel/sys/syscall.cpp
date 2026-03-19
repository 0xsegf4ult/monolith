#include <sys/syscall.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>
#include <fs/vfs.hpp>
#include <sys/executable.hpp>
#include <sys/process.hpp>
#include <sys/scheduler.hpp>

#include <lib/klog.hpp>

void syscall_handler(cpu_context_t* ctx)
{
	auto id = syscall_id(ctx->rdi);
	auto* proc = CPU::get_current()->get_current_process();

	switch(id)
	{
	using enum syscall_id;
	case OPEN:
	{
		if(!ctx->rsi)
		{
			ctx->rax = -1;
			break;
		}	

		log::debug("sys_open: {:x} -> {}", ctx->rsi, (const char*)ctx->rsi);
		int v_fd = vfs::open((const char*)ctx->rsi, (int)ctx->rdx);
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

		proc->open_files[fd] = v_fd;
		ctx->rax = fd;

		break;
	}
	case CLOSE:
	{
		auto fd = int(ctx->rsi);
		if(fd < 0)
		{
			ctx->rax = 1;
			break;
		}
		
		auto v_fd = proc->open_files[fd];
		proc->open_files[fd] = -1;
		
		auto res = vfs::close(v_fd);
		ctx->rax = res;
	
		break;
	}
	case READ:
	{
		log::debug("sys_read fd{}", ctx->rsi);

		auto fd = int(ctx->rsi);
		auto* buf = reinterpret_cast<byte*>(ctx->rdx);
		auto len = size_t(ctx->rcx);

		if(fd < 0 || proc->open_files[fd] < 0)
		{
			ctx->rax = -1;
			break;
		}

		// validate buf

		ctx->rax = vfs::read(proc->open_files[fd], buf, len);

		break;
	}
	case WRITE:
	{
		auto fd = int(ctx->rsi);
		auto* buf = reinterpret_cast<const byte*>(ctx->rdx);
		auto len = size_t(ctx->rcx);
		
		log::debug("sys_write fd {} mem {:x} len {}", ctx->rsi, ctx->rdx, ctx->rcx);

		if(fd < 0 || proc->open_files[fd] < 0)
		{
			ctx->rax = -1;
			break;
		}

		// validate buf
		ctx->rax = vfs::write(proc->open_files[fd], buf, len);

		break;
	}
	case SPAWN:
	{
		auto* path = reinterpret_cast<const char*>(ctx->rsi);
		if(!path)
		{
			ctx->rax = -1;
			break;
		}
		
		log::debug("sys_spawn: {}", path);
		
		int fd = vfs::open((const char*)ctx->rsi, (int)ctx->rdx);
		if(fd < 0)
		{
			ctx->rax = -1;
			break;
		}
		
		auto* proc = create_process(path, true);
		load_executable(fd, proc);
		vfs::close(fd);
		
		sched_add_ready(proc);

		break;
	}
	case EXIT:
	{
		auto status = int(ctx->rsi);
		log::debug("process {} exited with status {}", proc->name, status); 
	
		sched_kill();

		break;
	}
	case DEBUG_PRINT:
	{
		if(!ctx->rsi)
			break;

		log::debug("userdebug: {}", (const char*)ctx->rsi);
		break;
	}
	default:
		log::error("unknown syscall {:x}", uint64_t(id));
	}
}
