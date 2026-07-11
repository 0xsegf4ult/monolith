#include <sys/thread.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <arch/x86_64/context.hpp>

#include <dev/tty.hpp>

#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>

#include <fs/vfs.hpp>
#include <fs/procfs/procfs.hpp>

#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <sys/executable.hpp>
#include <sys/scheduler.hpp>
#include <sys/spinlock.hpp>
#include <sys/signal.hpp>
#include <sys/waitqueue.hpp>
#include <panic.hpp>
#include <list.hpp>

static uint32_t next_pid = 1;

list_head_t g_thread_list = {&g_thread_list, &g_thread_list};
spinlock_t g_thread_list_lock;

static thread_t* init_task = nullptr;

typedef void (*entry_function_t)();

void thread_entry_stub()
{
	enable_interrupts();
	auto self = smp_current_cpu()->get_current_thread();

/*	log::debug("started thread {}", self->name);
	log::debug("entrypoint {:#x}", self->entry);
	log::debug("rsp0 {:#x} rsp3 {:#x}", self->rsp0, self->rsp);
*/
	if(!self->rsp)
	{
		(reinterpret_cast<entry_function_t>(self->entry))();
	}
	else
	{
		arch_switch_to_usermode(self->rsp, self->entry);
	}
}

constexpr size_t kernel_stack_size = 0x2000;
constexpr size_t user_stack_size = 0x4000;

thread_t* create_thread(const char* name, const char** argv, bool is_user)
{
	auto* thread = (thread_t*)kmalloc(sizeof(thread_t));
	strncpy(thread->name, name, 32);
	thread->pid = next_pid++;
	if(thread->pid == 1)
		init_task = thread;

	thread->pgid = thread->pid;
	thread->sid = thread->pid;
	thread->vm_space = (address_space*)kmalloc(sizeof(address_space));
	thread->vm_space->init();
	clone_kernel_vm(thread->vm_space);
	thread->status = THREAD_RUNNING;
	thread->flags = 0;
	thread->next = nullptr;

	//get_kernel_vmspace()->dump_objects();
	auto kstack_alloc = vmalloc(kernel_stack_size, vm_write);
	auto* stack_ptr = reinterpret_cast<uint64_t*>(kstack_alloc + kernel_stack_size);
	*(--stack_ptr) = reinterpret_cast<virtaddr_t>(thread_entry_stub);
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;

	thread->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
	thread->rsp = 0;
	thread->rsp0_top = thread->rsp0;
	thread->parent = nullptr;
	
	list_node_init(thread->children);
	list_node_init(thread->sibling);
	spinlock_init(thread->child_list_lock);

	thread->cred.uid = 0;
	thread->cred.euid = 0;
	thread->cred.suid = 0;
	thread->cred.gid = 0;
	thread->cred.egid = 0;
	thread->cred.sgid = 0;
	thread->cwd = vfs::get_root_dentry();
	
	thread->tty = nullptr;
	thread->return_status = 0;
	thread->return_signal = 0;

	thread->sig_pending = 0;
	thread->sig_blocked = 0;
	spinlock_init(thread->sig_lock);

	wait_queue_node_init(thread->wait);
	thread->wait.thread = thread;

	list_node_init(thread->queue_node);
	list_node_init(thread->list_node);

	procfs_register_thread(thread);

	for(int i = 0; i < 32; i++)
		thread->open_files[i] = -1;

	if(is_user)
	{
		int argc = 0;
		size_t argv_size = 0;
		for(argc = 0; argv[argc]; ++argc)
			argv_size += string_length(argv[argc]) + 1;

		thread->rsp = thread->vm_space->alloc(user_stack_size, vm_write | vm_user | vm_present) + user_stack_size - 8;
		auto rsp_phys = thread->vm_space->get_mapping(thread->rsp) + ((user_stack_size - 8) & 0xfff) + mm::direct_mapping_offset;
		auto rsp_phys_orig = rsp_phys;

		auto rsp_argv_offset = rsp_phys - argv_size;
		size_t strings_align = rsp_argv_offset % 16;
		rsp_argv_offset -= strings_align;

		auto rsp_argv_array_offset = thread->rsp - (rsp_phys_orig - rsp_argv_offset) - (argc * 8);

		for(int i = argc - 1; i >= 0; --i)
		{
			auto len = string_length(argv[i]) + 1;
			rsp_phys -= len;
			rsp_argv_offset -= 8;
			memcpy((void*)rsp_phys, argv[i], len);
			auto str_offset = thread->rsp - (rsp_phys_orig - rsp_phys);
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
		thread->rsp -= stack_reserve;
	}

	uint64_t rflags;
	spinlock_acquire_irqsave(g_thread_list_lock, rflags);
	list_add_tail(g_thread_list, thread->list_node);
	spinlock_release_irqsave(g_thread_list_lock, rflags);

	return thread;	
}

void thread_zombify(thread_t* thr)
{
	thread_set_zombie(thr);

	if(thr->pid == thr->sid && thr->tty)
	{
		auto* tty = thr->tty;
		
		uint64_t rflags;
		spinlock_acquire_irqsave(g_thread_list_lock, rflags);

		thread_t* cur;
		list_for_each_entry(cur, g_thread_list, list_node)
		{
			if(cur->sid == thr->sid)
				cur->tty = nullptr;
		}
		spinlock_release_irqsave(g_thread_list_lock, rflags);

		mutex_lock(tty->lock);
		tty->session_id = 0;
		tty->fg_pgrp = 0;
		mutex_unlock(tty->lock);
	}
	
	uint64_t rflags;
	spinlock_acquire_irqsave(init_task->child_list_lock, rflags);
	spinlock_acquire(thr->child_list_lock);
	thread_t* child;
	thread_t* tmp;
	list_for_each_entry_safe(child, tmp, thr->children, sibling)
	{
		spinlock_acquire(child->child_list_lock);
		list_del(child->sibling);

		thread_t* last_parent = child->parent;
		list_move(init_task->children, child->sibling);
		if(!atomic_compare_exchange_strong_explicit(&child->parent, &last_parent, init_task, memory_order_release, memory_order_relaxed))
		{
			panic("thread_zombify: could not reparent to init, current parent not expected");
		}
		spinlock_release(child->child_list_lock);

		if(thread_is_zombie(child))
		{
			send_signal(child->parent, SIGCHLD);
		}
	}
	spinlock_release(thr->child_list_lock);
	spinlock_release_irqsave(init_task->child_list_lock, rflags);
	
	for(int i = 0; i < 32; i++)
	{
		if(thr->open_files[i] >= 0)
			vfs::close(thr->open_files[i]);
	}

	if(thr->cwd)
		ventry_put(thr->cwd);

	thr->cwd = nullptr;

	thr->vm_space->destroy();
	kfree(thr->vm_space);

	atomic_fetch_or(&thr->flags, THREAD_CAN_REAP);
}

void destroy_thread(thread_t* thr)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(g_thread_list_lock, rflags);
	list_del(thr->list_node);
	spinlock_release_irqsave(g_thread_list_lock, rflags);

	procfs_unregister_thread(thr);
	vmfree(thr->rsp0_top - kernel_stack_size + 56);
	kfree(thr);
}

thread_t* thread_lookup_by_pid(pid_t pid)
{
	thread_t* lookup = nullptr;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_thread_list_lock, rflags);

	thread_t* cur;
	list_for_each_entry(cur, g_thread_list, list_node)
	{
		if(cur->pid == pid)
		{
			lookup = cur;
			break;
		}
	}
	spinlock_release_irqsave(g_thread_list_lock, rflags);
	return lookup;
}

thread_t* get_pgrp_leader(pid_t pgrp)
{
	thread_t* lookup = nullptr;

	uint64_t rflags;
	spinlock_acquire_irqsave(g_thread_list_lock, rflags);

	thread_t* cur;
	list_for_each_entry(cur, g_thread_list, list_node)
	{
		if(cur->pid == pgrp)
		{
			lookup = cur;
			break;
		}
	}
	spinlock_release_irqsave(g_thread_list_lock, rflags);

	return lookup;
}

void threading_init()
{
	list_node_init(g_thread_list);
	spinlock_init(g_thread_list_lock);

	sleep_queue_head = nullptr;
	spinlock_init(sleep_queue_lock);
}
