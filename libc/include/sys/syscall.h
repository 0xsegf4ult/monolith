#ifndef _MONOLITH_SYSCALL
#define _MONOLITH_SYSCALL

#include <stdint.h>
#include <stddef.h>

enum SYSCALL_ID
{
	SYS_OPEN,
	SYS_OPENAT,
	SYS_CLOSE,
	SYS_READ,
	SYS_WRITE,
	SYS_SPAWN,
	SYS_SPAWNAT,
	SYS_EXIT,
	SYS_WAIT,
	SYS_IOCTL,
	SYS_STAT,
	SYS_FSTAT,
	SYS_GETDENTS,
	SYS_CHDIR,
	SYS_MKDIR,
	SYS_GETCWD,
	SYS_MMAP,
	SYS_MUNMAP,
	SYS_MOUNT,
	SYS_GETUID,
	SYS_SETUID,
	SYS_GETGID,
	SYS_SETGID,
	SYS_GETPID,
	SYS_SETSID,
	SYS_GETPGID,
	SYS_SETPGID,
	SYS_SOCKET,
};

__attribute__((always_inline)) inline uint64_t _syscall(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	register uint64_t r_rax asm("rax") = id;
	register uint64_t r_rdi asm("rdi") = arg0;
	register uint64_t r_rsi asm("rsi") = arg1;
	register uint64_t r_rdx asm("rdx") = arg2;
	register uint64_t r_rcx asm("rcx") = arg3;
	register uint64_t r_r8 asm("r8") = arg4;
	register uint64_t r_r9 asm("r9") = arg5;

	uint64_t ret;
	asm volatile("int $0x80" : "=a"(ret) : "r"(r_rax), "r"(r_rdi), "r"(r_rsi), "r"(r_rdx), "r"(r_rcx), "r"(r_r8), "r"(r_r9));
	return ret;
}

#endif
