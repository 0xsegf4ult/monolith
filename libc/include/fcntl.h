#ifndef _LIBC_FCNTL_H
#define _LIBC_FCNTL_H

#include <sys/syscall.h>

#include <sys/stat.h>

#define O_NONBLOCK	0x0001
#define O_NDELAY	O_NONBLOCK
#define O_CLOEXEC	0x0002
#define O_RDONLY	0x0004
#define O_WRONLY	0x0008
#define O_RDWR		(O_RDONLY | O_WRONLY)
#define O_APPEND	0x0010
#define O_CREAT		0x0020
#define O_DSYNC		0x0040
#define O_EXCL		0x0080
#define O_NOCTTY	0x0100
#define O_RSYNC		0x0200
#define O_SYNC		0x0400
#define O_TRUNC		0x0800
#define O_CLOFORK	0x1000
#define O_ACCMODE	(O_RDONLY | O_WRONLY | O_RDWR)

inline int open(const char* path, int flags)
{
	return (int)(_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0, 0));
}

inline int openat(int dirfd, const char* path, int flags)
{
	return (int)(_syscall(SYS_OPENAT, (uint64_t)dirfd, (uint64_t)path, (uint64_t)flags, 0, 0, 0));
}

#endif
