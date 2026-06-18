#pragma once

#include <arch/x86_64/context.hpp>
#include <lib/types.hpp>
#include <sys/cred.hpp>

enum class thread_status : uint32_t
{
	ready,
	running,
	sleeping,
	terminated	
};

struct address_space;

namespace vfs
{
	struct ventry_t;
}

struct tty_device;

struct thread_t
{
	char name[32];
	uint32_t pid;
	thread_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	virtaddr_t rsp0_top;
	address_space* vm_space;
	virtaddr_t entry;

	thread_t* parent;
	thread_t* children;
	thread_t* sibling;

	cred_t cred;
	vfs::ventry_t* cwd;
	tty_device* tty;
	int open_files[32];

	int return_status;

	thread_t* next;
};

static_assert(__builtin_offsetof(thread_t, rsp0) == 40, "asm context switch expects rsp0 at 40 bytes"); 
thread_t* create_thread(const char* name, const char** argv, bool is_user);
void thread_zombify(thread_t* proc);
void destroy_thread(thread_t* proc);
