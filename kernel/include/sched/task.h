#pragma once

#include <sched/waitqueue.h>
#include <sys/spinlock.h>
#include <sys/cred.h>
#include <libk/list.h>
#include <types.h>
#include <stdatomic.h>

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

struct vm_space;
struct cpu_context;
struct tty_device;
struct ventry;

struct task
{
	char name[32];

	_Atomic uint32_t flags;
	_Atomic task_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	virtaddr_t rsp0_top;
	struct vm_space* owned_vm_space;
	struct vm_space* current_vm_space;
	struct cpu_context* context;
	virtaddr_t tls_base;

	pid_t pid;
	pid_t tgid;
	pid_t pgid;
	pid_t sid;

	struct task* _Atomic parent;

	list_head_t children;
	list_head_t sibling;
	spinlock_t child_list_lock;

	cred_t cred;
	struct ventry* cwd;
	struct tty_device* tty;
	int open_files[32];
	
	int return_status;
	int return_signal;

	sigset_t sig_pending;
	sigset_t sig_blocked;
	spinlock_t sig_lock;

	wait_queue_node wait;

	int argc;
	int envc;
	char** argv;
	char** envp;

	struct task* next;

	list_node_t queue_node;
	list_node_t list_node;
};
static_assert(offsetof(struct task, rsp0) == 40, "asm context switch expects rsp0 at 40 bytes");

extern list_head_t g_task_list;
extern spinlock_t g_task_list_lock;

struct task* task_new(const char* name, pid_t forcepid);
struct task* thread_kernel_new(const char* name, virtaddr_t entry);
struct task* process_userspace_new(const char* name, virtaddr_t entry);
void task_zombify(struct task* task);
void task_destroy(struct task* task);

int task_copy_args(struct task* proc, const char** argv, const char** envp);
struct task* lookup_by_pid(pid_t pid);
struct task* get_pgrp_leader(pid_t pgrp);

static inline bool is_session_leader(struct task* task)
{
	return task->sid == task->tgid;
}
