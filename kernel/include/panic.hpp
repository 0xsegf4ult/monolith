#pragma once

#include <kfmt.hpp>
#include <types.hpp>

void panic(const char* string);
void panic_prepare();
void panic_complete();

template <typename... Args>
void panic(const char* fmt_string, Args&&... args)
{
	static char buffer[512];
	format_to(string_span{&buffer[0], 512}, fmt_string, args...);
	panic(buffer);
}
