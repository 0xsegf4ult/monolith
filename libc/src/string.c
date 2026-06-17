#include <stdint.h>
#include <stddef.h>
#include <errno.h>

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
	case ENOEXEC:
		return "ENOEXEC";
	case ENOMEM:
		return "ENOMEM";
	case EACCES:
		return "EACCES";
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
	case ENOEXEC:
		return "Exec format error";
	case ENOMEM:
		return "Cannot allocate memory";
	case EACCES:
		return "Permission denied";
	default:
		return "Unknown error";
	}
}
