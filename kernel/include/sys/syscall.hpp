#pragma once

#include <types.hpp>

enum mmap_flags
{
	MAP_SHARED = 0x1,
	MAP_PRIVATE = 0x2,
	MAP_ANONYMOUS = 0x16,
};

enum class syscall_id : uint64_t
{
	OPEN,
	OPENAT,
	CLOSE,
	READ,
	WRITE,
	SEEK,
	SPAWN,
	EXIT,
	WAITPID,
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
	SOCKET,
	BIND,
	RECVFROM,
	SENDTO,
	CLOCK_GETTIME,
	CLOCK_NANOSLEEP,
	ARCH_PRCTL
};

void sys_exit(int status);

struct cpu_context_t;
void syscall_handler(cpu_context_t* ctx);
