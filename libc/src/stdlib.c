#include <stdlib.h>
#include <sys/syscall.h>

void abort(void)
{
	*(volatile char*)(0) = 0;
}

int atoi(const char* s)
{
	return 0;
}

char* getenv(const char* s)
{
	return nullptr;
}
