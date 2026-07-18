#pragma once

#include <sys/spinlock.h>
#include <libk/list.h>
#include <types.h>
#include <stdatomic.h>

struct vm_space;

typedef enum task_status_type : uint32_t
{
	TASK_RUNNING,
	TASK_SLEEPING,
	TASK_INTR_SLEEPING,
	TASK_ZOMBIE,
	TASK_STOPPED
} task_status;

enum task_flags : uint32_t
{
	TASK_CAN_REAP = 1
};

static inline const char* get_status_name(task_status status)
{
	switch(status)
        {
        case TASK_RUNNING:
                return "R (running)";
        case TASK_INTR_SLEEPING:
                return "S (sleeping)";
        case TASK_SLEEPING:
                return "D (uninterruptible sleep)";
        case TASK_ZOMBIE:
                return "Z (zombie)";
        case TASK_STOPPED:
                return "T (stopped)";
        }
}

struct task
{
	char name[32];

	_Atomic uint32_t flags;
	_Atomic task_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	virtaddr_t rsp0_top;
	struct vm_space* owner_vm_space;
	struct vm_space* current_vm_space;

	pid_t pid;
	pid_t tgid;
	pid_t pgid;
	pid_t sid;

	struct task* next;
};
