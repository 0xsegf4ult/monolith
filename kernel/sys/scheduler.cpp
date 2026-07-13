#include <sys/scheduler.hpp>
#include <sys/task.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <fs/vfs.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>
#include <init.hpp>
#include <list.hpp>
#include <panic.hpp>

struct sched_percpu_t
{
	task_t* idle;
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
	auto* cur_task = smp_current_cpu()->get_current_task();
	bool cur_running = (atomic_load_explicit(&cur_task->status, memory_order_relaxed) == TASK_RUNNING);

	if(list_empty(sdata.runqueue) && cur_running && cur_task != sdata.idle)
		return;

	disable_interrupts();
	spinlock_acquire(sdata.lock);

	if(cur_running)
	{
		if(cur_task != sdata.idle)
		{
			list_add_tail(sdata.runqueue, cur_task->queue_node);
			sdata.queue_entry_count++;
		}
	}

	task_t* last = cur_task;	
	task_t* next = sdata.idle;
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
				next = sdata.idle;
			}
			else
			{	
				next = list_first_entry(&steal_sdata->runqueue, task_t, queue_node);
			       	list_del(next->queue_node);	
				//log::debug("sched_cpu{}: got task {}", curcpu, next->name);
				steal_sdata->queue_entry_count--;
			}

			spinlock_release(steal_sdata->lock);
		}
	}
	else
	{	next = list_first_entry(&sdata.runqueue, task_t, queue_node);
		list_del(next->queue_node);	
		sdata.queue_entry_count--;
		spinlock_release(sdata.lock);
	}

	if(last != next)
	{
		arch_context_switch(last, next);
	}
}

void sched_yield()
{
	schedule();
}

void sched_init(uint32_t cpu_count)
{
	sched_pcpu_data = (sched_percpu_t*)kmalloc(sizeof(sched_percpu_t) * cpu_count);
	for(uint32_t i = 0; i < cpu_count; i++)
	{
		list_node_init(sched_pcpu_data[i].runqueue);
		sched_pcpu_data[i].queue_entry_count = 0;
		spinlock_init(sched_pcpu_data[i].lock);
		
		auto* idle_thread = thread_kernel_new("sched", reinterpret_cast<virtaddr_t>(i == 0 ? idle_thread_bsp_entry : idle_thread_ap_entry));
		sched_pcpu_data[i].idle = idle_thread;
	}

	num_cpus = cpu_count;
}

void sched_start_bsp()
{
	task_t boot_thr;
	arch_context_switch(&boot_thr, sched_pcpu_data[0].idle);
	panic("sched: failed to start BSP");	
}

void sched_start_ap()
{
	task_t boot_thr;
	arch_context_switch(&boot_thr, sched_pcpu_data[smp_current_cpu()->id].idle);
	panic("sched: failed to start AP");
}

void sched_add_ready(task_t* task)
{
	auto& sdata = sched_pcpu_data[smp_current_cpu()->id];

	uint64_t rflags;
	spinlock_acquire_irqsave(sdata.lock, rflags);
	list_add_tail(sdata.runqueue, task->queue_node);
	sdata.queue_entry_count++;
	spinlock_release_irqsave(sdata.lock, rflags);
}
