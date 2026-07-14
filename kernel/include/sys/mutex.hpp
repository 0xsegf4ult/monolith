#pragma once

#include <sys/spinlock.hpp>
#include <sys/waitqueue.hpp>
#include <types.hpp>

struct task_t;

struct mutex_waiter
{
	mutex_waiter* list_prev;
	mutex_waiter* list_next;
	task_t* task;
};

struct mutex_t
{
	spinlock_t spinlock;
	task_t* waitqueue;
	bool locked;
};

void mutex_init(mutex_t& mutex);
void mutex_lock(mutex_t& mutex);
void mutex_unlock(mutex_t& mutex);
