#pragma once

#include <types.hpp>

enum mmap_flags
{
	MAP_SHARED = 0x1,
	MAP_PRIVATE = 0x2,
	MAP_ANONYMOUS = 0x8,
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
	MOUNT,
	GETUID,
	SETUID,
	GETGID,
	SETGID,
	GETPID,
	SETSID,
	GETPGID,
	SETPGID,
};

void sys_exit(int status);

struct cpu_context_t;
void syscall_handler(cpu_context_t* ctx);
