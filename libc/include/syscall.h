#ifndef _MONOLITH_SYSCALL
#define _MONOLITH_SYSCALL

#include <stdint.h>
#include <stddef.h>
typedef int64_t ssize_t;

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
	SYS_DEBUGMSG
};

__attribute__((always_inline)) inline uint64_t _syscall(uint64_t id, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
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
	_syscall(SYS_DEBUGMSG, (uint64_t)message, 0, 0, 0, 0);
}

enum OPEN_FLAGS
{
	O_CREAT = 1
};

inline int open(const char* path, int flags)
{
	return (int)(_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0));
}

inline int openat(int dirfd, const char* path, int flags)
{
	return (int)(_syscall(SYS_OPENAT, (uint64_t)dirfd, (uint64_t)path, (uint64_t)flags, 0, 0));
}

inline int close(int fd)
{
	return (int)(_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0));
}

inline ssize_t read(int fd, void* buffer, size_t size)
{
	return (ssize_t)(_syscall(SYS_READ, fd, (uint64_t)buffer, size, 0, 0));
}

inline ssize_t write(int fd, const void* buffer, size_t size)
{
	return (ssize_t)(_syscall(SYS_WRITE, fd, (uint64_t)buffer, size, 0, 0));
}

inline int spawn(const char* path, const char** argv)
{
	return (int)_syscall(SYS_SPAWN, (uint64_t)path, (uint64_t)argv, 0, 0, 0);
}

inline int spawnat(int dirfd, const char* path, const char** argv)
{
	return (int)_syscall(SYS_SPAWNAT, (uint64_t)dirfd, (uint64_t)path, (uint64_t)argv, 0, 0);
}

inline int wait()
{
	return (int)_syscall(SYS_WAIT, 0, 0, 0, 0, 0);
}

inline int ioctl(int fd, uint64_t op, uint64_t arg)
{
	return (int)_syscall(SYS_IOCTL, fd, op, arg, 0, 0);
}

typedef enum
{
	S_IFDIR,
	S_IFREG,
	S_IFLNK,
	S_IFCHR,
	S_IFBLK
} stat_mode;

typedef struct 
{
	stat_mode mode;
	size_t size;
} stat_t;

typedef enum : uint8_t
{
	DT_DIR,
	DT_REG,
	DT_LNK,
	DT_CHR,
	DT_BLK
} dirent_type;

typedef struct
{
	uint16_t length;
	dirent_type type;
} dirent_info;

inline int stat(const char* path, stat_t* buffer)
{
	return (int)_syscall(SYS_STAT, (uint64_t)path, (uint64_t)buffer, 0, 0, 0);
}

inline int fstat(int fd, stat_t* buffer)
{
	return (int)_syscall(SYS_FSTAT, (uint64_t)fd, (uint64_t)buffer, 0, 0, 0);
}

inline ssize_t getdents(int fd, void* buffer, size_t length)
{
	return (ssize_t)_syscall(SYS_GETDENTS, (uint64_t)fd, (uint64_t)buffer, (uint64_t)length, 0, 0);
}

inline int chdir(const char* path)
{
	return (int)_syscall(SYS_CHDIR, (uint64_t)path, 0, 0, 0, 0);
}

inline int mkdir(const char* path)
{
	return (int)_syscall(SYS_MKDIR, (uint64_t)path, 0, 0, 0, 0);
}

#endif
