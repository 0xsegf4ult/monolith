#ifndef _LIBC_SYS_MMAN_H
#define _LIBC_SYS_MMAN_H

#include <sys/types.h>
#include <sys/syscall.h>

typedef enum
{
	MAP_SHARED = 0x1,
        MAP_PRIVATE = 0x2,
	MAP_FIXED = 0x4,
        MAP_ANONYMOUS = 0x8,
	MAP_ANON = 0x8
} mmap_flags;

typedef enum
{
        PROT_NONE = 0,
        PROT_READ = 0x1,
        PROT_WRITE = 0x2,
        PROT_EXEC = 0x4
} mmap_prot_flags;

#define MAP_FAILED ((void*) -1)

inline void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
        return (void*)_syscall(SYS_MMAP, (uint64_t)addr, (uint64_t)length, (uint64_t)prot, (uint64_t) flags, (uint64_t)fd, (uint64_t)offset);
}

inline int munmap(void* addr, size_t length)
{
        return (int)_syscall(SYS_MUNMAP, (uint64_t)addr, (uint64_t)length, 0, 0, 0, 0);
}

#endif
