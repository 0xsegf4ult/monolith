#include <sys/syscall.hpp>
#include <sys/cred.hpp>
#include <sys/err.hpp>
#include <sys/executable.hpp>
#include <sys/scheduler.hpp>
#include <sys/signal.hpp>
#include <sys/smp.hpp>
#include <sys/stat.hpp>
#include <sys/task.hpp>
#include <sys/time.hpp>

#include <fs/lookup.hpp>
#include <fs/vfs.hpp>

#include <mm/vmm.hpp>

#include <net/socket.hpp>

#include <kstd.hpp>
#include <klog.hpp>
#include <panic.hpp>
#include <types.hpp>

int sys_open(const char* path, int flags)
{
	if(!path)
		return -EINVAL;

	int v_fd = vfs::open(path, flags);
	if(v_fd < 0)
		return v_fd;
	
	auto* task = smp_current_task();
	int fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] == -1)
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
	task->open_files[fd] = v_fd;
	return fd;
}	

int sys_openat(int fd, const char* path, int flags)
{
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0 || !path)
		return -EINVAL;

	int v_fd = vfs::openat(task->open_files[fd], path, flags);
	if(v_fd < 0)
		return v_fd;

	int task_fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] == -1)
		{
			task_fd = i;
			break;
		}
	}

	if(task_fd < 0)
	{
		vfs::close(v_fd);
		return -EMFILE;
	}
	task->open_files[task_fd] = v_fd;
	return task_fd;
}

int sys_close(int fd)
{
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	auto v_fd = task->open_files[fd];
	task->open_files[fd] = -1;

	return vfs::close(v_fd);
}

ssize_t sys_read(int fd, byte* buffer, size_t length)
{
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
	       return -EBADF;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::read(task->open_files[fd], buffer, length);
}	

ssize_t sys_write(int fd, const byte* buffer, size_t length)
{
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::write(task->open_files[fd], buffer, length);
}

enum SPAWNFLAGS
{
	SPAWN_SETPGID = 1
};

static int common_spawn(pid_t* out_pid, const char** argv, uint64_t flags, int at_fd)
{
	auto* parent = smp_current_task();

	auto* proc = process_userspace_new(argv[0], argv);
	atomic_store(&proc->parent, parent);

	if(flags & SPAWN_SETPGID)
		proc->pgid = proc->pid;
	else	
		proc->pgid = parent->pgid;
	
	proc->sid = parent->sid;

	proc->cwd = parent->cwd;
	ventry_ref(proc->cwd);
	
	proc->cred = parent->cred;
	proc->tty = parent->tty;

	uint64_t rflags;
	spinlock_acquire_irqsave(parent->child_list_lock, rflags);
	list_add(parent->children, proc->sibling);
	spinlock_release_irqsave(parent->child_list_lock, rflags);

	for(int i = 0; i < 32; i++)
	{
		if(parent->open_files[i] >= 0)
			proc->open_files[i] = vfs::dup(parent->open_files[i]);
	}

	auto* exec_dir = at_fd >= 0 ? vfs::get_open_fd(at_fd).path : proc->cwd;
	int exec_status = load_executable(argv[0], proc, exec_dir);
       	if(exec_status < 0)
	{
		task_zombify(proc);	
		return exec_status;
	}	

	if(out_pid)
		*out_pid = proc->pid;

	sched_add_ready(proc);
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
	auto* task = smp_current_task();
	if(task->open_files[fd] < 0 || !argv || !argv[0])
		return -EINVAL;
	
	if(reinterpret_cast<virtaddr_t>(out_pid) > 0x7fffffffffff)
		return -EFAULT; 
	
	auto res = common_spawn(out_pid, argv, flags, task->open_files[fd]);
	return res;
}

void sys_exit(int status)
{
	auto* task = smp_current_task();
	if(task->pid == 1)
		panic("init exited with status {}", uint32_t(status));

	task->return_status = status;
	
	task_status exp_state = TASK_RUNNING;
	if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_ZOMBIE))
	{
		exp_state = TASK_INTR_SLEEPING;
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_ZOMBIE))
			panic("sys_exit: task in invalid state {}", get_status_name(exp_state));
	}

	task_zombify(task);	

	asm volatile("mfence" ::: "memory");
	uint64_t rflags;
	spinlock_acquire_irqsave(task->child_list_lock, rflags);
	task_t* parent = atomic_load_explicit(&task->parent, memory_order_acquire);

	if(parent)
	{
		send_signal(parent, SIGCHLD);
	}

	spinlock_release_irqsave(task->child_list_lock, rflags);
	sched_yield();
}

pid_t sys_wait(int* status)
{
	auto* task = smp_current_task();
	bool found_child = false;
	int out_status = 0;
	pid_t pid = 0;

	task_status exp_state = TASK_RUNNING;
	while(1)
	{
		uint64_t rflags;
		spinlock_acquire_irqsave(task->child_list_lock, rflags);
		if(list_empty(task->children))
		{
			spinlock_release_irqsave(task->child_list_lock, rflags);
			atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING); 
			return -ECHILD;
		}

		task_t* child;
		list_for_each_entry(child, task->children, sibling)
		{
			spinlock_acquire(child->child_list_lock);
			auto flags = atomic_load(&child->flags);
			bool is_zombie = (atomic_load_explicit(&child->status, memory_order_relaxed) == TASK_ZOMBIE);
			if(is_zombie && (flags & TASK_CAN_REAP))
			{
				list_del(child->sibling);
				spinlock_release(child->child_list_lock);

				if(child->return_signal)
					out_status = (child->return_signal & 0xFF) | 0x200;
				else
					out_status = (child->return_status & 0xFF) | 0x100;

				found_child = true;
				pid = child->pid;

				task_destroy(child);
				break;
			}
			spinlock_release(child->child_list_lock);
		}
		spinlock_release_irqsave(task->child_list_lock, rflags);
		
		if(found_child)
			break;
	
		if(signal_pending(task))
			return -EINTR;
		
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_INTR_SLEEPING))
			panic("sys_wait: task in invalid state {}", get_status_name(exp_state));

		exp_state = TASK_INTR_SLEEPING;
		
		sched_yield();
		exp_state = TASK_RUNNING;
	}
	exp_state = TASK_INTR_SLEEPING;
	atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING);

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
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	return vfs::ioctl(task->open_files[fd], op, arg); 
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
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	if(!buffer)
		return -EINVAL;

        if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
                return -EFAULT;

	return vfs::fstat(task->open_files[fd], buffer);
}

ssize_t sys_getdents(int fd, byte* buffer, size_t length)
{
	auto* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;
	
	if(reinterpret_cast<virtaddr_t>(buffer) > 0x7fffffffffff)
		return -EFAULT;

	return vfs::getdents(task->open_files[fd], buffer, length);
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

	auto* task = smp_current_task();
	ventry_ref(query);
	ventry_put(task->cwd);
	task->cwd = query;

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

	auto* task = smp_current_task();
	
	//FIXME: resolve full name from root
	strncpy(buffer, task->cwd->name, max_len);

	return 0;
}

void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	if(addr || !length || offset & 0xFFF)
		return (void*)-EINVAL;

	if(!(flags & MAP_PRIVATE) && !(flags & MAP_SHARED))
		return (void*)-EINVAL;

	if((flags & MAP_PRIVATE) && (flags & MAP_SHARED))
		return (void*)-EINVAL;
		
	auto* task = smp_current_task();
	bool is_anon = (flags & MAP_ANONYMOUS);
	if(is_anon)
	{
		if(fd >= 0)
			return (void*)-EINVAL;

		if(flags & MAP_SHARED)
			return (void*)-ENOTSUP;

		auto virt = vm_space_map(task->current_vm_space,
		{
			.length = length,
			.prot = (prot & 7) | PROT_USER 
		});
		
		return (void*)virt;
	}
	else
	{
		if(fd < 0)
			return (void*)-EBADF;

		int v_fd = task->open_files[fd];
		if(v_fd < 0)
			return (void*)-EBADF;

		auto& filp = vfs::get_open_fd(v_fd);
		if(!filp.inode->fops->mmap)
			return (void*)-ENODEV;

		auto virt = vm_space_map(task->current_vm_space,
		{
			.length = length,
			.prot = (prot & 7) | PROT_USER,
			.flags = VM_FLAG_FILE,
			.offset = offset,
			.fd = v_fd
		});

		return (void*)virt;
	}
}

int sys_munmap(void* addr, size_t length)
{
	//TODO: page size might not be 4K
	if(!length || (virtaddr_t)addr & 0xFFF)
		return -EINVAL;

	if((virtaddr_t)addr < VM_USERSPACE_BASE)
		return -EINVAL;

	if((virtaddr_t)addr + length > VM_USERSPACE_END)
		return -EINVAL;

	auto* task = smp_current_task();
	vm_space_unmap(task->current_vm_space, (virtaddr_t)addr, length);

	return 0;
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
	return smp_current_task()->cred.uid;
}

int sys_setuid(uid_t uid)
{
	auto* creds = &smp_current_task()->cred;
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
	return smp_current_task()->cred.gid;
}

int sys_setgid(gid_t gid)
{
	auto* creds = &smp_current_task()->cred;
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
	auto* task = smp_current_task();
	return task->tgid;
}

int sys_setsid()
{
	auto* task = smp_current_task();

	// already is a process group leader, good enough check?
	if(task->tgid == task->pgid)
		return -EPERM;

	task->pgid = task->tgid;
	task->sid = task->tgid;
	task->tty = nullptr;
	return 0;
}

pid_t sys_getpgid(pid_t pid)
{
	auto* task = smp_current_task();
	if(pid == 0)
		return task->pgid;

	auto* search = lookup_by_pid(pid);
	if(!search)
		return -ESRCH;

	if(search->sid != task->sid)
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

	auto* task = smp_current_task();
	if(task->tgid == task->sid)
		return -EPERM;

	if(pgid == 0)
	{
		task->pgid = task->tgid;
		return 0;
	}

	auto* pgrp_leader = get_pgrp_leader(pgid);
	if(!pgrp_leader)
		return -ESRCH;

	if(pgrp_leader->sid != task->sid)
		return -EPERM;

	task->pgid = pgid;
	return 0;
}

int sys_socket(int domain, int type, int protocol)
{
	socket_t* sock = nullptr;
	int status = socket_create(&sock, domain, type, protocol);
	if(status < 0)
		return status;

	auto s_fd = socket_open(sock);
	if(s_fd < 0)
	{
		socket_put(sock);
		return s_fd;
	}

	auto* task = smp_current_task();
	int fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] == -1)
		{
			fd = i;
			break;
		}
	}

	if(fd < 0)
	{
		vfs::close(s_fd);
		return -EMFILE;
	}
	task->open_files[fd] = s_fd;
	return fd;
}

int sys_bind(int sockfd, const sockaddr* addr, socklen_t addrlen)
{
	if(!addr || reinterpret_cast<virtaddr_t>(addr) > 0x7fffffffffff)
		return -EFAULT;

	if(sockfd < 0)
		return -EBADF;

	auto* task = smp_current_task();
	auto s_fd = task->open_files[sockfd];
	if(s_fd < 0)
		return -EBADF;

	auto& filp = vfs::get_open_fd(s_fd);
	if(!S_ISSOCK(filp.inode->mode))
		return -ENOTSOCK;

	return socket_bind((socket_t*)filp.inode->data, addr, addrlen);
}

ssize_t sys_recvfrom(int sockfd, byte* buf, size_t len, int flags, sockaddr* src_addr, socklen_t* addrlen)
{
	if(!buf || reinterpret_cast<virtaddr_t>(buf) > 0x7fffffffffff)
		return -EFAULT;

	if(reinterpret_cast<virtaddr_t>(src_addr) > 0x7fffffffffff)
		return -EFAULT;

	if(reinterpret_cast<virtaddr_t>(addrlen) > 0x7fffffffffff)
		return -EFAULT;

	if(sockfd < 0)
		return -EBADF;
	
	auto* task = smp_current_task();
	auto s_fd = task->open_files[sockfd];
	if(s_fd < 0)
		return -EBADF;

	auto& filp = vfs::get_open_fd(s_fd);
	if(!S_ISSOCK(filp.inode->mode))
		return -ENOTSOCK;

	return socket_recvfrom((socket_t*)filp.inode->data, buf, len, flags, src_addr, addrlen);	
}

ssize_t sys_sendto(int sockfd, const byte* buf, size_t len, int flags, const sockaddr* dest_addr, socklen_t addrlen)
{
	if(!buf || reinterpret_cast<virtaddr_t>(buf) > 0x7fffffffffff)
		return -EFAULT;

	if(reinterpret_cast<virtaddr_t>(dest_addr) > 0x7fffffffffff)
		return -EFAULT;

	if(sockfd < 0)
		return -EBADF;
	
	auto* task = smp_current_task();
	auto s_fd = task->open_files[sockfd];
	if(s_fd < 0)
		return -EBADF;

	auto& filp = vfs::get_open_fd(s_fd);
	if(!S_ISSOCK(filp.inode->mode))
		return -ENOTSOCK;

	return socket_sendto((socket_t*)filp.inode->data, buf, len, flags, dest_addr, addrlen);	
}

int sys_clock_gettime(clockid_t clockid, timespec* tv)
{
	if(clockid != CLOCK_REALTIME)
		return -EINVAL;

	if(!tv || reinterpret_cast<virtaddr_t>(tv) > 0x7fffffffffff)
                return -EFAULT;

	*tv = time_get_current();
	return 0;
}	

int sys_clock_nanosleep(clockid_t clockid, int flags, const timespec* t, timespec* remain)
{
	if(clockid != CLOCK_REALTIME)
		return -EINVAL;

	if(!t || reinterpret_cast<virtaddr_t>(t) > 0x7fffffffffff)
                return -EFAULT;

	if(reinterpret_cast<virtaddr_t>(remain) > 0x7fffffffffff)
                return -EFAULT;

	timespec sleep_spec;
	memcpy(&sleep_spec, t, sizeof(timespec));
	if(sleep_spec.tv_sec < 0)
		return -EINVAL;
	if(sleep_spec.tv_nsec > 999999999)
		return -EINVAL;

	return task_sleep(&sleep_spec, remain);
}

int sys_arch_prctl(int op, virtaddr_t addr)
{
	if(op != 1)
		return -EINVAL;

	auto* task = smp_current_task();
	task->tls_base = addr;
	arch_set_tls(addr);

	return 0;
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
	case SOCKET:
		ctx->rax = static_cast<uint64_t>(sys_socket((int)ctx->rdi, (int)ctx->rsi, (int)ctx->rdx));
		break;
	case BIND:
		ctx->rax = static_cast<uint64_t>(sys_bind((int)ctx->rdi, (const sockaddr*)ctx->rsi, (socklen_t)ctx->rdx));
		break;
	case RECVFROM:
		ctx->rax = static_cast<uint64_t>(sys_recvfrom((int)ctx->rdi, (byte*)ctx->rsi, (size_t)ctx->rdx, (int)ctx->rcx, (sockaddr*)ctx->r8, (socklen_t*)ctx->r9));
		break;
	case SENDTO:
		ctx->rax = static_cast<uint64_t>(sys_sendto((int)ctx->rdi, (const byte*)ctx->rsi, (size_t)ctx->rdx, (int)ctx->rcx, (const sockaddr*)ctx->r8, (socklen_t)ctx->r9));
		break;
	case CLOCK_GETTIME:
		ctx->rax = static_cast<uint64_t>(sys_clock_gettime((clockid_t)ctx->rdi, (timespec*)ctx->rsi));
		break;
	case CLOCK_NANOSLEEP:
		ctx->rax = static_cast<uint64_t>(sys_clock_nanosleep((clockid_t)ctx->rdi, (int)ctx->rsi, (const timespec*)ctx->rdx, (timespec*)ctx->rcx));
		break;
	case ARCH_PRCTL:
		ctx->rax = static_cast<uint64_t>(sys_arch_prctl((int)ctx->rdi, ctx->rsi));
		break;
	default:
		log::error("unknown syscall {:x}", uint64_t(id));
	}
}
