#pragma once

#include <lib/kfmt.hpp>
#include <lib/types.hpp>

inline void* operator new(size_t, void* p) { return p; }
inline void* operator new[](size_t, void* p) { return p; }
inline void operator delete(void*, void*) {};
inline void operator delete[](void*, void*) {};

void panic_inner(const char* string);

constexpr void panic(const char* string)
{
	panic_inner(string);
}

template <typename... Args>
void panic(const char* fmt_string, Args&&... args)
{
	static char buffer[512];
	format_to(string_span{&buffer[0], 512}, fmt_string, args...);
	panic_inner(buffer);
}

size_t strncmp(const char* src, const char* dst, size_t len);
extern "C" void* memcpy(void* dest, const void* src, size_t n);
extern "C" void* memset(void* dest, int data, size_t n);
extern "C" void* memmove(void* dest, const void* src, size_t n);
extern "C" int memcmp(const void* s1, const void* s2, size_t n);
