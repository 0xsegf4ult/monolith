#pragma once

#define CONFIG_ARCH x86_64
#define CONFIG_ARCH_PATH "arch/x86_64/config.hpp"

#define CONFIG_MAX_CPUS 64
#define CONFIG_HZ 1000

#include CONFIG_ARCH_PATH
