#ifndef _LIBC_UNISTD_H
#define _LIBC_UNISTD_H

#include <sys/types.h>
#include <sys/syscall.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0x1
#define R_OK 0x2
#define W_OK 0x4
#define X_OK 0x8

inline int close(int fd)
{
	return (int)(_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0));
}

inline ssize_t read(int fd, void* buffer, size_t size)
{
	return (ssize_t)(_syscall(SYS_READ, fd, (uint64_t)buffer, size, 0, 0, 0));
}

inline ssize_t write(int fd, const void* buffer, size_t size)
{
	return (ssize_t)(_syscall(SYS_WRITE, fd, (uint64_t)buffer, size, 0, 0, 0));
}

inline int spawn(const char** argv)
{
	return (int)_syscall(SYS_SPAWN, (uint64_t)argv, 0, 0, 0, 0, 0);
}

inline int spawnat(int dirfd, const char** argv)
{
	return (int)_syscall(SYS_SPAWNAT, (uint64_t)dirfd, (uint64_t)argv, 0, 0, 0, 0);
}

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

inline ssize_t getdents(int fd, void* buffer, size_t length)
{
	return (ssize_t)_syscall(SYS_GETDENTS, (uint64_t)fd, (uint64_t)buffer, (uint64_t)length, 0, 0, 0);
}

inline int chdir(const char* path)
{
	return (int)_syscall(SYS_CHDIR, (uint64_t)path, 0, 0, 0, 0, 0);
}

inline int getcwd(const char* buffer, size_t length)
{
	return (int)_syscall(SYS_GETCWD, (uint64_t)buffer, (uint64_t)length, 0, 0, 0, 0);
}

inline uid_t getuid()
{
	return (uid_t)_syscall(SYS_GETUID, 0, 0, 0, 0, 0, 0);
}

inline int setuid(uid_t uid)
{
	return (int)_syscall(SYS_SETUID, (uint64_t)uid, 0, 0, 0, 0, 0);
}

inline gid_t getgid()
{
	return (gid_t)_syscall(SYS_GETGID, 0, 0, 0, 0, 0, 0);
}

inline int setgid(gid_t gid)
{
	return (int)_syscall(SYS_SETGID, (uint64_t)gid, 0, 0, 0, 0, 0);
}

extern char* optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char* const argv[], const char* optstring);

#endif
