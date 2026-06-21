#ifndef _LIBC_STDLIB_H
#define _LIBC_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void abort(void);
int atexit(void (*)(void));
int atoi(const char*);
char* getenv(const char*);
void* malloc(size_t size);
void free(void* ptr);

void exit(int status);

#endif
