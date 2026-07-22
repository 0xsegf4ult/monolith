#include <sched/task.h>
#include <sched/scheduler.h>
#include <sched/signal.h>
#include <sched/waitqueue.h>

#include <dev/tty.h>

#include <fs/procfs/procfs.h>
#include <fs/vfs.h>

#include <mm/slab.h>
#include <mm/vmm.h>

#include <sys/spinlock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <libk/list.h>
#include <libk/string.h>

#include <cpu.h>
#include <errno.h>
#include <panic.h>
#include <types.h>
#include <stdatomic.h>

#include <klog.h>

list_head_t g_task_list = {&g_task_list, &g_task_list};
spinlock_t g_task_list_lock = {0};

static _Atomic pid_t next_pid = 1;
static struct task* init_task = nullptr;
constexpr size_t kernel_stack_size = 0x2000;
constexpr size_t user_stack_size = 0x4000;

static void kernel_stack_init(struct task* task, virtaddr_t entry)
{
	virtaddr_t stack_alloc = (virtaddr_t)vmalloc(kernel_stack_size);
	uint64_t* stack_ptr = (uint64_t*)(stack_alloc + kernel_stack_size);
	*(--stack_ptr) = entry;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;

	task->rsp0 = (virtaddr_t)stack_ptr;
	task->rsp0_top = task->rsp0;
}

static void user_stack_init(struct task* task)
{
	task->rsp = vm_space_map(task->current_vm_space,
	(vm_mapping_info)
	{
		.length = user_stack_size,
		.prot = PROT_READ | PROT_WRITE | PROT_USER,
		.flags = VM_FLAG_ALLOCATE,
	}) + user_stack_size;
}

static void fd_table_init(struct task* task)
{
	for(int i = 0; i < 32; i++)
		task->open_files[i] = -1;
}

struct task* task_new(const char* name, pid_t forcepid)
{
	struct task* task = kmalloc(sizeof(struct task));
	strncpy(task->name, name, 32);

	if(forcepid >= 0)
		task->pid = forcepid;
	else
	{
		task->pid = atomic_fetch_add(&next_pid, 1);
		if(task->pid == 1)
			init_task = task;
	}

	task->pgid = task->pid;
	task->sid = task->pid;
	task->tgid = task->pid;

	task->owned_vm_space = nullptr;
	task->current_vm_space = nullptr;

	atomic_store_explicit(&task->status, TASK_RUNNING, memory_order_relaxed);
	atomic_store_explicit(&task->flags, 0, memory_order_relaxed);

	task->rsp0 = 0;
	task->rsp0_top = 0;
	task->rsp = 0;

	task->context = nullptr;
	task->tls_base = 0;

	atomic_store_explicit(&task->parent, nullptr, memory_order_relaxed);
	list_node_init(&task->children);
	list_node_init(&task->sibling);
	spinlock_init(&task->child_list_lock);

	task->cred.uid = 0;
	task->cred.euid = 0;
	task->cred.suid = 0;
	task->cred.gid = 0;
	task->cred.egid = 0;
	task->cred.sgid = 0;

	task->cwd = nullptr;
	task->tty = nullptr;

	task->return_status = 0;
	task->return_signal = 0;

	task->sig_pending = 0;
	task->sig_blocked = 0;
	spinlock_init(&task->sig_lock);

	wait_queue_node_init(&task->wait, task);

	task->argc = 0;
	task->envc = 0;
	task->argv = nullptr;
	task->envp = nullptr;

	list_node_init(&task->queue_node);
	list_node_init(&task->list_node);

	task->next = nullptr;

	if(task->pid)
	{
		uint64_t flags;
		spinlock_acquire_irqsave(&g_task_list_lock, &flags);
		list_add_tail(&g_task_list, &task->list_node);
		spinlock_release_irqsave(&g_task_list_lock, flags);
	}

	return task;
}

struct task* thread_kernel_new(const char* name, virtaddr_t entry)
{
	struct task* task = task_new(name, 0);
	task->current_vm_space = vm_get_kernel_space();
	kernel_stack_init(task, entry);
	fd_table_init(task);

	return task;
}

int task_copy_args(struct task* proc, const char** argv, const char** envp)
{
	proc->argv = kmalloc(sizeof(char*) * proc->argc);
	if(!proc->argv)
		return -ENOMEM;

	if(envp)
	{
		proc->envp = kmalloc(sizeof(char*) * proc->envc);
		if(!proc->envp)
			return -ENOMEM;
	}

	// write to top of stack physical page
	vm_mapping stack_map = vm_space_get_mapping(proc->current_vm_space, proc->rsp - 0x1000);
	virtaddr_t orig_stack = stack_map.base + VM_DMAP_BASE + 0x1000;
	virtaddr_t stack = orig_stack;

	for(int i = 0; i < proc->envc; i++)
	{
		size_t len = strlen(envp[i]);
		for(ssize_t j = len; j >= 0; j--)
		{
			stack--;
			*(char*)stack = envp[i][j];
		}

		proc->envp[i] = (char*)(proc->rsp - (orig_stack - stack));
	}

	for(int i = 0; i < proc->argc; i++)
	{
		size_t len = strlen(argv[i]);
		for(ssize_t j = len; j >= 0; j--)
		{
			stack--;
			*(char*)stack = argv[i][j];
		}


		proc->argv[i] = (char*)(proc->rsp - (orig_stack - stack));
	}

	proc->rsp = align_down(proc->rsp - (orig_stack - stack), 16);

	return 0;
}

struct task* process_userspace_new(const char* name, virtaddr_t entry)
{
	struct task* task = task_new(name, -1);

	task->owned_vm_space = vm_userspace_new();
	task->current_vm_space = task->owned_vm_space;
	task->cwd = vfs_context()->root_node;

	kernel_stack_init(task, entry);
	fd_table_init(task);

	user_stack_init(task);
	task->context = cpu_context_new();

	procfs_register_process(task);
	return task;
}

void task_zombify(struct task* task)
{
	if(is_session_leader(task) && task->tty)
	{
		struct tty_device* tty = task->tty;

		uint64_t flags;
		spinlock_acquire_irqsave(&g_task_list_lock, &flags);

		struct task* cur;
		list_for_each_entry(cur, &g_task_list, list_node)
		{
			if(cur->sid == task->sid)
				cur->tty = nullptr;
		}
		spinlock_release_irqsave(&g_task_list_lock, flags);

		mutex_lock(&tty->lock);
		tty->session_id = 0;
		tty->fg_pgrp = 0;
		mutex_unlock(&tty->lock);
	}

	uint64_t flags;
	spinlock_acquire_irqsave(&init_task->child_list_lock, &flags);
	spinlock_acquire(&task->child_list_lock);
	struct task* child;
	struct task* tmp;
	list_for_each_entry_safe(child, tmp, &task->children, sibling)
	{
		spinlock_acquire(&child->child_list_lock);
		list_del(&child->sibling);

		struct task* last_parent = atomic_load_explicit(&child->parent, memory_order_relaxed);
		list_move(&init_task->children, &child->sibling);
		if(!atomic_compare_exchange_strong_explicit(&child->parent, &last_parent, init_task, memory_order_release, memory_order_relaxed))
		{
			if(last_parent != init_task)
				panic("task_zombify: could not reparent to init");
		}
		spinlock_release(&child->child_list_lock);

		bool is_zombie = (atomic_load_explicit(&child->status, memory_order_relaxed) == TASK_ZOMBIE);
		if(is_zombie)
			send_signal(atomic_load_explicit(&child->parent, memory_order_relaxed), SIGCHLD);
	}
	spinlock_release(&task->child_list_lock);
	spinlock_release_irqsave(&init_task->child_list_lock, flags);

	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] >= 0)
			vfs_close(task->open_files[i]);
	}

	if(task->cwd)
	{
		ventry_put(task->cwd);
		task->cwd = nullptr;
	}

	if(task->owned_vm_space)
		vm_space_destroy(task->owned_vm_space);

	cpu_context_destroy(task->context);
	task->context = nullptr;

	atomic_fetch_or(&task->flags, TASK_CAN_REAP);
	atomic_store(&task->status, TASK_ZOMBIE);
}

void task_destroy(struct task* task)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&g_task_list_lock, &flags);
	list_del(&task->list_node);
	spinlock_release_irqsave(&g_task_list_lock, flags);

	if(task->pid && task->pid == task->tgid)
		procfs_unregister_process(task);

	vfree((void*)(task->rsp0_top - kernel_stack_size + 56));
	kfree(task);
}

struct task* lookup_by_pid(pid_t pid)
{
	struct task* lookup = nullptr;

	uint64_t flags;
	spinlock_acquire_irqsave(&g_task_list_lock, &flags);

	struct task* cur;
	list_for_each_entry(cur, &g_task_list, list_node)
	{
		if(cur->pid == pid)
		{
			lookup = cur;
			break;
		}
	}
	spinlock_release_irqsave(&g_task_list_lock, flags);
	return lookup;
}

struct task* get_pgrp_leader(pid_t pgrp)
{
	struct task* lookup = nullptr;

	uint64_t flags;
	spinlock_acquire_irqsave(&g_task_list_lock, &flags);

	struct task* cur;
	list_for_each_entry(cur, &g_task_list, list_node)
	{
		if(cur->pid == pgrp)
		{
			lookup = cur;
			break;
		}
	}
	spinlock_release_irqsave(&g_task_list_lock, flags);

	return lookup;
}
