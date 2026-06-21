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

struct sched_percpu_t
{
	thread_t* idle;
	thread_t* ready_list_head;
	thread_t* ready_list_tail;
	size_t queue_entry_count;
	spinlock_t lock;
};

static sched_percpu_t* sched_pcpu_data = nullptr;
static size_t num_cpus = 0;

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

sched_percpu_t* sched_find_most_loaded_queue()
{
	sched_percpu_t* maxq = nullptr;
	size_t maxc = 0;

	sched_percpu_t* start = sched_pcpu_data;
	for(int i = 0; i < num_cpus; i++)
	{
		if(!start || !start->ready_list_head)
			break;
		
		if(start->queue_entry_count > maxc)
		{
			maxc = start->queue_entry_count;
			maxq = start;
		}

		start++;
	}

	return maxq;
}

void schedule()
{
	auto curcpu = smp_current_cpu()->id;
	auto& sdata = sched_pcpu_data[curcpu];
	auto* current_thread = smp_current_cpu()->get_current_thread();
	if(!sdata.ready_list_head && current_thread->status == thread_status::running && current_thread != sdata.idle)
		return;

	disable_interrupts();
	spinlock_acquire(sdata.lock);

	if(current_thread->status == thread_status::running)
	{
		current_thread->status = thread_status::ready;
		if(current_thread != sdata.idle)
		{
			sdata.ready_list_tail->next = current_thread;
			sdata.ready_list_tail = current_thread;
			sdata.queue_entry_count++;
		}
	}

	thread_t* last = current_thread;	

	thread_t* thr = sdata.ready_list_head;
	if(!thr)
	{
		spinlock_release(sdata.lock);
		auto* steal_sdata = sched_find_most_loaded_queue();
		if(steal_sdata)
		{
			spinlock_acquire(steal_sdata->lock);
//			log::debug("sched_cpu{}: stealing task from other CPU queue", curcpu);
	
			thr = steal_sdata->ready_list_head;
			if(!thr)
			{
//				log::debug("sched_cpu{}: task already stolen", curcpu);
				thr = sdata.idle;
			}
			else
			{	
//				log::debug("sched_cpu{}: got task {}", curcpu, thr->name);
				steal_sdata->ready_list_head = thr->next;
				thr->next = nullptr;
				if(steal_sdata->ready_list_tail == thr)
					steal_sdata->ready_list_tail = steal_sdata->ready_list_head;

				steal_sdata->queue_entry_count--;
			}

			spinlock_release(steal_sdata->lock);
		}
		else
			thr = sdata.idle;
	}
	else
	{	
		sdata.ready_list_head = thr->next;
		thr->next = nullptr;
		if(sdata.ready_list_tail == thr)
			sdata.ready_list_tail = sdata.ready_list_head;
	
		sdata.queue_entry_count--;
		spinlock_release(sdata.lock);
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
	sched_pcpu_data = (sched_percpu_t*)kmalloc(sizeof(sched_percpu_t) * cpu_count);
	for(uint32_t i = 0; i < cpu_count; i++)
	{
		sched_pcpu_data[i].ready_list_head = nullptr;
		sched_pcpu_data[i].ready_list_tail = nullptr;
		sched_pcpu_data[i].queue_entry_count = 0;
		spinlock_init(sched_pcpu_data[i].lock);
	}

	num_cpus = cpu_count;
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

	sched_pcpu_data[0].idle = idle_thread;

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

	sched_pcpu_data[smp_current_cpu()->id].idle = idle_thread;

	thread_t boot_thr;
	arch_context_switch(&boot_thr, idle_thread);
}

void sched_add_ready(thread_t* thr)
{
	auto& sdata = sched_pcpu_data[smp_current_cpu()->id];
	thr->next = nullptr;

	uint64_t rflags;
	spinlock_acquire_irqsave(sdata.lock, rflags);
	if(!sdata.ready_list_head)
	{
		sdata.ready_list_head = thr;
		sdata.ready_list_tail = thr;
	}
	else
	{
		sdata.ready_list_tail->next = thr;
		sdata.ready_list_tail = thr;
	}
	sdata.queue_entry_count++;
	spinlock_release_irqsave(sdata.lock, rflags);
}
