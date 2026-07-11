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
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>
#include <init.hpp>
#include <list.hpp>

struct sched_percpu_t
{
	thread_t* idle;
	list_head_t runqueue;
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
		if(!start)
			break;

		if(list_empty(start->runqueue))
			continue;
		
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
	bool cur_running = thread_running(current_thread);
	if(list_empty(sdata.runqueue) && cur_running && current_thread != sdata.idle)
		return;

	disable_interrupts();
	spinlock_acquire(sdata.lock);

	if(cur_running)
	{
		if(current_thread != sdata.idle)
		{
			//log::debug("preempt: [{}] going into cpu{} RQ", current_thread->name, curcpu);
			list_add_tail(sdata.runqueue, current_thread->queue_node);
			sdata.queue_entry_count++;
		}
	}

	thread_t* last = current_thread;	

	thread_t* thr = sdata.idle;
	if(list_empty(sdata.runqueue))
	{
		spinlock_release(sdata.lock);
		auto* steal_sdata = sched_find_most_loaded_queue();
		if(steal_sdata)
		{
			spinlock_acquire(steal_sdata->lock);
//			log::debug("sched_cpu{}: stealing task from other CPU queue", curcpu);
	
			if(list_empty(steal_sdata->runqueue))
			{
//				log::debug("sched_cpu{}: task already stolen", curcpu);
				thr = sdata.idle;
			}
			else
			{	
				thr = list_first_entry(&steal_sdata->runqueue, thread_t, queue_node);
			       	list_del(thr->queue_node);	
				//log::debug("sched_cpu{}: got task {}", curcpu, thr->name);
				steal_sdata->queue_entry_count--;
			}

			spinlock_release(steal_sdata->lock);
		}
		else
			thr = sdata.idle;
	}
	else
	{	thr = list_first_entry(&sdata.runqueue, thread_t, queue_node);
		list_del(thr->queue_node);	
		sdata.queue_entry_count--;
		spinlock_release(sdata.lock);
	}

	thread_set_running(thr);
	if(last != thr)
	{
		//log::debug("arch_context_switch {:#x}[{}] -> {:#x}[{}]", last, last->name, thr, thr ? thr->name : "?");
		arch_context_switch(last, thr);
	}
}

void sched_yield()
{
	schedule();
}

void sched_unblock(thread_t* thr)
{
	thread_set_running(thr);
	sched_add_ready(thr);
}

void sched_init(uint32_t cpu_count)
{
	sched_pcpu_data = (sched_percpu_t*)kmalloc(sizeof(sched_percpu_t) * cpu_count);
	for(uint32_t i = 0; i < cpu_count; i++)
	{
		list_node_init(sched_pcpu_data[i].runqueue);
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
        idle_thread->status = THREAD_RUNNING;
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
	idle_thread->status = THREAD_RUNNING;
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

	sched_pcpu_data[smp_current_cpu()->id].idle = idle_thread;

	thread_t boot_thr;
	arch_context_switch(&boot_thr, idle_thread);
}

void sched_add_ready(thread_t* thr)
{
	auto& sdata = sched_pcpu_data[smp_current_cpu()->id];

	uint64_t rflags;
	spinlock_acquire_irqsave(sdata.lock, rflags);
	list_add_tail(sdata.runqueue, thr->queue_node);
	sdata.queue_entry_count++;
	spinlock_release_irqsave(sdata.lock, rflags);
}
