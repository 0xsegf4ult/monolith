#ifndef _LIBC_UNISTD_H
#define _LIBC_UNISTD_H

#include <syscall.h>

typedef int64_t ssize_t;
typedef int64_t off_t;

enum OPEN_FLAGS
{
	O_CREAT = 1
};

inline int open(const char* path, int flags)
{
	return (int)(_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0, 0));
}

inline int openat(int dirfd, const char* path, int flags)
{
	return (int)(_syscall(SYS_OPENAT, (uint64_t)dirfd, (uint64_t)path, (uint64_t)flags, 0, 0, 0));
}

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

inline int wait()
{
	return (int)_syscall(SYS_WAIT, 0, 0, 0, 0, 0, 0);
}

inline int ioctl(int fd, uint64_t op, uint64_t arg)
{
	return (int)_syscall(SYS_IOCTL, fd, op, arg, 0, 0, 0);
}

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

typedef uint32_t mode_t;

typedef struct 
{
	mode_t mode;
	uint32_t nlinks;
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
	return (int)_syscall(SYS_STAT, (uint64_t)path, (uint64_t)buffer, 0, 0, 0, 0);
}

inline int fstat(int fd, stat_t* buffer)
{
	return (int)_syscall(SYS_FSTAT, (uint64_t)fd, (uint64_t)buffer, 0, 0, 0, 0);
}

inline ssize_t getdents(int fd, void* buffer, size_t length)
{
	return (ssize_t)_syscall(SYS_GETDENTS, (uint64_t)fd, (uint64_t)buffer, (uint64_t)length, 0, 0, 0);
}

inline int chdir(const char* path)
{
	return (int)_syscall(SYS_CHDIR, (uint64_t)path, 0, 0, 0, 0, 0);
}

inline int mkdir(const char* path, mode_t mode)
{
	return (int)_syscall(SYS_MKDIR, (uint64_t)path, (uint64_t)mode, 0, 0, 0, 0);
}

inline int getcwd(const char* buffer, size_t length)
{
	return (int)_syscall(SYS_GETCWD, (uint64_t)buffer, (uint64_t)length, 0, 0, 0, 0);
}

inline void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	return (void*)_syscall(SYS_MMAP, (uint64_t)addr, (uint64_t)length, (uint64_t)prot, (uint64_t) flags, (uint64_t)fd, (uint64_t)offset);
}

inline int munmap(void* addr, size_t length)
{
	return (int)_syscall(SYS_MUNMAP, (uint64_t)addr, (uint64_t)length, 0, 0, 0, 0);
}

extern char* optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char* const argv[], const char* optstring);

#endif
