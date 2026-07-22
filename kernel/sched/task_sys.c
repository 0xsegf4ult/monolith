#include <sched/task_sys.h>
#include <sched/task.h>
#include <sched/signal.h>
#include <sched/scheduler.h>
#include <sys/binfmt/binfmt.h>
#include <sys/smp.h>
#include <sys/spinlock.h>

#include <mm/mmu.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <fs/lookup.h>
#include <fs/stat.h>
#include <fs/vfs.h>

#include <libk/string.h>

#include <cpu.h>
#include <errno.h>
#include <panic.h>
#include <types.h>
#include <stdatomic.h>

#include <klog.h>

pid_t sys_spawn(const char** argv, const char** envp, uint64_t flags)
{
	if(!vm_validate_ptr(argv, sizeof(void*)))
		return -EFAULT;

	if(envp && !vm_validate_ptr(envp, sizeof(void*)))
		return -EFAULT;

	if(!vm_validate_ptr(argv[0], PATH_MAX))
		return -EFAULT;
	
	struct task* proc = process_userspace_new(argv[0], (virtaddr_t)binfmt_exec_task);
	struct task* parent = smp_current_task();
	atomic_store(&proc->parent, parent);
	
	if(flags & SPAWN_SETPGID)
		proc->pgid = proc->pid;
	else
		proc->pgid = parent->pgid;
	
	proc->sid = parent->sid;
	
	uint64_t irqflags;
	spinlock_acquire_irqsave(&parent->child_list_lock, &irqflags);
	list_add(&parent->children, &proc->sibling);
	spinlock_release_irqsave(&parent->child_list_lock, irqflags);
	
	while(argv[proc->argc])
	{
		if(!vm_validate_ptr(argv[proc->argc], PATH_MAX))
			return -EFAULT; 

		proc->argc++;
	}

	if(envp)
	{
		while(envp[proc->envc])
		{
			if(!vm_validate_ptr(envp[proc->envc], PATH_MAX))
				return -EFAULT; 

			proc->envc++;
		}
	}

	int ret = task_copy_args(proc, argv, envp);
	if(ret < 0)
	{
		task_zombify(proc);
		return ret;
	}

	proc->cwd = parent->cwd;
	ventry_ref(proc->cwd);

	proc->cred = parent->cred;
	proc->tty = parent->tty;

	for(int i = 0; i < 32; i++)
	{
		if(parent->open_files[i] >= 0)
			proc->open_files[i] = vfs_dup(parent->open_files[i]);
	}

	sched_add_ready(proc);
	return proc->pid;
}

void sys_exit(int status)
{
	struct task* task = smp_current_task();
	if(task->pid == 1)
		panic("init exited with status %d", status);

	task->return_status = status;

	task_status exp_state = TASK_RUNNING;
	if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_ZOMBIE))
	{
		exp_state = TASK_INTR_SLEEPING;
		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_ZOMBIE))
			panic("task_exit: task in invalid state %s", get_status_name(exp_state));
	}

	task_zombify(task);

	native_memory_barrier();

	uint64_t flags;
	spinlock_acquire_irqsave(&task->child_list_lock, &flags);
	struct task* parent = atomic_load_explicit(&task->parent, memory_order_acquire);

	if(parent)
		send_signal(parent, SIGCHLD);

	spinlock_release_irqsave(&task->child_list_lock, flags);
	sched_yield();
}

pid_t sys_waitpid(pid_t pid, int* status, int options)
{
	struct task* task = smp_current_task();
	bool found_child = false;
	int out_status = 0;
	pid_t ret_pid = 0;

	task_status exp_state = TASK_RUNNING;
	while(1)
	{
		uint64_t flags;
		spinlock_acquire_irqsave(&task->child_list_lock, &flags);
		if(list_empty(&task->children))
		{
			spinlock_release_irqsave(&task->child_list_lock, flags);
			atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING);
			return -ECHILD;
		}

		struct task* child;
		list_for_each_entry(child, &task->children, sibling)
		{
			spinlock_acquire(&child->child_list_lock);
			uint32_t flags = atomic_load(&child->flags);
			bool is_zombie = (atomic_load_explicit(&child->status, memory_order_relaxed) == TASK_ZOMBIE);
			if(is_zombie && (flags & TASK_CAN_REAP))
			{
				list_del(&child->sibling);
				spinlock_release(&child->child_list_lock);

				if(child->return_signal)
					out_status = child->return_signal & 0x7F;
			       	else
					out_status = (child->return_status & 0xFF) << 8;

				found_child = true;
				ret_pid = child->pid;

				task_destroy(child);
				break;
			}
			spinlock_release(&child->child_list_lock);
		}
		spinlock_release_irqsave(&task->child_list_lock, flags);

		if(found_child)
			break;

		if(signal_pending(task))
			return -EINTR;

		if(!atomic_compare_exchange_strong(&task->status, &exp_state, TASK_INTR_SLEEPING))
			panic("sys_wait: task in invalid state %s", get_status_name(exp_state));

		exp_state = TASK_INTR_SLEEPING;

		sched_yield();
		exp_state = TASK_RUNNING;
	}
	exp_state = TASK_INTR_SLEEPING;
	atomic_compare_exchange_strong(&task->status, &exp_state, TASK_RUNNING);

	if(status)
	{
		if(!vm_validate_ptr(status, sizeof(int)))
			return -EFAULT;

		*status = out_status;
	}

	return ret_pid;	
}

pid_t sys_getpid()
{
	return smp_current_task()->tgid;
}

int sys_setsid()
{
	struct task* task = smp_current_task();
	if(task->tgid == task->pgid)
		return -EPERM;

	//FIXME: tty fg pgrp starts new session?

	task->pgid = task->tgid;
	task->sid = task->tgid;
	task->tty = nullptr;
	return 0;
}

pid_t sys_getpgid(pid_t pid)
{
	struct task* task = smp_current_task();
	if(pid == 0)
		return task->pgid;

	struct task* search = lookup_by_pid(pid);
	if(!search)
		return -ESRCH;

	if(search->sid != task->sid)
		return -EPERM;

	return search->pgid;
}

int sys_setpgid(pid_t pgid)
{
	struct task* task = smp_current_task();
	if(task->tgid == task->sid)
		return -EPERM;

	if(pgid == 0)
	{
		task->pgid = task->tgid;
		return 0;
	}

	struct task* pgrp_leader = get_pgrp_leader(pgid);
	if(!pgrp_leader)
		return -ESRCH;

	if(pgrp_leader->sid != task->sid)
		return -EPERM;

	task->pgid = pgid;
	return 0;
}

int sys_chdir(const char* path)
{
	if(!vm_validate_ptr(path, PATH_MAX))
		return -EFAULT;

	struct ventry* query = nullptr;
	int status = vfs_lookup(path, &query, 0);
	if(status < 0)
		return status;

	if(!S_ISDIR(query->node->mode))
		return -ENOTDIR;

	struct task* task = smp_current_task();
	ventry_ref(query);
	ventry_put(task->cwd);
	task->cwd = query;
	return 0;
}

int sys_getcwd(char* buffer, size_t length)
{
	if(!vm_validate_ptr(buffer, length))
		return -EFAULT;

	//FIXME: absolute path
	strncpy(buffer, smp_current_task()->cwd->name, length);
	return 0;
}

uid_t sys_getuid()
{
	return smp_current_task()->cred.uid;
}

int sys_setuid(uid_t uid)
{
	cred_t* creds = &smp_current_task()->cred;
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

gid_t sys_getgid()
{
	return smp_current_task()->cred.gid;
}

int sys_setgid(gid_t gid)
{
	cred_t* creds = &smp_current_task()->cred;
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

