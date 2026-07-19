#pragma once

#include <types.h>

struct stat
{
        dev_t st_dev;
        uint64_t st_ino;
        nlink_t st_nlink;
        mode_t st_mode;
        uid_t st_uid;
        gid_t st_gid;
        uint32_t padding;
        dev_t st_rdev;
        off_t st_size;
        int64_t st_blksize;
        int64_t st_blocks;
        struct timespec st_atim;
        struct timespec st_mtim;
        struct timespec st_ctim;
        int64_t _unused[3];
};

enum mode_flags_t : uint32_t
{
	S_IFMT		= 0170000,
	S_IFDIR 	= 0040000,
	S_IFCHR		= 0020000,
	S_IFBLK 	= 0060000,
	S_IFREG 	= 0100000,
	S_IFLNK 	= 0120000,
	S_IFSOCK	= 0140000,
	S_IFIFO		= 0010000,
	
	S_IRWXU		= 0000700,
	S_IRUSR		= 0000400,
	S_IWUSR		= 0000200,
	S_IXUSR		= 0000100,
	S_IRWXG		= 0000070,
	S_IRGRP		= 0000040,
	S_IWGRP		= 0000020,
	S_IXGRP		= 0000010,
	S_IRWXO		= 0000007,
	S_IROTH		= 0000004,
	S_IWOTH		= 0000002,
	S_IXOTH		= 0000001,

	S_ISUID		= 0004000,
	S_ISGID		= 0002000,
	S_ISVTX		= 0001000
};

static inline bool S_ISREG(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFREG;
};

static inline bool S_ISDIR(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFDIR;
}

static inline bool S_ISBLK(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFBLK;
}

static inline bool S_ISCHR(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFCHR;
}

static inline bool S_ISLNK(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFLNK;
}

static inline bool S_ISSOCK(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFSOCK;
}

static inline bool S_ISFIFO(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFIFO;
}
