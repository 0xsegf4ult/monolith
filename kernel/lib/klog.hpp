#pragma once

#include <arch/x86_64/timer.hpp>
#include <lib/kfmt.hpp>
#include <lib/types.hpp>

void klog_internal(const char* buffer);
void klog_internal(const char* buffer, size_t length);
void klog_internal_newline();

template <typename... Args>
void generic_log(const char* fmt_string, Args&&... args)
{
	static char buffer[512];
	format_to(string_span{&buffer[0], 512}, fmt_string, args...);

	klog_internal(buffer);
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
	auto tick = timer::get_ticks() * 100;
	auto tick_wp = tick / 100000;
	auto tick_fp = tick % 100000;
	generic_log("\033[0m[{}.{:05}] ", tick_wp, tick_fp); 
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
