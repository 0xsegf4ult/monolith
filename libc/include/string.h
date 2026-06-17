#ifndef _LIBC_STRING_H
#define _LIBC_STRING_H

#include <stddef.h>
#include <stdint.h>

size_t strlen(const char* s);
char* strncpy(char* dst, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* src, const char* dst, size_t n);
char* strchr(const char* s, int c);
char* strtok_r(char* s1, const char* s2, char** saveptr);
char* strtok(char* s1, const char* s2);

const char* strerrorname_np(int errnum);
const char* strerrordesc_np(int errnum);

#endif
