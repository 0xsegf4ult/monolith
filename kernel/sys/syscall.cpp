#include <sys/syscall.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/smp.hpp>
#include <fs/lookup.hpp>
#include <fs/vfs.hpp>
#include <sys/err.hpp>
#include <sys/executable.hpp>
#include <sys/thread.hpp>
#include <sys/scheduler.hpp>
#include <sys/cred.hpp>
#include <sys/stat.hpp>
#include <sys/signal.hpp>
#include <mm/address_space.hpp>

#include <klog.hpp>
#include <panic.hpp>

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
	auto* thr = smp_current_cpu()->get_current_thread();
	if(fd < 0 || thr->open_files[fd] < 0)
		return -EBADF;

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

enum SPAWNFLAGS
{
	SPAWN_SETPGID = 1
};

int common_spawn(pid_t* out_pid, const char** argv, uint64_t flags, int at_fd)
{
	auto* parent = smp_current_cpu()->get_current_thread();

	auto* thr = create_thread(argv[0], argv, true);
	thr->parent = parent;

	if(flags & SPAWN_SETPGID)
		thr->pgid = thr->pid;
	else	
		thr->pgid = parent->pgid;
	
	thr->sid = parent->sid;

	thr->cwd = parent->cwd;
	ventry_ref(thr->cwd);
	
	thr->cred = parent->cred;
	thr->tty = parent->tty;

	uint64_t rflags;
	spinlock_acquire_irqsave(parent->child_list_lock, rflags);
	list_add(parent->children, thr->sibling);
	spinlock_release_irqsave(parent->child_list_lock, rflags);

	for(int i = 0; i < 32; i++)
	{
		if(parent->open_files[i] >= 0)
			thr->open_files[i] = vfs::dup(parent->open_files[i]);
	}

	auto* exec_dir = at_fd >= 0 ? vfs::get_open_fd(at_fd).path : thr->cwd;
	int exec_status = load_executable(argv[0], thr, exec_dir);
       	if(exec_status < 0)
	{
		thread_zombify(thr);	
		return exec_status;
	}	

	if(out_pid)
		*out_pid = thr->pid;

	sched_add_ready(thr);
	return 0;
}

int sys_spawn(pid_t* out_pid, const char** argv, uint64_t flags)
{
	if(!argv || !argv[0])
		return -EINVAL;

	if(reinterpret_cast<virtaddr_t>(out_pid) > 0x7fffffffffff)
		return -EFAULT; 

	auto res = common_spawn(out_pid, argv, flags, -1);
	return res;
}

int sys_spawnat(int fd, pid_t* out_pid, const char** argv, uint64_t flags)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr->open_files[fd] < 0 || !argv || !argv[0])
		return -EINVAL;
	
	if(reinterpret_cast<virtaddr_t>(out_pid) > 0x7fffffffffff)
		return -EFAULT; 
	
	auto res = common_spawn(out_pid, argv, flags, thr->open_files[fd]);
	return res;
}

void sys_exit(int status)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr->pid == 1)
		panic("init exited with status {}", uint32_t(status));

	thr->return_status = status;
	
	thread_status exp_state = THREAD_RUNNING;
	if(!atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_ZOMBIE))
	{
		exp_state = THREAD_INTR_SLEEPING;
		if(!atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_ZOMBIE))
			panic("sys_exit: thread in invalid state {}", get_status_name(exp_state));
	}

	thread_zombify(thr);	

	asm volatile("mfence" ::: "memory");
	uint64_t rflags;
	spinlock_acquire_irqsave(thr->child_list_lock, rflags);
	thread_t* parent = atomic_load_explicit(&thr->parent, memory_order_acquire);

	if(parent)
	{
		send_signal(parent, SIGCHLD);
	}

	spinlock_release_irqsave(thr->child_list_lock, rflags);
	sched_yield();
}

pid_t sys_wait(int* status)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	bool found_child = false;
	int out_status = 0;
	pid_t pid = 0;

	thread_status exp_state = THREAD_RUNNING;
	while(1)
	{
		uint64_t rflags;
		spinlock_acquire_irqsave(thr->child_list_lock, rflags);
		if(list_empty(thr->children))
		{
			spinlock_release_irqsave(thr->child_list_lock, rflags);
			atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_RUNNING); 
			return -ECHILD;
		}

		thread_t* child;
		list_for_each_entry(child, thr->children, sibling)
		{
			spinlock_acquire(child->child_list_lock);
			auto flags = atomic_load(&child->flags);

			if(thread_is_zombie(child) && (flags & THREAD_CAN_REAP))
			{
				list_del(child->sibling);
				spinlock_release(child->child_list_lock);

				if(child->return_signal)
					out_status = (child->return_signal & 0xFF) | 0x200;
				else
					out_status = (child->return_status & 0xFF) | 0x100;

				found_child = true;
				pid = child->pid;

				destroy_thread(child);
				break;
			}
			spinlock_release(child->child_list_lock);
		}
		spinlock_release_irqsave(thr->child_list_lock, rflags);
		
		if(found_child)
			break;
	
		if(signal_pending(thr))
			return -EINTR;
		
		if(!atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_INTR_SLEEPING))
			panic("sys_wait: thread in invalid state {}", get_status_name(exp_state));

		exp_state = THREAD_INTR_SLEEPING;
		
		sched_yield();
		exp_state = THREAD_RUNNING;
	}
	exp_state = THREAD_INTR_SLEEPING;
	atomic_compare_exchange_strong(&thr->status, &exp_state, THREAD_RUNNING);

	if(status)
	{
		if(reinterpret_cast<virtaddr_t>(status) > 0x7fffffffffff)
			return -EFAULT;

		*status = out_status;
	}

	return pid;
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

	return vfs::getdents(thr->open_files[fd], buffer, length);
}

int sys_chdir(const char* path)
{
	if(!path)
		return -EINVAL;

	vfs::ventry_t* query = nullptr;
	auto status = vfs::lookup(path, &query, 0); 
	if(status < 0)
		return status;

	if(!S_ISDIR(query->node->mode))
	       return -ENOTDIR;	

	auto* thr = smp_current_cpu()->get_current_thread();
	ventry_ref(query);
	ventry_put(thr->cwd);
	thr->cwd = query;

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

void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	if(!length)
		return (void*)-EINVAL;

	if(!(flags & MAP_PRIVATE))
		return (void*)-EINVAL;

	auto* thr = smp_current_cpu()->get_current_thread();
	bool is_anon = (flags & MAP_ANONYMOUS);
	if(is_anon)
	{
		if(addr)
			return (void*)-EINVAL;

		uint64_t vmflags = 0;
		if(prot & PROT_WRITE)
			vmflags |= vm_write;
		if(prot & PROT_EXEC)
			vmflags |= vm_exec;

		auto virt = thr->vm_space->alloc(length, vmflags | vm_user);
		if(!virt)
			return (void*)-ENOMEM;

		return (void*)virt;
	}
	else
	{
		return (void*)-EINVAL;
	}
}

int sys_munmap(void* addr, size_t length)
{
	return -1;
}

int sys_mount(const char* source, const char* target, const char* fsname)
{
	if(!target || !fsname)
		return -EINVAL;
	
	if(reinterpret_cast<virtaddr_t>(source) > 0x7fffffffffff)
		return -EFAULT;

	if(reinterpret_cast<virtaddr_t>(target) > 0x7fffffffffff)
		return -EFAULT;
	
	if(reinterpret_cast<virtaddr_t>(fsname) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::mount(source, target, fsname);
}

int sys_getuid()
{
	return smp_current_cpu()->get_current_thread()->cred.uid;
}

int sys_setuid(uid_t uid)
{
	auto* creds = &smp_current_cpu()->get_current_thread()->cred;
	if(creds->euid == 0)
	{
		creds->uid = uid;
		creds->euid = uid;
		creds->suid = uid;
	}
	else if(creds->uid == uid || creds->suid == uid)
		creds->euid = uid;
	else
		return -EPERM;

	return 0;
}

int sys_getgid()
{
	return smp_current_cpu()->get_current_thread()->cred.gid;
}

int sys_setgid(gid_t gid)
{
	auto* creds = &smp_current_cpu()->get_current_thread()->cred;
	if(creds->euid == 0)
	{
		creds->gid = gid;
		creds->egid = gid;
		creds->sgid = gid;
	}
	else if(creds->gid == gid || creds->sgid == gid)
		creds->egid = gid;
	else
		return -EPERM;

	return 0;
}

pid_t sys_getpid()
{
	auto* thr = smp_current_cpu()->get_current_thread();
	return thr->pid;
}

int sys_setsid()
{
	auto* thr = smp_current_cpu()->get_current_thread();

	// already is a process group leader, good enough check?
	if(thr->pid == thr->pgid)
		return -EPERM;

	thr->pgid = thr->pid;
	thr->sid = thr->pid;
	thr->tty = nullptr;
	return 0;
}

pid_t sys_getpgid(pid_t pid)
{
	auto* thr = smp_current_cpu()->get_current_thread();
	if(pid == 0)
		return thr->pgid;

	auto* search = thread_lookup_by_pid(pid);
	if(!search)
		return -ESRCH;

	if(search->sid != thr->sid)
		return -EPERM;

	return search->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgid)
{
	// on fully POSIX systems you can do this in between fork() and exec()
	// we do not do forking here
	// use SPAWN_SETPGID flag in spawn() instead
	if(pid != 0)
		return -EACCES;

	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr->pid == thr->sid)
		return -EPERM;

	if(pgid == 0)
	{
		thr->pgid = thr->pid;
		return 0;
	}

	auto* pgrp_leader = get_pgrp_leader(pgid);
	if(!pgrp_leader)
		return -ESRCH;

	if(pgrp_leader->sid != thr->sid)
		return -EPERM;

	thr->pgid = pgid;
	return 0;
}

}

void syscall_handler(cpu_context_t* ctx)
{
	auto id = syscall_id(ctx->rax);

	switch(id)
	{
	using enum syscall_id;
	case OPEN:
		ctx->rax = static_cast<uint64_t>(sys_open((const char*)ctx->rdi, (int)ctx->rsi));
		break;
	case OPENAT:
		ctx->rax = static_cast<uint64_t>(sys_openat((int)ctx->rdi, (const char*)ctx->rsi, (int)ctx->rdx));
		break;
	case CLOSE:
		ctx->rax = static_cast<uint64_t>(sys_close((int)ctx->rdi));
		break;
	case READ:
		ctx->rax = static_cast<uint64_t>(sys_read((int)ctx->rdi, (byte*)ctx->rsi, (size_t)ctx->rdx));
		break;
	case WRITE:
		ctx->rax = static_cast<uint64_t>(sys_write((int)ctx->rdi, (const byte*)ctx->rsi, (size_t)ctx->rdx));
		break;
	case SPAWN:
		ctx->rax = static_cast<uint64_t>(sys_spawn((pid_t*)ctx->rdi, (const char**)ctx->rsi, ctx->rdx));
		break;
	case SPAWNAT:
		ctx->rax = static_cast<uint64_t>(sys_spawnat((int)ctx->rdi, (pid_t*)ctx->rsi, (const char**)ctx->rdx, ctx->rcx));
		break;
	case EXIT:
		sys_exit((int)ctx->rdi);
		break;
	case WAIT:
		ctx->rax = static_cast<uint64_t>(sys_wait((int*)ctx->rdi));
		break;
	case IOCTL:
		ctx->rax = static_cast<uint64_t>(sys_ioctl((int)ctx->rdi, ctx->rsi, ctx->rdx));
		break;
	case STAT:
		ctx->rax = static_cast<uint64_t>(sys_stat((const char*)ctx->rdi, (vfs::stat_t*)ctx->rsi));
		break;
	case FSTAT:
		ctx->rax = static_cast<uint64_t>(sys_fstat((int)ctx->rdi, (vfs::stat_t*)ctx->rsi));
		break;
	case GETDENTS:
		ctx->rax = static_cast<uint64_t>(sys_getdents((int)ctx->rdi, (byte*)ctx->rsi, (size_t)ctx->rdx));
		break;
	case CHDIR:
		ctx->rax = static_cast<uint64_t>(sys_chdir((const char*)ctx->rdi));
		break;
	case MKDIR:
		ctx->rax = static_cast<uint64_t>(sys_mkdir((const char*)ctx->rdi, (mode_t)ctx->rsi));
		break;
	case GETCWD:
		ctx->rax = static_cast<uint64_t>(sys_getcwd((char*)ctx->rdi, (size_t)ctx->rsi));
		break;
	case MMAP:
		ctx->rax = reinterpret_cast<uint64_t>(sys_mmap((void*)ctx->rdi, (size_t)ctx->rsi, (int)ctx->rdx, (int)ctx->rcx, (int)ctx->r8, (off_t)ctx->r9));
		break;
	case MUNMAP:
		ctx->rax = static_cast<uint64_t>(sys_munmap((void*)ctx->rdi, (size_t)ctx->rsi));
		break;
	case MOUNT:
		ctx->rax = static_cast<uint64_t>(sys_mount((const char*)ctx->rdi, (const char*)ctx->rsi, (const char*)ctx->rdx));
		break;
	case GETUID:
		ctx->rax = static_cast<uint64_t>(sys_getuid());
		break;
	case SETUID:
		ctx->rax = static_cast<uint64_t>(sys_setuid((uid_t)ctx->rdi));
		break;
	case GETGID:
		ctx->rax = static_cast<uint64_t>(sys_getgid());
		break;
	case SETGID:
		ctx->rax = static_cast<uint64_t>(sys_setgid((gid_t)ctx->rdi));
		break;
	case GETPID:
		ctx->rax = static_cast<uint64_t>(sys_getpid());
		break;
	case SETSID:
		ctx->rax = static_cast<uint64_t>(sys_setsid());
		break;
	case GETPGID:
		ctx->rax = static_cast<uint64_t>(sys_getpgid((pid_t)ctx->rdi));
		break;
	case SETPGID:
		ctx->rax = static_cast<uint64_t>(sys_setpgid((pid_t)ctx->rdi, (pid_t)ctx->rsi));
		break;
	default:
		log::error("unknown syscall {:x}", uint64_t(id));
	}
}
