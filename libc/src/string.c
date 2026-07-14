#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <signal.h>

void* memcpy(void* dst, const void* src, size_t n)
{
	uint8_t* pdest = (uint8_t*)dst;
	const uint8_t* psrc = (const uint8_t*)src;

	for(size_t i = 0; i < n; i++)
		pdest[i] = psrc[i];

	return dst;
}

void* memset(void* dst, int data, size_t n)
{
	uint8_t* pdest = (uint8_t*)dst;

	for(size_t i = 0; i < n; i++)
		pdest[i] = (uint8_t)data;

	return dst;
}

size_t strlen(const char* s)
{
	size_t sz = 0;
	while(*s++ != '\0') { ++sz; }
	return sz;
}
	
char* strncpy(char* dst, const char* src, size_t n)
{
	size_t i = 0;

        for(; i < n && src[i] != '\0'; i++)
                dst[i] = src[i];

        for(; i < n; i++)
                dst[i] = '\0';

        return dst;
}

int strcmp(const char* s1, const char* s2)
{
	while(*s1 == *s2)
	{
		if(!*s1)
			return 0;

		s1++;
		s2++;
	}

	return *s1 - *s2;
}

int strncmp(const char* src, const char* dst, size_t n)
{
	while(n && *src && (*src == *dst))
	{
		src++;
		dst++;
		n--;
	}

	if(n == 0)
		return 0;
	else
		return (*((unsigned char*)src) - *((unsigned char*)dst));
}

char* strchr(const char* s, int c)
{
	while(*s)
	{
		if(*s == (char)c)
			return (char*)s;
		s++;
	}

	return NULL;
}

char* strtok_r(char* s1, const char* s2, char** saveptr)
{
	if(s1 == NULL)
		s1 = *saveptr;

	if(s1 == NULL)
		return NULL;

	while(*s1 != '\0')
	{
		int is_delim = 0;
		for(size_t i = 0; s2[i] != '\0'; i++)
		{
			if(*s1 == s2[i])
			{
				is_delim = 1;
				break;
			}
		}

		if(!is_delim)
			break;

		s1++;
	}

	if(*s1 == '\0')
	{
		*saveptr = s1;
		return NULL;
	}

	char* tok_start = s1;

	while(*s1 != '\0')
	{
		int is_delim = 0;
		for(size_t i = 0; s2[i] != '\0'; i++)
		{
			if(*s1 == s2[i])
			{
				is_delim = 1;
				break;
			}
		}

		if(is_delim)
		{
			*s1 = '\0';
			*saveptr = s1 + 1;
			return tok_start;
		}
		s1++;
	}

	*saveptr = s1;
	return tok_start;
}

static char* strtok_last = NULL;

char* strtok(char* s1, const char* s2)
{
	return strtok_r(s1, s2, &strtok_last);
}

const char* strerrorname_np(int errnum)
{
	switch(errnum)
	{
	case EPERM:
		return "EPERM";
	case ENOENT:
		return "ENOENT";
	case ESRCH:
		return "ESRCH";
	case EINTR:
		return "EINTR";
	case EIO:
		return "EIO";
	case ENXIO:
		return "ENXIO";
	case E2BIG:
		return "E2BIG";
	case ENOEXEC:
		return "ENOEXEC";
	case EBADF:
		return "EBADF";
	case ECHILD:
		return "ECHILD";
	case EAGAIN:
		return "EAGAIN";
	case ENOMEM:
		return "ENOMEM";
	case EACCES:
		return "EACCES";
	case EFAULT:
		return "EFAULT";
	case ENOTBLK:
		return "ENOTBLK";
	case EBUSY:
		return "EBUSY";
	case EEXIST:
		return "EEXIST";
	case EXDEV:
		return "EXDEV";
	case ENODEV:
		return "ENODEV";
	case ENOTDIR:
		return "ENOTDIR";
	case EISDIR:
		return "EISDIR";
	case EINVAL:
		return "EINVAL";
	case ENFILE:
		return "ENFILE";
	case EMFILE:
		return "EMFILE";
	case ENOTTY:
		return "ENOTTY";
	case ETXTBSY:
		return "ETXTBSY";
	case EFBIG:
		return "EFBIG";
	case ENOSPC:
		return "ENOSPC";
	case ESPIPE:
		return "ESPIPE";
	case EROFS:
		return "EROFS";
	case EMLINK:
		return "EMLINK";
	case EPIPE:
		return "EPIPE";
	case EDOM:
		return "EDOM";
	case ERANGE:
		return "ERANGE";
	case EDEADLK:
		return "EDEADLK";
	case ENAMETOOLONG:
		return "ENAMETOOLONG";
	case ENOLCK:
		return "ENOLCK";
	case ENOSYS:
		return "ENOSYS";
	case ENOTEMPTY:
		return "ENOTEMPTY";
	case ELOOP:
		return "ELOOP";
	case ENOTSOCK:
		return "ENOTSOCK";
	case EAFNOSUPPORT:
		return "EAFNOSUPPORT";
	case EADDRINUSE:
		return "EADDRINUSE";
	case ENETUNREACH:
		return "ENETUNREACH";
	case ENOBUFS:
		return "ENOBUFS";
	case ENOTSUP:
		return "ENOTSUP";
	default:
		return "(UNKNOWN)";	
	}
}

const char* strerrordesc_np(int errnum)
{
	switch(errnum)
	{
	case EPERM:
		return "Operation not permitted";
	case ENOENT:
		return "No such file or directory";
	case ESRCH:
		return "No such process";
	case EINTR:
		return "Interrupted system call";
	case EIO:
		return "Input/output error";
	case ENXIO:
		return "No such device or address";
	case E2BIG:
		return "Argument list too long";
	case ENOEXEC:
		return "Exec format error";
	case EBADF:
		return "Bad file descriptor";
	case ECHILD:
		return "No child processes";
	case EAGAIN:
		return "Resource temporarily unavailable";
	case ENOMEM:
		return "Cannot allocate memory";
	case EACCES:
		return "Permission denied";
	case EFAULT:
		return "Bad address";
	case ENOTBLK:
		return "Block device required";
	case EBUSY:
		return "Device or resource busy";
	case EEXIST:
		return "File exists";
	case EXDEV:
		return "Invalid cross-device link";
	case ENODEV:
		return "No such device";
	case ENOTDIR:
		return "Not a directory";
	case EISDIR:
		return "Is a directory";
	case EINVAL:
		return "Invalid argument";
	case ENFILE:
		return "Too many open files in system";
	case EMFILE:
		return "Too many open files";
	case ENOTTY:
		return "Inappropriate ioctl for device";
	case ETXTBSY:
		return "Text file busy";
	case EFBIG:
		return "File too large";
	case ENOSPC:
		return "No space left on device";
	case ESPIPE:
		return "Illegal seek";
	case EROFS:
		return "Read-only file system";
	case EMLINK:
		return "Too many links";
	case EPIPE:
		return "Broken pipe";
	case EDOM:
		return "Numerical argument out of domain";
	case ERANGE:
		return "Numerical result out of range";
	case EDEADLK:
		return "Resource deadlock avoided";
	case ENAMETOOLONG:
		return "File name too long";
	case ENOLCK:
		return "No locks available";
	case ENOSYS:
		return "Function not implemented";
	case ENOTEMPTY:
		return "Directory not empty";
	case ELOOP:
		return "Too many levels of symbolic links";
	case ENOTSOCK:
		return "Socket operation on non-socket";
	case EAFNOSUPPORT:
		return "Address family not supported";
	case EADDRINUSE:
		return "Address already in use";
	case ENETUNREACH:
		return "Network is unreachable";
	case ENOBUFS:
		return "No buffer space available";
	case ENOTSUP:
		return "Not supported parameter or option";
	default:
		return "Unknown error";
	}
}

const char* sigabbrev_np(int sig)
{
	switch(sig)
	{
	case SIGHUP:
		return "HUP";
	case SIGINT:
		return "INT";
	case SIGQUIT:
		return "QUIT";
	case SIGILL:
		return "ILL";
	case SIGTRAP:
		return "TRAP";
	case SIGABRT:
		return "ABRT";
	case SIGBUS:
		return "BUS";
	case SIGFPE:
		return "FPE";
	case SIGKILL:
		return "KILL";
	case SIGUSR1:
		return "USR1";
	case SIGSEGV:
		return "SEGV";
	case SIGUSR2:
		return "USR2";
	case SIGPIPE:
		return "PIPE";
	case SIGALRM:
		return "ALRM";
	case SIGTERM:
		return "TERM";
	case SIGSTKFLT:
		return "STKFLT";
	case SIGCHLD:
		return "CHLD";
	case SIGCONT:
		return "CONT";
	case SIGSTOP:
		return "STOP";
	case SIGTSTP:
		return "TSTP";
	case SIGTTIN:
		return "TTIN";
	case SIGTTOU:
		return "TTOU";
	case SIGURG:
		return "URG";
	case SIGXCPU:
		return "XCPU";
	case SIGXFSZ:
		return "XFSZ";
	case SIGVTALRM:
		return "VTALRM";
	case SIGPROF:
		return "PROF";
	case SIGWINCH:
		return "WINCH";
	case SIGPOLL:
		return "POLL";
	case SIGPWR:
		return "PWR";
	case SIGSYS:
		return "SYS";
	default:
		return NULL;
	}
}

const char* sigdescr_np(int sig)
{
	switch(sig)
	{
	case SIGHUP:
		return "Hangup";
	case SIGINT:
		return "Interrupt";
	case SIGQUIT:
		return "Quit";
	case SIGILL:
		return "Illegal instruction";
	case SIGTRAP:
		return "Trace/breakpoint trap";
	case SIGABRT:
		return "Aborted";
	case SIGBUS:
		return "Bus error";
	case SIGFPE:
		return "Floating point exception";
	case SIGKILL:
		return "Killed";
	case SIGUSR1:
		return "User defined signal 1";
	case SIGSEGV:
		return "Segmentation fault";
	case SIGUSR2:
		return "User defined signal 2";
	case SIGPIPE:
		return "Broken pipe";
	case SIGALRM:
		return "Alarm clock";
	case SIGTERM:
		return "Terminated";
	case SIGSTKFLT:
		return "Stack fault";
	case SIGCHLD:
		return "Child exited";
	case SIGCONT:
		return "Continued";
	case SIGSTOP:
		return "Stopped (signal)";
	case SIGTSTP:
		return "Stopped";
	case SIGTTIN:
		return "Stopped (tty input)";
	case SIGTTOU:
		return "Stopped (tty output)";
	case SIGURG:
		return "Urgent I/O condition";
	case SIGXCPU:
		return "CPU time limit exceeded";
	case SIGXFSZ:
		return "File size limit exceeded";
	case SIGVTALRM:
		return "Virtual timer expired";
	case SIGPROF:
		return "Profiling timer expired";
	case SIGWINCH:
		return "Window changed";
	case SIGPOLL:
		return "I/O possible";
	case SIGPWR:
		return "Power failure";
	case SIGSYS:
		return "Bad system call";
	default:
		return NULL;
	}
}
