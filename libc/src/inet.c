#include <arpa/inet.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

int inet_pton(int af, const char* src, void* in)
{
	if(af != AF_INET)
		return -EAFNOSUPPORT;

	uint8_t* dst = (uint8_t*)in;
	int val = 0;
	int digits = 0;
	int dots = 0;
	while(*src != '\0')
	{
		if(isdigit(*src))
		{
			val = val * 10 + (*src - '0');
			if(val > 255)
				return 0;
			
			digits++;

			if(digits > 3)
				return 0;
		}
		else if(*src == '.')
		{
			if(digits == 0 || dots == 3)
				return 0;

			*dst++ = (uint8_t)val;
			val = 0;
			digits = 0;
			dots++;
		}
		else
			return 0;

		src++;
	}

	if(digits == 0 || dots != 3)
		return 0;

	*dst = (uint8_t)val;

	return 1;
}

char* inet_ntop(int af, const void* src, char* dst, socklen_t size)
{
	if(af != AF_INET)
		return (char*)-EAFNOSUPPORT;

	if(size < INET_ADDRSTRLEN)
		return (char*)-ENOSPC;

	uint8_t* addr = (uint8_t*)src;
	int written = sprintf(dst, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
	*(dst + written) = '\0';

	return dst;
}
