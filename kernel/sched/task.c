#include <sched/task.h>
#include <libk/list.h>
#include <libk/string.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <panic.h>
#include <types.h>
#include <stdatomic.h>

list_head_t g_task_list = {&g_task_list, &g_task_list};
spinlock_t g_task_list_lock = {0};

static _Atomic pid_t next_pid = 1;
static struct task* init_task = nullptr;
constexpr size_t kernel_stack_size = 0x2000;

static void kernel_stack_init(struct task* task, virtaddr_t entry)
{
	virtaddr_t stack_alloc = vmalloc(kernel_stack_size);
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
	task->tgid = task->tgid;

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
