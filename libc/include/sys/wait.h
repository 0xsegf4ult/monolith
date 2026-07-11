#ifndef _LIBC_SYS_WAIT_H
#define _LIBC_SYS_WAIT_H

#include <sys/syscall.h>
#include <sys/types.h>

#define _EXIT_NORMAL	0x100
#define _EXIT_SIGNALED	0x200

#define WIFEXITED(status)	(status & _EXIT_NORMAL)
#define WEXITSTATUS(status)	((int8_t)status & 0xFF)
#define WIFSIGNALED(status)	(status & _EXIT_SIGNALED)
#define WTERMSIG(status)	((int8_t)status & 0xFF)
#define WIFSTOPPED(status)	(0)
#define WSTOPSIG(status)	(0)

static inline pid_t wait(int* status)
{
	return (pid_t)_syscall(SYS_WAIT, (uint64_t)status, 0, 0, 0, 0, 0);
}

#endif
