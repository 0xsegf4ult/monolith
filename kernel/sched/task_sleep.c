#include <sched/task_sleep.h>
#include <errno.h>
#include <types.h>

int sys_clock_nanosleep(clockid_t clock, int flags, const struct timespec* tv, struct timespec* rem)
{
	return -ENOSYS;
}
