#include <stdint.h>
#include <stddef.h>
#include <errno.h>

size_t strlen(const char* s)
{
	size_t sz = 0;
	while(*s++ != '\0') { ++sz; }
	return sz;
}

int strncmp(const char* src, const char* dst, size_t len)
{
	while(len && *src && (*src == *dst))
	{
		src++;
		dst++;
		len--;
	}

	if(len == 0)
		return 0;
	else
		return (*((unsigned char*)src) - *((unsigned char*)dst));
}

const char* strerrorname_np(int errnum)
{
	switch(errnum)
	{
	case EPERM:
		return "EPERM";
	case ENOENT:
		return "ENOENT";
	case EBADF:
		return "EBADF";
	case ECHILD:
		return "ECHILD";
	case EFAULT:
		return "EFAULT";
	case EEXIST:
		return "EEXIST";
	case ENOTDIR:
		return "ENOTDIR";
	case EISDIR:
		return "EISDIR";
	case EINVAL:
		return "EINVAL";
	case EMFILE:
		return "EMFILE";
	case ENOTTY:
		return "ENOTTY";
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
	case EBADF:
		return "Bad file descriptor";
	case ECHILD:
		return "No child processes";
	case EFAULT:
		return "Bad address";
	case EEXIST:
		return "File exists";
	case ENOTDIR:
		return "Not a directory";
	case EISDIR:
		return "Is a directory";
	case EINVAL:
		return "Invalid argument";
	case EMFILE:
		return "Too many open files";
	case ENOTTY:
		return "Inappropriate ioctl for device";
	default:
		return "Unknown error";
	}
}
