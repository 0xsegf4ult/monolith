#pragma once

#include <stdint.h>
#include <stddef.h>

enum SYSCALL_ID
{
	SYS_OPEN,
	SYS_CLOSE,
	SYS_READ,
	SYS_WRITE,
	SYS_SPAWN,
	SYS_EXIT,
	SYS_DEBUGMSG
};

__attribute__((always_inline)) inline size_t _syscall(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	register uint64_t r_rdi asm("rdi") = id;
	register uint64_t r_rsi asm("rsi") = arg0;
	register uint64_t r_rdx asm("rdx") = arg1;
	register uint64_t r_rcx asm("rcx") = arg2;
	register uint64_t r_r8 asm("r8") = arg3;
	register uint64_t r_r9 asm("r9") = arg4;

	uint64_t ret;
	asm volatile("int $0x80" : "=a"(ret) : "r"(r_rdi), "r"(r_rsi), "r"(r_rdx), "r"(r_rcx), "r"(r_r8), "r"(r_r9));
	return ret;
}

inline void debugmsg(const char* message)
{
	_syscall(SYS_DEBUGMSG, reinterpret_cast<uint64_t>(message), 0, 0, 0, 0);
}

inline int open(const char* path, int flags)
{
	return _syscall(SYS_OPEN, reinterpret_cast<uint64_t>(path), flags, 0, 0, 0);
}

inline size_t read(int fd, void* buffer, size_t size)
{
	return _syscall(SYS_READ, fd, reinterpret_cast<uint64_t>(buffer), size, 0, 0);
}

inline size_t write(int fd, const void* buffer, size_t size)
{
	return _syscall(SYS_WRITE, fd, reinterpret_cast<uint64_t>(buffer), size, 0, 0);
}

inline int spawn(const char* path)
{
	return _syscall(SYS_SPAWN, reinterpret_cast<uint64_t>(path), 0, 0, 0, 0);
}

inline void _exit(int status)
{
	_syscall(SYS_EXIT, status, 0, 0, 0, 0);
}
