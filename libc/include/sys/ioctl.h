#ifndef _LIBC_SYS_IOCTL_H
#define _LIBC_SYS_IOCTL_H

#include <sys/types.h>
#include <sys/syscall.h>

inline int ioctl(int fd, uint64_t op, uint64_t arg)
{
        return (int)_syscall(SYS_IOCTL, fd, op, arg, 0, 0, 0);
}

#endif
