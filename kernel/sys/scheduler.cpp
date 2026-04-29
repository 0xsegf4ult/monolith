#include <sys/scheduler.hpp>
#include <sys/thread.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <fs/vfs.hpp>
#include <mm/address_space.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>
#include <lib/types.hpp>
#include <init/init.hpp>

static thread_t** idle_threads = nullptr;
static thread_t* ready_list_head = nullptr;
static thread_t* ready_list_tail = nullptr;

void idle_thread_bsp_entry(void* arg)
{
	enable_interrupts();
	
	kernel_main();

	for(;;)
	{
		asm volatile("hlt");
	}
}

void idle_thread_ap_entry(void* arg)
{
	enable_interrupts();

	for(;;)
	{
		asm volatile("hlt");
	}
}

void schedule()
{
	auto curcpu = smp_current_cpu()->id;
	if(curcpu != 0)
	{
		return;
	}

	auto* current_thread = smp_current_cpu()->get_current_thread();
	if(!ready_list_head && current_thread->status == thread_status::running)
		return;

	disable_interrupts();

	auto* idle_thread = idle_threads[curcpu];

	if(current_thread->status == thread_status::running)
	{
		current_thread->status = thread_status::ready;
		if(current_thread != idle_thread)
		{
			ready_list_tail->next = current_thread;
			ready_list_tail = current_thread;
		}
	}

	thread_t* last = current_thread;	

	thread_t* thr = ready_list_head;
	if(!thr)
		thr = idle_thread;
	else
	{
		ready_list_head = thr->next;
		thr->next = nullptr;
		if(ready_list_tail == thr)
			ready_list_tail = ready_list_head;
	}

	thr->status = thread_status::running; 
	arch_context_switch(last, thr);
}

void sched_block(thread_status status)
{
	auto* current_thread = smp_current_cpu()->get_current_thread();
	current_thread->status = status;
	schedule();
}

void sched_unblock(thread_t* thr)
{
	thr->status = thread_status::ready;
	sched_add_ready(thr);
}

void sched_init(uint32_t cpu_count)
{
	idle_threads = (thread_t**)kmalloc(sizeof(thread_t*) * cpu_count);
}

void sched_start_bsp()
{
	auto* idle_thread = (thread_t*)kmalloc(sizeof(thread_t));
        strncpy(idle_thread->name, "sched", 32);
        idle_thread->pid = 0;
        idle_thread->vm_space = get_kernel_vmspace();
        idle_thread->status = thread_status::ready;
	idle_thread->next = nullptr;

        auto stack_alloc = pmm_allocate() + mm::direct_mapping_offset;
        auto* stack_ptr = reinterpret_cast<uint64_t*>(stack_alloc + 0x1000);
        *(--stack_ptr) = reinterpret_cast<virtaddr_t>(idle_thread_bsp_entry);
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = 0;

        idle_thread->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
        idle_thread->rsp = 0;
	idle_thread->rsp0_top = idle_thread->rsp0;

	idle_thread->cwd = vfs::get_root_dentry();

	idle_thread->status = thread_status::running;

	idle_threads[0] = idle_thread;

	thread_t boot_thr;
	arch_context_switch(&boot_thr, idle_thread);
}

void sched_start_ap()
{
	auto* idle_thread = (thread_t*)kmalloc(sizeof(thread_t));
	strncpy(idle_thread->name, "sched", 32);
	idle_thread->pid = 0;
	idle_thread->vm_space = get_kernel_vmspace();
	idle_thread->status = thread_status::ready;
	idle_thread->next = nullptr;

	auto stack_alloc = pmm_allocate() + mm::direct_mapping_offset;
	auto* stack_ptr = reinterpret_cast<uint64_t*>(stack_alloc + 0x1000);
	*(--stack_ptr) = reinterpret_cast<virtaddr_t>(idle_thread_ap_entry);
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;
	*(--stack_ptr) = 0;

	idle_thread->rsp0 = reinterpret_cast<virtaddr_t>(stack_ptr);
	idle_thread->rsp = 0;
	idle_thread->rsp0_top = idle_thread->rsp0;

	idle_thread->cwd = vfs::get_root_dentry();

	idle_thread->status = thread_status::running;

	idle_threads[smp_current_cpu()->id] = idle_thread;

	thread_t boot_thr;
	arch_context_switch(&boot_thr, idle_thread);
}

void sched_add_ready(thread_t* thr)
{
	if(!ready_list_head)
	{
		ready_list_head = thr;
		ready_list_tail = thr;
	}
	else
	{
		ready_list_tail->next = thr;
		ready_list_tail = thr;
	}
}

void sched_dump_state()
{
	auto* current_thread = smp_current_cpu()->get_current_thread();
	log::debug("SCHEDULER STATE: ");
	generic_log("current: {}", current_thread->name);
	generic_log("ready_list");
	auto* r_cur = ready_list_head;
	while(r_cur)
	{
		generic_log(" -> {}", r_cur->name);
		r_cur = r_cur->next;
	}
}
