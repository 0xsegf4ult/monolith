#include <libk/string.h>

size_t strlen(const char* s)
{
	const char* begin = s;
	while(*s != '\0') { ++s; }
	return (size_t)(s - begin);
}

