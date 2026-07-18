#pragma once

#include <stdint.h>
#include <stddef.h>

constexpr size_t CONFIG_MAX_CPUS = 64;
constexpr uint64_t CONFIG_HZ = 1000;
constexpr size_t CONFIG_PAGE_SIZE = 0x1000;

#define ARCH_HAS_MEMCPY
#define ARCH_HAS_MEMSET
