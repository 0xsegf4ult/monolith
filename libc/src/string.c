#include <stdint.h>
#include <stddef.h>

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
