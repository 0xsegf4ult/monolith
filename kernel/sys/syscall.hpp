#pragma once

#include <lib/types.hpp>

enum class syscall_id : uint64_t
{
	OPEN,
	CLOSE,
	READ,
	WRITE,
	SPAWN,
	EXIT,
	WAIT,
	IOCTL,
	STAT,
	GETDENTS,
	CHDIR,
	MKDIR,
	DEBUG_PRINT
};

struct cpu_context_t;
void syscall_handler(cpu_context_t* ctx);
