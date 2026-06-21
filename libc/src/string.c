#include <stdint.h>
#include <stddef.h>
#include <errno.h>

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
	case ENOBUFS:
		return "No buffer space available";
	case ENOTSUP:
		return "Not supported parameter or option";
	default:
		return "Unknown error";
	}
}
