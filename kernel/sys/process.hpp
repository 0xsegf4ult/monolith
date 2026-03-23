#pragma once

#include <arch/x86_64/context.hpp>
#include <lib/types.hpp>

enum class process_status : uint32_t
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

struct process_t
{
	char name[32];
	uint32_t pid;
	process_status status;
	virtaddr_t rsp0;
	virtaddr_t rsp;
	virtaddr_t rsp0_top;
	address_space* vm_space;
	virtaddr_t entry;

	process_t* parent;
	process_t* children;
	process_t* sibling;

	vfs::ventry_t* cwd;
	int open_files[32];

	process_t* next;
};

static_assert(__builtin_offsetof(process_t, rsp0) == 40, "asm context switch expects rsp0 at 40 bytes"); 
process_t* create_process(const char* name, bool is_user);
void destroy_process(process_t* proc);
