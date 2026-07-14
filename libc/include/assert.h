#ifndef _LIBC_ASSERT_H
#define _LIBC_ASSERT_H

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) __assert((int)expr, #expr, __FILE__, __LINE__, __func__)
#endif

void __assert(int, const char*, const char*, int, const char*);

#endif
