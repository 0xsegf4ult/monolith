#pragma once

#include <stddef.h>

size_t strlen(const char* s);
char* strncpy(char* dst, const char* src, size_t n);
int strncmp(const char* s1, const char* s2, size_t n);

void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
