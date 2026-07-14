#ifndef _LIBC_STDIO_H
#define _LIBC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

typedef struct
{
	int fd;
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* path, const char* mode);
int fflush(FILE* file);
int fclose(FILE* file);

size_t fread(void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
void setbuf(FILE*, char*);

int fprintf(FILE* file, const char* fmt, ...);
int vfprintf(FILE* file, const char* fmt, va_list);
int printf(const char* fmt, ...);
int sprintf(char* buffer, const char* fmt, ...);

#endif
