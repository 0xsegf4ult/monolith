#pragma once

#include <types.hpp>

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

constexpr bool S_ISREG(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFREG;
};

constexpr bool S_ISDIR(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFDIR;
}

constexpr bool S_ISBLK(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFBLK;
}

constexpr bool S_ISCHR(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFCHR;
}

constexpr bool S_ISLNK(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFLNK;
}

constexpr bool S_ISSOCK(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFSOCK;
}

constexpr bool S_ISFIFO(uint32_t flags)
{
	return (flags & S_IFMT) == S_IFIFO;
}

using mode_t = uint32_t;

