#pragma once

#include <types.hpp>

inline void* operator new(size_t, void* p) { return p; }
inline void* operator new[](size_t, void* p) { return p; }
inline void operator delete(void*, void*) {};
inline void operator delete[](void*, void*) {};

char* strncpy(char* dst, const char* src, size_t n);
size_t strncmp(const char* src, const char* dst, size_t len);
extern "C" void* memcpy(void* dest, const void* src, size_t n);
extern "C" void* memset(void* dest, int data, size_t n);
extern "C" void* memmove(void* dest, const void* src, size_t n);
extern "C" int memcmp(const void* s1, const void* s2, size_t n);

constexpr uint64_t align_up(uint64_t value, uint64_t alignment)
{
        return (value + alignment - 1) & (~(alignment - 1));
}

constexpr uint64_t align_down(uint64_t value, uint64_t alignment)
{
        return value & ~(alignment - 1);
}

