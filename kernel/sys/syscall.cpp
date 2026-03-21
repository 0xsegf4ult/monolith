#include <sys/syscall.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>
#include <fs/vfs.hpp>
#include <sys/err.hpp>
#include <sys/executable.hpp>
#include <sys/process.hpp>
#include <sys/scheduler.hpp>

#include <lib/klog.hpp>

int sys_open(const char* path, int flags)
{
	if(!path)
		return -EFAULT;

	int v_fd = vfs::open(path, flags);
	if(v_fd < 0)
		return v_fd;
	
	auto* proc = CPU::get_current()->get_current_process();
	int fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(proc->open_files[i] == -1)
		{
			fd = i;
			break;
		}
	}

	if(fd < 0)
	{
		vfs::close(v_fd);
		return -EMFILE;
	}
	proc->open_files[fd] = v_fd;
	return fd;
}	

int sys_close(int fd)
{
	if(fd < 0)
		return -EBADF;

	auto* proc = CPU::get_current()->get_current_process();
	auto v_fd = proc->open_files[fd];
	proc->open_files[fd] = -1;

	return vfs::close(v_fd);
}

ssize_t sys_read(int fd, byte* buffer, size_t length)
{
	auto* proc = CPU::get_current()->get_current_process();
	if(fd < 0 || proc->open_files[fd] < 0)
	       return -EBADF;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::read(proc->open_files[fd], buffer, length);
}	

ssize_t sys_write(int fd, const byte* buffer, size_t length)
{
	auto* proc = CPU::get_current()->get_current_process();
	if(fd < 0 || proc->open_files[fd] < 0)
		return -EBADF;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::write(proc->open_files[fd], buffer, length);
}

int sys_spawn(const char* path)
{
	if(!path)
		return -EINVAL;
	
	int fd = vfs::open(path, 0);
	if(fd < 0)
		return fd;
		
	auto* proc = create_process(path, true);
	load_executable(fd, proc);
	vfs::close(fd);
	
	sched_add_ready(proc);
	return 0;
}

void sys_exit(int status)
{
	auto* proc = CPU::get_current()->get_current_process();
	log::debug("process {} exited with status {}", proc->name, uint32_t(status)); 
	sched_kill();	
}

int sys_ioctl(int fd, uint64_t op, uint64_t arg)
{
	auto* proc = CPU::get_current()->get_current_process();
	if(fd < 0 || proc->open_files[fd] < 0)
		return -EBADF;

	return vfs::ioctl(proc->open_files[fd], op, arg); 
}

void sys_dbgwrite(const char* message)
{
	if(!message)
		return;

	log::debug("userdebug: {}", message);
}

void syscall_handler(cpu_context_t* ctx)
{
	auto id = syscall_id(ctx->rdi);

	switch(id)
	{
	using enum syscall_id;
	case OPEN:
		ctx->rax = static_cast<uint64_t>(sys_open((const char*)ctx->rsi, (int)ctx->rdx));
		break;
	case CLOSE:
		ctx->rax = static_cast<uint64_t>(sys_close((int)ctx->rsi));
		break;
	case READ:
		ctx->rax = static_cast<uint64_t>(sys_read((int)ctx->rsi, (byte*)ctx->rdx, (size_t)ctx->rcx));
		break;
	case WRITE:
		ctx->rax = static_cast<uint64_t>(sys_write((int)ctx->rsi, (const byte*)ctx->rdx, (size_t)ctx->rcx));
		break;
	case SPAWN:
		ctx->rax = static_cast<uint64_t>(sys_spawn((const char*)ctx->rsi));
		break;
	case EXIT:
		sys_exit((int)ctx->rsi);
		break;
	case IOCTL:
		ctx->rax = static_cast<uint64_t>(sys_ioctl((int)ctx->rsi, ctx->rdx, ctx->rcx));
		break;
	case DEBUG_PRINT:
		sys_dbgwrite((const char*)ctx->rsi);
		break;
	default:
		log::error("unknown syscall {:x}", uint64_t(id));
	}
}
