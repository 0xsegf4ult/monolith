#pragma once

#include <types.h>
#include <stdarg.h>

ssize_t vsprintf(char* buf, const char* fmt, va_list arg);
ssize_t sprintf(char* buf, const char* fmt, ...);
