#include <sched/scheduler.h>
#include <sched/task.h>

#include <mm/slab.h>

#include <sys/smp.h>
#include <sys/spinlock.h>
#include <sys/timer.h>

#include <irq.h>

#include <libk/list.h>
#include <init.h>
#include <panic.h>
#include <types.h>

#include <klog.h>

typedef struct
{
	struct task* idle;
	list_head_t runqueue;
	_Atomic size_t queue_entry_count;
	spinlock_t lock;
} sched_percpu_t;

static sched_percpu_t* sched_pcpu_data = nullptr;
static size_t num_cpus = 0;

static void idle_thread_bsp_entry(void* arg)
{
	local_irq_enable();

	kernel_main();

	for(;;)
		native_cpu_idle();
}

static void idle_thread_ap_entry(void* arg)
{
	local_irq_enable();

	for(;;)
		native_cpu_idle();
}

void sched_add_ready(struct task* task)
{
	sched_percpu_t* sdata = &sched_pcpu_data[smp_current_cpu()];

	uint64_t flags;
	spinlock_acquire_irqsave(&sdata->lock, &flags);
	list_add_tail(&sdata->runqueue, &task->queue_node);
	atomic_fetch_add_explicit(&sdata->queue_entry_count, 1, memory_order_release);
	spinlock_release_irqsave(&sdata->lock, flags);
}

static sched_percpu_t* sched_find_most_loaded_queue()
{
	sched_percpu_t* maxq = nullptr;
	size_t maxc = 0;

	sched_percpu_t* start = sched_pcpu_data;
	for(size_t i = 0; i < num_cpus; i++)
	{
		if(!start)
			break;

		size_t queue_size = atomic_load_explicit(&start->queue_entry_count, memory_order_acquire);
		if(!queue_size)
			continue;

		if(queue_size > maxc)
		{
			maxc = queue_size;
			maxq = start;
		}
		
		start++;
	}

	return maxq;
}

void schedule()
{
	uint32_t curcpu = smp_current_cpu();
	sched_percpu_t* sdata = &sched_pcpu_data[curcpu];
	struct task* cur_task = smp_current_task();
	bool cur_running = (atomic_load_explicit(&cur_task->status, memory_order_relaxed) == TASK_RUNNING);
	size_t queue_size = atomic_load_explicit(&sdata->queue_entry_count, memory_order_acquire);
	if(!queue_size && cur_running && cur_task != sdata->idle)
		return;

	local_irq_disable();
	spinlock_acquire(&sdata->lock);

	struct task* last = cur_task;
	struct task* next = sdata->idle;
	queue_size = atomic_load_explicit(&sdata->queue_entry_count, memory_order_acquire);
	if(!queue_size)
	{
		spinlock_release(&sdata->lock);
		sched_percpu_t* steal_sdata = sched_find_most_loaded_queue();
		if(steal_sdata)
		{
			spinlock_acquire(&steal_sdata->lock);

			size_t steal_queuesize = atomic_load_explicit(&steal_sdata->queue_entry_count, memory_order_acquire);
			if(steal_queuesize)
			{
				next = list_first_entry(&steal_sdata->runqueue, struct task, queue_node);
				list_del(&next->queue_node);
				atomic_fetch_add_explicit(&steal_sdata->queue_entry_count, -1, memory_order_release);
			}

			spinlock_release(&steal_sdata->lock);
		}
	}
	else
	{
		next = list_first_entry(&sdata->runqueue, struct task, queue_node);
		list_del(&next->queue_node);
		atomic_fetch_add_explicit(&sdata->queue_entry_count, -1, memory_order_release);
		spinlock_release(&sdata->lock);
	}
	
	if(cur_running && cur_task != sdata->idle)
	{
		list_add_tail(&sdata->runqueue, &cur_task->queue_node);
		atomic_fetch_add_explicit(&sdata->queue_entry_count, 1, memory_order_release);
	}


	if(!next->rsp0)
		panic("ctx switch to no stack %p %d %s\n", next, next->pid, next->name);

	if(last != next)
		native_context_switch(last, next);
}

void sched_yield()
{
	schedule();
}

void sched_init(uint32_t cpu_count)
{
	sched_pcpu_data = kmalloc(sizeof(sched_percpu_t) * cpu_count);
	for(uint32_t i = 0; i < cpu_count; i++)
	{
		list_node_init(&sched_pcpu_data[i].runqueue);
		atomic_store(&sched_pcpu_data[i].queue_entry_count, 0);
		spinlock_init(&sched_pcpu_data[i].lock);

		struct task* idle_thread = thread_kernel_new("sched", (virtaddr_t)(i == 0 ? idle_thread_bsp_entry : idle_thread_ap_entry));
		sched_pcpu_data[i].idle = idle_thread;
	}

	num_cpus = cpu_count;
}

void sched_start_cpu()
{
	timer_start();

	struct task boot_thr;
	boot_thr.context = nullptr;
	boot_thr.current_vm_space = nullptr;

	native_context_switch(&boot_thr, sched_pcpu_data[smp_current_cpu()].idle);
}
