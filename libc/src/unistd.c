#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <errno.h>

#define __TIO 90 
#define TIOCSPGRP ((__TIO << 8) + 1)

int isatty(int fd)
{
	struct stat st;
	if(fstat(fd, &st))
		return 0;

	if(S_ISCHR(st.st_mode))
		return 1;

	return -ENOTTY;
}

int tcsetpgrp(int fd, pid_t pgrp)
{
	if(isatty(fd) != 1)
		return -ENOTTY;

	return ioctl(fd, TIOCSPGRP, &pgrp);
}
