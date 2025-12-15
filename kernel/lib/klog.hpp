#pragma once

#include <lib/kfmt.hpp>
#include <lib/types.hpp>

void klog_internal(const char* buffer);
void klog_internal_newline();

template <typename... Args>
void generic_log(const char* fmt_string, Args&&... args)
{
	static char buffer[512];
	format_to(string_span{&buffer[0], 512}, fmt_string, args...);

	klog_internal(buffer);
	klog_internal_newline();
}

namespace log
{

template <typename... Args>
void debug(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[96m[debug]\033[0m ");
        generic_log(fmt_string, args...);
}

template <typename... Args>
void info(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[32m[info]\033[0m ");
        generic_log(fmt_string, args...);
}

template <typename... Args>
void warn(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[33m[warn]\033[0m ");
        generic_log(fmt_string, args...);
}

template <typename... Args>
void error(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[31m[error]\033[0m ");
        generic_log(fmt_string, args...);
}

}
