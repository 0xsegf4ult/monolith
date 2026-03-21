#include <syscall.h>

void _exit(int status)
{
	_syscall(SYS_EXIT, status, 0, 0, 0, 0);
}

void exit(int status)
{
	_exit(status);
}
