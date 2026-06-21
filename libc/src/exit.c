#include <sys/syscall.h>
#include <errno.h>

void _exit(int status)
{
	_syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
}

#define MAX_ATEXIT 32

static void (*__atexit[MAX_ATEXIT])(void);
static int __atexit_count = 0;

int atexit(void (*func)(void))
{
	if(__atexit_count >= MAX_ATEXIT)
		return ENOBUFS;

	__atexit[__atexit_count] = func;
	__atexit_count++;
	return 0;
}

void exit(int status)
{
	for(int i = __atexit_count - 1; __atexit_count && i >= 0; i--)
		__atexit[i]();

	_exit(status);
}
