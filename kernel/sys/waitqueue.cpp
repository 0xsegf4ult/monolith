#include <sys/waitqueue.hpp>
#include <sys/spinlock.hpp>
#include <sys/scheduler.hpp>
#include <sys/task.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <panic.hpp>

void wait_queue_init(wait_queue& queue)
{
	list_node_init(queue.head);
	spinlock_init(queue.lock);
}

void wait_queue_node_init(wait_queue_node& node)
{
	list_node_init(node.list_node);
	node.queue = nullptr;
	node.task = nullptr;
}

void wait_queue_register(wait_queue& queue, wait_queue_node& node)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(queue.lock, rflags);

	if(list_empty(node.list_node))
	{
		list_add_tail(queue.head, node.list_node);
		node.queue = &queue;
	} 
	else if(node.queue != &queue)
	{
		panic("wait_queue_register: adding node to multiple queues!");
	}

	spinlock_release_irqsave(queue.lock, rflags);
}

void wait_queue_unregister(wait_queue_node& node)
{
	wait_queue* queue = node.queue;

	asm volatile("mfence" ::: "memory");

	if(!queue)
		return;

	uint64_t rflags;
	spinlock_acquire_irqsave(queue->lock, rflags);
	if(!list_empty(node.list_node))
	{
		list_del(node.list_node);
		node.queue = nullptr;
	}
	else if(node.queue)
	{
		panic("wait_queue_unregister: node has queue but does not point to list");
	}
	spinlock_release_irqsave(queue->lock, rflags);
}

void wait_queue_wake(wait_queue& queue)
{
	uint64_t rflags;
	spinlock_acquire_irqsave(queue.lock, rflags);

	wait_queue_node* cur;
	wait_queue_node* tmp;
	list_for_each_entry_safe(cur, tmp, queue.head, list_node)
	{
		cur->queue = nullptr;
		list_del(cur->list_node);
		
		task_status exp_state = TASK_INTR_SLEEPING;
		if(!atomic_compare_exchange_strong(&cur->task->status, &exp_state, TASK_RUNNING))
			panic("wait_queue_wake: task on queue not sleeping");

		sched_add_ready(cur->task);
	}

	spinlock_release_irqsave(queue.lock, rflags);
}
