#pragma once

#include <sys/time.hpp>
#include <kfmt.hpp>
#include <types.hpp>

void klog_init();
void klog_internal_nolock(const char* buffer);
void klog_internal(const char* buffer);
void klog_internal(const char* buffer, size_t length);
void klog_internal_newline();

template <typename... Args>
void generic_log(const char* fmt_string, Args&&... args)
{
	char buffer[512];
	format_to(string_span{&buffer[0], 476}, fmt_string, args...);

	klog_internal(buffer);
}

template <typename... Args>
void generic_log_nolock(const char* fmt_string, Args&&... args)
{
	char buffer[512];
	format_to(string_span{&buffer[0], 476}, fmt_string, args...);
	klog_internal_nolock(buffer);
}

namespace log
{

template <typename... Args>
void debug(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[96m");
        generic_log(fmt_string, args...);
	klog_internal("\033[0m\n");
}

template <typename... Args>
void info(const char* fmt_string, Args&&... args)
{
	auto tv = time_get_uptime();
	uint64_t time = uint64_t(tv.tv_sec) * 1000000000ULL + tv.tv_nsec;

	generic_log("\033[0m[{}.{:05}] ", time / 1000000000, (time % 1000000000) / 1000); 
	generic_log(fmt_string, args...);
	klog_internal_newline();
}

template <typename... Args>
void warn(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[33m");
        generic_log(fmt_string, args...);
	klog_internal("\033[0m\n");
}

template <typename... Args>
void error(const char* fmt_string, Args&&... args)
{
        klog_internal("\033[31m");
        generic_log(fmt_string, args...);
	klog_internal("\033[0m\n");
}

}
