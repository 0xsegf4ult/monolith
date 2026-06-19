#ifndef _LIBC_SYS_STAT_H
#define _LIBC_SYS_STAT_H

#include <sys/types.h>
#include <sys/syscall.h>

typedef enum
{
	S_IFMT          = 0170000,
        S_IFDIR         = 0040000,
        S_IFCHR         = 0020000,
        S_IFBLK         = 0060000,
        S_IFREG         = 0100000,
        S_IFLNK         = 0120000,
        S_IFSOCK        = 0140000,
        S_IFIFO         = 0010000,

        S_IRWXU         = 0000700,
        S_IRUSR         = 0000400,
        S_IWUSR         = 0000200,
        S_IXUSR         = 0000100,
        S_IRWXG         = 0000070,
        S_IRGRP         = 0000040,
        S_IWGRP         = 0000020,
        S_IXGRP         = 0000010,
        S_IRWXO         = 0000007,
        S_IROTH         = 0000004,
        S_IWOTH         = 0000002,
        S_IXOTH         = 0000001,

        S_ISUID         = 0004000,
        S_ISGID         = 0002000,
        S_ISVTX         = 0001000
} stat_mode;

#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

struct stat 
{
	mode_t mode;
	uint32_t nlinks;
	size_t size;
};

inline int stat(const char* path, struct stat* buffer)
{
	return (int)_syscall(SYS_STAT, (uint64_t)path, (uint64_t)buffer, 0, 0, 0, 0);
}

inline int fstat(int fd, struct stat* buffer)
{
	return (int)_syscall(SYS_FSTAT, (uint64_t)fd, (uint64_t)buffer, 0, 0, 0, 0);
}

inline int mkdir(const char* path, mode_t mode)
{
	return (int)_syscall(SYS_MKDIR, (uint64_t)path, (uint64_t)mode, 0, 0, 0, 0);
}

#endif
