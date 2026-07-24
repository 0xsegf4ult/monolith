#pragma once

#include <types.h>

enum MAP_FLAGS
{
	MAP_SHARED 	= 0x1,
	MAP_PRIVATE	= 0x2,
	MAP_ANONYMOUS	= 0x20
};

void* sys_mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset);
int sys_munmap(void* addr, size_t size);
int sys_mprotect(void* addr, size_t size, int prot);
