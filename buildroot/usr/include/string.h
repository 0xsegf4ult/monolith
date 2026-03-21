#ifndef _LIBC_STRING_H
#define _LIBC_STRING_H

#include <stdint.h>

inline size_t strncmp(const char* src, const char* dst, size_t len)
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

#endif
