#include <stdio.h>

FILE* fopen(const char* path, const char* mode)
{
	return NULL;
}

int fflush(FILE* file)
{
	return 0;
}

int fclose(FILE* file)
{
	return 0;
}

size_t fread(void*, size_t, size_t, FILE*)
{
	return 0;
}

int fseek(FILE*, long, int)
{
	return 0;
}

long ftell(FILE*)
{
	return 0;
}

size_t fwrite(const void*, size_t, size_t, FILE*)
{
	return 0;
}

void setbuf(FILE*, char*)
{
}
