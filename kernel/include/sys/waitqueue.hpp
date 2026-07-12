#pragma once

#include <sys/spinlock.hpp>
#include <list.hpp>

struct task_t;

struct wait_queue;

struct wait_queue_node
{
	list_node_t list_node;
	wait_queue* queue;
	task_t* task;
};

struct wait_queue
{
	list_head_t head;
	spinlock_t lock;
};

void wait_queue_init(wait_queue& queue);
void wait_queue_node_init(wait_queue_node& node);
void wait_queue_register(wait_queue& queue, wait_queue_node& node);
void wait_queue_unregister(wait_queue_node& node);
void wait_queue_wake(wait_queue& queue);
