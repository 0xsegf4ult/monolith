#include <sys/syscall.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/smp.hpp>
#include <fs/vfs.hpp>
#include <sys/err.hpp>
#include <sys/executable.hpp>
#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <sys/cred.hpp>
#include <sys/stat.hpp>

#include <lib/klog.hpp>

int sys_open(const char* path, int flags)
{
	if(!path)
		return -EINVAL;

	int v_fd = vfs::open(path, flags);
	if(v_fd < 0)
		return v_fd;
	
	auto* thr = smp_current_cpu()->get_current_thread();
	int fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(thr->open_files[i] == -1)
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
	thr->open_files[fd] = v_fd;
	return fd;
}	

int sys_openat(int fd, const char* path, int flags)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0 || !path)
		return -EINVAL;

	int v_fd = vfs::openat(thr->open_files[fd], path, flags);
	if(v_fd < 0)
		return v_fd;

	int thr_fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(thr->open_files[i] == -1)
		{
			thr_fd = i;
			break;
		}
	}

	if(thr_fd < 0)
	{
		vfs::close(v_fd);
		return -EMFILE;
	}
	thr->open_files[thr_fd] = v_fd;
	return thr_fd;
}

int sys_close(int fd)
{
	if(fd < 0)
		return -EBADF;

	auto* thr = smp_current_cpu()->get_current_thread();
	auto v_fd = thr->open_files[fd];
	thr->open_files[fd] = -1;

	return vfs::close(v_fd);
}

ssize_t sys_read(int fd, byte* buffer, size_t length)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0)
	       return -EBADF;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::read(thr->open_files[fd], buffer, length);
}	

ssize_t sys_write(int fd, const byte* buffer, size_t length)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0)
		return -EBADF;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::write(thr->open_files[fd], buffer, length);
}

int common_spawn(const char** argv, int at_fd)
{
	auto* parent = smp_current_cpu()->get_current_thread();

	auto* thr = create_thread(argv[0], argv, true);
	thr->parent = parent;
	thr->cwd = parent->cwd;
	thr->cred = parent->cred;

	if(parent->children)
		thr->sibling = parent->children;

	parent->children = thr;

	for(int i = 0; i < 32; i++)
	{
		if(parent->open_files[i] >= 0)
			thr->open_files[i] = vfs::dup(parent->open_files[i]);
	}

	auto* exec_dir = at_fd >= 0 ? vfs::get_open_fd(at_fd).path : thr->cwd;
	int exec_status = load_executable(argv[0], thr, exec_dir);
       	if(exec_status < 0)
	{
		thr->status = thread_status::terminated;
		thread_zombify(thr);	
		return exec_status;
	}	

	sched_add_ready(thr);
	return 0;
}

int sys_spawn(const char** argv)
{
	if(!argv || !argv[0])
		return -EINVAL;

	auto res = common_spawn(argv, -1);
	return res;
}

int sys_spawnat(int fd, const char** argv)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr->open_files[fd] < 0 || !argv || !argv[0])
		return -EINVAL;
	
	auto res = common_spawn(argv, thr->open_files[fd]);
	return res;
}

void sys_exit(int status)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	log::debug("thread {} exited with status {}", thr->name, uint32_t(status)); 
	thr->return_status = status;
	if(thr->parent && thr->parent->status == thread_status::sleeping)
		sched_unblock(thr->parent);

	thread_zombify(thr);	
	sched_block(thread_status::terminated);	
}

int sys_wait()
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(!thr->children)
		return -ECHILD;
	
	if(thr->children->status != thread_status::terminated)
		sched_block(thread_status::sleeping);

	return thr->children->return_status;
}

int sys_ioctl(int fd, uint64_t op, uint64_t arg)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0)
		return -EBADF;

	return vfs::ioctl(thr->open_files[fd], op, arg); 
}

int sys_stat(const char* path, vfs::stat_t* buffer)
{
	if(!path || !buffer)
		return -EINVAL;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::stat(path, buffer);
}

int sys_fstat(int fd, vfs::stat_t* buffer)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0)
		return -EBADF;

	if(!buffer)
		return -EINVAL;

        if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
                return -EFAULT;

	return vfs::fstat(thr->open_files[fd], buffer);
}

ssize_t sys_getdents(int fd, byte* buffer, size_t length)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0)
		return -EBADF;
	
	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::getdents(fd, buffer, length);
}

int sys_chdir(const char* path)
{
	if(!path)
		return -EINVAL;

	auto query = vfs::lookup(path, 0); 
	if(!query.result)
		return -ENOENT;

	if(!S_ISDIR(query.result->node->mode))
	       return -ENOTDIR;	

	auto* thr = smp_current_cpu()->get_current_thread();
	thr->cwd = query.result;

	return 0;
}

int sys_mkdir(const char* path, mode_t mode)
{
	if(!path)
		return -EINVAL;

	return vfs::mkdir(path, mode);
}

int sys_getcwd(char* buffer, size_t max_len)
{
	if(!buffer)
		return -EINVAL;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	auto* thr = smp_current_cpu()->get_current_thread();
	
	//FIXME: resolve full name from root
	strncpy(buffer, thr->cwd->name, max_len);

	return 0;
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
	case OPENAT:
		ctx->rax = static_cast<uint64_t>(sys_openat((int)ctx->rsi, (const char*)ctx->rdx, (int)ctx->rcx));
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
		ctx->rax = static_cast<uint64_t>(sys_spawn((const char**)ctx->rsi));
		break;
	case SPAWNAT:
		ctx->rax = static_cast<uint64_t>(sys_spawnat((int)ctx->rsi, (const char**)ctx->rdx));
		break;
	case EXIT:
		sys_exit((int)ctx->rsi);
		break;
	case WAIT:
		ctx->rax = static_cast<uint64_t>(sys_wait());
		break;
	case IOCTL:
		ctx->rax = static_cast<uint64_t>(sys_ioctl((int)ctx->rsi, ctx->rdx, ctx->rcx));
		break;
	case STAT:
		ctx->rax = static_cast<uint64_t>(sys_stat((const char*)ctx->rsi, (vfs::stat_t*)ctx->rdx));
		break;
	case FSTAT:
		ctx->rax = static_cast<uint64_t>(sys_fstat((int)ctx->rsi, (vfs::stat_t*)ctx->rdx));
		break;
	case GETDENTS:
		ctx->rax = static_cast<uint64_t>(sys_getdents((int)ctx->rsi, (byte*)ctx->rdx, (size_t)ctx->rcx));
		break;
	case CHDIR:
		ctx->rax = static_cast<uint64_t>(sys_chdir((const char*)ctx->rsi));
		break;
	case MKDIR:
		ctx->rax = static_cast<uint64_t>(sys_mkdir((const char*)ctx->rsi, (mode_t)ctx->rdx));
		break;
	case GETCWD:
		ctx->rax = static_cast<uint64_t>(sys_getcwd((char*)ctx->rsi, (size_t)ctx->rdx));
		break;
	case DEBUG_PRINT:
		sys_dbgwrite((const char*)ctx->rsi);
		break;
	default:
		log::error("unknown syscall {:x}", uint64_t(id));
	}
}
