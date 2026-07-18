#pragma once

#include <stdint.h>
#include <stddef.h>

typedef int64_t ssize_t;
typedef int64_t off_t;
typedef uint64_t physaddr_t;
typedef uint64_t virtaddr_t;
typedef uint8_t byte;
typedef int32_t pid_t;
typedef uint32_t sigset_t;
typedef int64_t time_t;

static inline uint64_t align_up(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

static inline uint64_t align_down(uint64_t value, uint64_t alignment)
{
	return value & ~(alignment - 1);
}
