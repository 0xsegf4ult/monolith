#ifndef _LIBC_SYS_IOCTL_H
#define _LIBC_SYS_IOCTL_H

#include <sys/types.h>
#include <sys/syscall.h>

static inline int ioctl(int fd, uint64_t op, void* arg)
{
        return (int)_syscall(SYS_IOCTL, fd, op, (uint64_t)arg, 0, 0, 0);
}

#endif
