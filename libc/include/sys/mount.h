#ifndef _LIBC_SYS_MOUNT_H
#define _LIBC_SYS_MOUNT_H

#include <sys/syscall.h>

static inline int mount(const char* source, const char* target, const char* fsname)
{
	return (int)_syscall(SYS_MOUNT, (uint64_t)source, (uint64_t)target, (uint64_t)fsname, 0, 0, 0);
}

#endif
