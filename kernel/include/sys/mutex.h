#pragma once

struct task;

#include <sys/spinlock.h>

typedef struct mutex
{
	spinlock_t spinlock;
	struct task* waitqueue;
	bool locked;
} mutex_t;

void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);
