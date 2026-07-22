#include <sched/waitqueue.h>
#include <sched/scheduler.h>
#include <sched/task.h>
#include <sys/spinlock.h>
#include <libk/list.h>
#include <cpu.h>
#include <panic.h>
#include <types.h>
#include <stdatomic.h>

void wait_queue_init(wait_queue* queue)
{
	list_node_init(&queue->head);
	spinlock_init(&queue->lock);
}

void wait_queue_node_init(wait_queue_node* node, struct task* task)
{
	list_node_init(&node->list_node);
	node->queue = nullptr;
	node->task = task;
}

void wait_queue_register(wait_queue* queue, wait_queue_node* node)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&queue->lock, &flags);

	if(list_empty(&node->list_node))
	{
		list_add_tail(&queue->head, &node->list_node);
		node->queue = queue;
	} 
	else if(node->queue != queue)
	{
		panic("wait_queue_register: adding node to multiple queues!");
	}

	spinlock_release_irqsave(&queue->lock, flags);
}

void wait_queue_unregister(wait_queue_node* node)
{
	wait_queue* queue = node->queue;

	native_memory_barrier();

	if(!queue)
		return;

	uint64_t flags;
	spinlock_acquire_irqsave(&queue->lock, &flags);
	if(!list_empty(&node->list_node))
	{
		list_del(&node->list_node);
		node->queue = nullptr;
	}
	else if(node->queue)
	{
		panic("wait_queue_unregister: node has queue but does not point to list");
	}
	spinlock_release_irqsave(&queue->lock, flags);
}

void wait_queue_wake(wait_queue* queue)
{
	uint64_t flags;
	spinlock_acquire_irqsave(&queue->lock, &flags);

	wait_queue_node* cur;
	wait_queue_node* tmp;
	list_for_each_entry_safe(cur, tmp, &queue->head, list_node)
	{
		cur->queue = nullptr;
		list_del(&cur->list_node);
		
		task_status exp_state = TASK_INTR_SLEEPING;
		if(!atomic_compare_exchange_strong(&cur->task->status, &exp_state, TASK_RUNNING))
			panic("wait_queue_wake: task on queue not sleeping");

		sched_add_ready(cur->task);
	}

	spinlock_release_irqsave(&queue->lock, flags);
}
