#pragma once

#include <lib/types.hpp>

enum mmap_flags
{
	MAP_PRIVATE = 1,
	MAP_ANONYMOUS = 2,
};

enum mmap_prot_flags
{
	PROT_NONE = 0,
	PROT_READ = 1,
	PROT_WRITE = 2,
	PROT_EXEC = 4
};

enum class syscall_id : uint64_t
{
	OPEN,
	OPENAT,
	CLOSE,
	READ,
	WRITE,
	SPAWN,
	SPAWNAT,
	EXIT,
	WAIT,
	IOCTL,
	STAT,
	FSTAT,
	GETDENTS,
	CHDIR,
	MKDIR,
	GETCWD,
	MMAP,
	MUNMAP,
	DEBUG_PRINT
};

struct cpu_context_t;
void syscall_handler(cpu_context_t* ctx);
