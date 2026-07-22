#pragma once

#include <sys/spinlock.h>
#include <libk/list.h>

struct task;

typedef struct wait_queue_type
{
	list_head_t head;
	spinlock_t lock;
} wait_queue;

typedef struct wait_queue_node_type
{
	list_node_t list_node;
	wait_queue* queue;
	struct task* task;
} wait_queue_node;

void wait_queue_init(wait_queue* queue);
void wait_queue_node_init(wait_queue_node* node, struct task* task);
void wait_queue_register(wait_queue* queue, wait_queue_node* node);
void wait_queue_unregister(wait_queue_node* node);
void wait_queue_wake(wait_queue* queue);
