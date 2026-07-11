#pragma once

#include <arch/x86_64/context.hpp>
#include <sys/cred.hpp>
#include <sys/spinlock.hpp>
#include <sys/waitqueue.hpp>
#include <types.hpp>
#include <list.hpp>
#include <stdatomic.h>

enum thread_status : uint32_t
{
	THREAD_RUNNING,
	THREAD_SLEEPING,
	THREAD_INTR_SLEEPING,
	THREAD_ZOMBIE,
	THREAD_STOPPED
};

enum thread_flags : uint32_t
{
	THREAD_CAN_REAP = 1
};

constexpr const char* get_status_name(thread_status status)
{
	switch(status)
	{
	case THREAD_RUNNING:
		return "R (running)";
	case THREAD_INTR_SLEEPING:
		return "S (sleeping)";
	case THREAD_SLEEPING:
		return "D (uninterruptible sleep)";
	case THREAD_ZOMBIE:
		return "Z (zombie)";
	case THREAD_STOPPED:
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

struct thread_t
{
	char name[32];
	
	_Atomic uint32_t flags;
	_Atomic thread_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	virtaddr_t rsp0_top;
	address_space* vm_space;
	virtaddr_t entry;
	
	pid_t pid;
	pid_t pgid;
	pid_t sid;

	thread_t* _Atomic parent;
	
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

	thread_t* next;
	list_node_t queue_node;
	list_node_t list_node;
};

constexpr bool thread_running(thread_t* thr)
{
	auto status = atomic_load_explicit(&thr->status, memory_order_acquire);
	return status == THREAD_RUNNING;
}

constexpr bool thread_intr_sleeping(thread_t* thr)
{
	auto status = atomic_load_explicit(&thr->status, memory_order_acquire);
	return status == THREAD_INTR_SLEEPING;
}	

constexpr bool thread_is_zombie(thread_t* thr)
{
	auto status = atomic_load_explicit(&thr->status, memory_order_acquire);
	return status == THREAD_ZOMBIE;
}

constexpr void thread_set_running(thread_t* thr)
{
	atomic_store_explicit(&thr->status, THREAD_RUNNING, memory_order_release);
}

constexpr void thread_set_sleeping(thread_t* thr)
{
	atomic_store_explicit(&thr->status, THREAD_SLEEPING, memory_order_release);
}

constexpr void thread_set_intr_sleeping(thread_t* thr)
{
	atomic_store_explicit(&thr->status, THREAD_INTR_SLEEPING, memory_order_release);
}

constexpr void thread_set_zombie(thread_t* thr)
{
	atomic_store_explicit(&thr->status, THREAD_ZOMBIE, memory_order_release);
}

extern list_head_t  g_thread_list;
extern spinlock_t g_thread_list_lock;

static_assert(__builtin_offsetof(thread_t, rsp0) == 40, "asm context switch expects rsp0 at 40 bytes"); 
thread_t* create_thread(const char* name, const char** argv, bool is_user);
void thread_zombify(thread_t* proc);
void destroy_thread(thread_t* proc);
thread_t* thread_lookup_by_pid(pid_t pid);
thread_t* get_pgrp_leader(pid_t pgrp);
void threading_init();
