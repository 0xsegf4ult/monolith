#pragma once

#include <arch/x86_64/context.hpp>
#include <sys/cred.hpp>
#include <sys/spinlock.hpp>
#include <sys/waitqueue.hpp>
#include <types.hpp>
#include <list.hpp>
#include <stdatomic.h>

enum task_status : uint32_t
{
	TASK_RUNNING,
	TASK_SLEEPING,
	TASK_INTR_SLEEPING,
	TASK_ZOMBIE,
	TASK_STOPPED
};

enum task_flags : uint32_t
{
	TASK_CAN_REAP = 1
};

constexpr const char* get_status_name(task_status status)
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

struct address_space;

namespace vfs
{
	struct ventry_t;
}

struct tty_device;
struct timespec;

struct task_t 
{
	char name[32];
	
	_Atomic uint32_t flags;
	_Atomic task_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	virtaddr_t rsp0_top;
	address_space* vm_space;
	address_space* current_vm_space;
	virtaddr_t entry;
	
	pid_t pid;
	pid_t pgid;
	pid_t sid;
	pid_t tgid;

	task_t* _Atomic parent;
	
	list_head_t children;
	list_head_t sibling;
	spinlock_t child_list_lock;

	cred_t cred;
	vfs::ventry_t* cwd;
	tty_device* tty;
	int open_files[32];

	int return_status;
	int return_signal;

	sigset_t sig_pending;
	sigset_t sig_blocked;
	spinlock_t sig_lock;

	wait_queue_node wait; 

	//FIXME: get rid of this
	task_t* next;

	list_node_t queue_node;
	list_node_t list_node;
};

extern list_head_t  g_task_list;
extern spinlock_t g_task_list_lock;

static_assert(__builtin_offsetof(task_t, rsp0) == 40, "asm context switch expects rsp0 at 40 bytes"); 
task_t* task_new(const char* name);
task_t* thread_kernel_new(const char* name, virtaddr_t entry);
task_t* process_userspace_new(const char* name, const char** argv);
void task_zombify(task_t* proc);
void task_destroy(task_t* proc);

task_t* lookup_by_pid(pid_t pid);
task_t* get_pgrp_leader(pid_t pgrp);

constexpr bool is_session_leader(task_t* task)
{
	return task->sid == task->tgid;
}

void task_init();
