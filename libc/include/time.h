#ifndef _LIBC_TIME_H
#define _LIBC_TIME_H

#include <sys/types.h>
#include <sys/syscall.h>

struct timespec
{
	time_t tv_sec;
	uint32_t tv_nsec;
};

typedef int clockid_t;

#define CLOCK_REALTIME 1

static inline int clock_gettime(clockid_t clockid, struct timespec* tp)
{
	return (int)_syscall(SYS_CLOCK_GETTIME, (uint64_t)clockid, (uint64_t)tp, 0, 0, 0, 0);
}

static inline int clock_nanosleep(clockid_t clockid, int flags, const struct timespec* t, struct timespec* remain)
{
	return (int)_syscall(SYS_CLOCK_NANOSLEEP, (uint64_t)clockid, (uint64_t)flags, (uint64_t)t, (uint64_t)remain, 0, 0);
}

#endif
