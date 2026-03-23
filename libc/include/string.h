#ifndef _LIBC_STRING_H
#define _LIBC_STRING_H

#include <stdint.h>

size_t strlen(const char* s);
int strncmp(const char* src, const char* dst, size_t len);

const char* strerrorname_np(int errnum);
const char* strerrordesc_np(int errnum);

#endif
