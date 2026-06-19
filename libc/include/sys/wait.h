#ifndef _LIBC_SYS_WAIT_H
#define _LIBC_SYS_WAIT_H

#include <sys/syscall.h>
#include <sys/types.h>

pid_t wait(int* status)
{
	return (pid_t)_syscall(SYS_WAIT, (uint64_t)status, 0, 0, 0, 0, 0);
}

#endif
