#include <sys/task.hpp>
#include <sys/executable.hpp>
#include <sys/scheduler.hpp>
#include <sys/spinlock.hpp>
#include <sys/smp.hpp>
#include <sys/signal.hpp>
#include <sys/time.hpp>
#include <sys/waitqueue.hpp>

#include <arch/generic.hpp>

#include <dev/tty.hpp>

#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>

#include <fs/vfs.hpp>
#include <fs/procfs/procfs.hpp>

#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <panic.hpp>
#include <list.hpp>

static atomic_int next_pid = 1;

list_head_t g_task_list = {&g_task_list, &g_task_list};
spinlock_t g_task_list_lock;

list_head_t sleep_queue_head;
spinlock_t sleep_queue_lock;

static task_t* init_task = nullptr;

typedef void (*entry_function_t)();

void task_entry_stub()
{
	auto self = smp_current_task();
	arch_switch_to_usermode(self->rsp, self->entry);
}

constexpr size_t kernel_stack_size = 0x2000;
constexpr size_t user_stack_size = 0x4000;

static void kernel_stack_init(task_t* task, virtaddr_t entry)
{
	auto stack_alloc = vmalloc(kernel_stack_size);
       	auto* stack_ptr = reinterpret_cast<uint64_t*>(stack_alloc + kernel_stack_size);
	*(--stack_ptr) = entry; // RIP
	*(--stack_ptr) = 0; // RBP
	*(--stack_ptr) = 0; // RBX
	*(--stack_ptr) = 0; // R12
	*(--stack_ptr) = 0; // R13
	*(--stack_ptr) = 0; // R14
	*(--stack_ptr) = 0; // R15

	task->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
	//FIXME: try setting to stack_alloc
	task->rsp0_top = task->rsp0;
}

static void user_stack_init(task_t* task)
{
	task->rsp = vm_space_map(task->current_vm_space, 
	{
		.length = user_stack_size, 
		.prot = PROT_READ | PROT_WRITE | PROT_USER,
	       	.flags = VM_FLAG_ALLOCATED
	}) + user_stack_size - 8;
}

static void fd_table_init(task_t* task)
{	
	for(int i = 0; i < 32; i++)
		task->open_files[i] = -1;
}

static void user_copy_args(task_t* task, const char** argv)
{
	int argc = 0;
	size_t argv_size = 0;
	for(argc = 0; argv[argc]; ++argc)
		argv_size += string_length(argv[argc]) + 1;

	auto rsp_phys = vm_space_get_mapping(task->current_vm_space, task->rsp).base + ((user_stack_size - 8) & 0xfff) + VM_DMAP_BASE;
	auto rsp_phys_orig = rsp_phys;

	auto rsp_argv_offset = rsp_phys - argv_size;
	size_t strings_align = rsp_argv_offset % 16;
	rsp_argv_offset -= strings_align;

	auto rsp_argv_array_offset = task->rsp - (rsp_phys_orig - rsp_argv_offset) - (argc * 8);

	for(int i = argc - 1; i >= 0; --i)
	{
		auto len = string_length(argv[i]) + 1;
		rsp_phys -= len;
		rsp_argv_offset -= 8;
		memcpy((void*)rsp_phys, argv[i], len);
		auto str_offset = task->rsp - (rsp_phys_orig - rsp_phys);
		memcpy((void*)rsp_argv_offset, &str_offset, 8); 
	}
	rsp_phys -= (strings_align + argc * 8);	
	int array_align = rsp_phys % 16;
	rsp_phys -= array_align;
	rsp_phys -= 8;
	memcpy((void*)rsp_phys, &rsp_argv_array_offset, sizeof(const char**));
	rsp_phys -= 8;
	memcpy((void*)rsp_phys, &argc, sizeof(int));

	size_t stack_reserve = argv_size + argc * sizeof(const char*) + sizeof(const char**) + sizeof(uint64_t) + strings_align + array_align;
	task->rsp -= stack_reserve;
}

task_t* task_new(const char* name, pid_t forcepid)
{
	auto* task = (task_t*)kmalloc(sizeof(task_t));
	strncpy(task->name, name, 32);
	
	if(forcepid >= 0)
	{
		task->pid = forcepid;
	}
	else
	{
		task->pid = atomic_fetch_add(&next_pid, 1);
		if(task->pid == 1)
			init_task = task;
	}

	task->pgid = task->pid;
	task->sid = task->sid;
	task->tgid = task->pid;

	task->owned_vm_space = nullptr;
	task->current_vm_space = nullptr;
	atomic_store_explicit(&task->status, TASK_RUNNING, memory_order_relaxed);
	atomic_store_explicit(&task->flags, 0, memory_order_relaxed);
	
	task->rsp0 = 0;
	task->rsp0_top = 0;
	task->rsp = 0;
	
	atomic_store_explicit(&task->parent, nullptr, memory_order_relaxed);
	list_node_init(task->children);
	list_node_init(task->sibling);

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
	spinlock_init(task->sig_lock);
	task->tls_base = 0;

	wait_queue_node_init(task->wait);
	task->wait.task = task;


	task->context = nullptr;

	list_node_init(task->queue_node);
	list_node_init(task->list_node);

	//FIXME: get rid of this
	task->next = nullptr;

	if(task->pid)
	{	
		uint64_t rflags;
		spinlock_acquire_irqsave(g_task_list_lock, rflags);
		list_add_tail(g_task_list, task->list_node);
		spinlock_release_irqsave(g_task_list_lock, rflags);
	}

	return task;
}

task_t* thread_kernel_new(const char* name, virtaddr_t entry)
{
	auto* task = task_new(name, 0);
	task->current_vm_space = vm_get_kernel_space();
	kernel_stack_init(task, entry);
	fd_table_init(task);

	return task;
}

task_t* process_userspace_new(const char* name, const char** argv)
{
	auto* task = task_new(name, -1);
	
	task->owned_vm_space = vm_userspace_new(); 
	task->current_vm_space = task->owned_vm_space;
	task->cwd = vfs::get_root_dentry();

	kernel_stack_init(task, reinterpret_cast<virtaddr_t>(task_entry_stub));
	fd_table_init(task);

	user_stack_init(task);
	user_copy_args(task, argv);
	task->context = arch_context_new();

	procfs_register_process(task);
	return task;
}

void task_zombify(task_t* task)
{
	if(is_session_leader(task) && task->tty)
	{
		auto* tty = task->tty;
		
		uint64_t rflags;
		spinlock_acquire_irqsave(g_task_list_lock, rflags);

		task_t* cur;
		list_for_each_entry(cur, g_task_list, list_node)
		{
			if(cur->sid == task->sid)
				cur->tty = nullptr;
		}
		spinlock_release_irqsave(g_task_list_lock, rflags);

		mutex_lock(tty->lock);
		tty->session_id = 0;
		tty->fg_pgrp = 0;
		mutex_unlock(tty->lock);
	}
	
	uint64_t rflags;
	spinlock_acquire_irqsave(init_task->child_list_lock, rflags);
	spinlock_acquire(task->child_list_lock);
	task_t* child;
	task_t* tmp;
	list_for_each_entry_safe(child, tmp, task->children, sibling)
	{
		spinlock_acquire(child->child_list_lock);
		list_del(child->sibling);

		task_t* last_parent = child->parent;
		list_move(init_task->children, child->sibling);
		if(!atomic_compare_exchange_strong_explicit(&child->parent, &last_parent, init_task, memory_order_release, memory_order_relaxed))
		{
			panic("task_zombify: could not reparent to init, current parent not expected");
		}
		spinlock_release(child->child_list_lock);

		bool is_zombie = (atomic_load_explicit(&child->status, memory_order_relaxed) == TASK_ZOMBIE); 
		if(is_zombie)
		{
			send_signal(child->parent, SIGCHLD);
		}
	}
	spinlock_release(task->child_list_lock);
	spinlock_release_irqsave(init_task->child_list_lock, rflags);
	
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] >= 0)
			vfs::close(task->open_files[i]);
	}

	if(task->cwd)
	{
		ventry_put(task->cwd);
		task->cwd = nullptr;
	}

	if(task->owned_vm_space)
		vm_space_destroy(task->owned_vm_space);

	arch_context_destroy(task->context);
	task->context = nullptr;

	atomic_fetch_or(&task->flags, TASK_CAN_REAP);
	atomic_store(&task->status, TASK_ZOMBIE);
}

void task_destroy(task_t* task)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(g_task_list_lock, rflags);
	list_del(task->list_node);
	spinlock_release_irqsave(g_task_list_lock, rflags);

	if(task->pid && task->pid == task->tgid)
		procfs_unregister_process(task);

	vfree(task->rsp0_top - kernel_stack_size + 56);
	kfree(task);
}

task_t* lookup_by_pid(pid_t pid)
{
	task_t* lookup = nullptr;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_task_list_lock, rflags);

	task_t* cur;
	list_for_each_entry(cur, g_task_list, list_node)
	{
		if(cur->pid == pid)
		{
			lookup = cur;
			break;
		}
	}
	spinlock_release_irqsave(g_task_list_lock, rflags);
	return lookup;
}

task_t* get_pgrp_leader(pid_t pgrp)
{
	task_t* lookup = nullptr;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_task_list_lock, rflags);

	task_t* cur;
	list_for_each_entry(cur, g_task_list, list_node)
	{
		if(cur->pid == pgrp)
		{
			lookup = cur;
			break;
		}
	}
	spinlock_release_irqsave(g_task_list_lock, rflags);

	return lookup;
}

void task_init()
{
	list_node_init(g_task_list);
	spinlock_init(g_task_list_lock);

	list_node_init(sleep_queue_head);
	spinlock_init(sleep_queue_lock);
}

struct task_sleep_desc
{
	task_t* task;
	uint64_t delta;
	list_node_t list_node;
};

int task_sleep(const timespec* spec, timespec* remain)
{
	auto* task = smp_current_task();
	uint64_t time_ns = spec->tv_sec * 1000000000 + spec->tv_nsec;
	uint64_t time_acc = 0;

	task_sleep_desc sleep_desc
	{
		.task = task,
		.delta = time_ns
	};
	list_node_init(sleep_desc.list_node);
	
	uint64_t rflags;
	spinlock_acquire_irqsave(sleep_queue_lock, rflags);

	task_sleep_desc* cur;
	list_for_each_entry(cur, sleep_queue_head, list_node)
	{
		if(sleep_desc.delta < cur->delta)
			break;
		
		sleep_desc.delta -= cur->delta;
	}

	if(list_empty(sleep_queue_head))
		list_add(sleep_queue_head, sleep_desc.list_node);
	else
	{
		cur->delta -= sleep_desc.delta; 
		list_add_tail(sleep_desc.list_node, cur->list_node);
	}

	spinlock_release_irqsave(sleep_queue_lock, rflags);

	task_status exp_state = TASK_RUNNING;
	atomic_compare_exchange_strong(&task->status, &exp_state, TASK_INTR_SLEEPING);
	sched_yield();

	log::debug("timer wakeup");
	spinlock_acquire_irqsave(sleep_queue_lock, rflags);
	list_del(sleep_desc.list_node);
	spinlock_release_irqsave(sleep_queue_lock, rflags);

	// FIXME: blows up when stopped with ^C
	// check here if interrupted
	// unlink self from sleep list
	// store remaining time into *remain
	// return EINTR

	return 0;
}

void task_tick_sleepers(uint64_t ns)
{
	//FIXME: hack
	if(!init_task)
		return;

	uint64_t rflags;
	spinlock_acquire_irqsave(sleep_queue_lock, rflags);
	
	task_sleep_desc* tmp;
	task_sleep_desc* cur;
	list_for_each_entry_safe(cur, tmp, sleep_queue_head, list_node)
	{
		if(cur->delta <= ns)
		{
			ns -= cur->delta;
			cur->delta = 0;

			list_del(cur->list_node);

			task_status exp_status = TASK_INTR_SLEEPING;
			if(!atomic_compare_exchange_strong(&cur->task->status, &exp_status, TASK_RUNNING))
			{
				log::debug("failed to wake sleeping task");
				break;
			}

			sched_add_ready(cur->task);
		}
		else
		{
			cur->delta -= ns;
			ns = 0;
			break;
		}

		if(!ns)
			break;
	}
	
	spinlock_release_irqsave(sleep_queue_lock, rflags);
}
