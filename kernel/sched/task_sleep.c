#include <sched/task_sleep.h>
#include <sched/scheduler.h>
#include <sched/task.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <sys/clock.h>
#include <sys/smp.h>
#include <libk/list.h>

#include <irq.h>
#include <errno.h>
#include <types.h>
#include <stdatomic.h>

typedef struct
{
	list_head_t queue;
} pcpu_sleep_queue;

typedef struct
{
	struct task* task;
	uint64_t delta;
	list_node_t list_node;
} task_sleep_desc;

static pcpu_sleep_queue* pcpu_data = nullptr;

int sys_clock_nanosleep(clockid_t clock, int flags, const struct timespec* tv, struct timespec* rem)
{
	if(clock != CLOCK_REALTIME)
		return -EINVAL;

	if(!vm_validate_ptr(tv, sizeof(struct timespec)))
		return -EFAULT;


	struct task* task = smp_current_task();
	uint64_t time_ns = tv->tv_sec * 1000000000 + tv->tv_nsec;
	uint64_t time_acc = 0;

	task_sleep_desc sleep_desc = 
	{
		.task = task,
		.delta = time_ns
	};
	list_node_init(&sleep_desc.list_node);

	uint64_t irq_flags = local_irq_save();

	task_sleep_desc* cur;
	pcpu_sleep_queue* data = &pcpu_data[smp_current_cpu()];
	list_for_each_entry(cur, &data->queue, list_node)
	{
		if(sleep_desc.delta < cur->delta)
			break;

		sleep_desc.delta -= cur->delta;
	}

	if(list_empty(&data->queue))
		list_add(&data->queue, &sleep_desc.list_node);
	else
	{
		cur->delta -= sleep_desc.delta;
		list_add_tail(&sleep_desc.list_node, &cur->list_node);
	}

	task_status exp_state = TASK_RUNNING;
	atomic_compare_exchange_strong(&task->status, &exp_state, TASK_INTR_SLEEPING);
	local_irq_restore(irq_flags);
	sched_yield();

	return 0;
}

void sleep_queue_tick(uint64_t ns)
{
	if(!pcpu_data)
		return;

	pcpu_sleep_queue* data = &pcpu_data[smp_current_cpu()];

	uint64_t flags = local_irq_save();
	
	task_sleep_desc* tmp;
	task_sleep_desc* cur;
	list_for_each_entry_safe(cur, tmp, &data->queue, list_node)
	{
		if(cur->delta <= ns)
		{
			ns -= cur->delta;
			cur->delta = 0;

			task_status exp_status = TASK_INTR_SLEEPING;
			if(!atomic_compare_exchange_strong(&cur->task->status, &exp_status, TASK_RUNNING))
			{
				break;
			}

			list_del(&cur->list_node);
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

	local_irq_restore(flags);
}		

void sleep_queue_init(uint32_t num_cpus)
{
	pcpu_data = kmalloc(num_cpus * sizeof(pcpu_sleep_queue));
	for(uint32_t i = 0; i < num_cpus; i++)
	{
		list_node_init(&pcpu_data[i].queue);
	}
}
