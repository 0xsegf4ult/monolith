#ifndef _LIBC_SYS_FRAMEBUFFER
#define _LIBC_SYS_FRAMEBUFFER

#include <stdint.h>
#include <stddef.h>

typedef struct fbinfo
{
	size_t width;
	size_t height;
	size_t pitch;
	uint32_t bpp;
} fbinfo_t;

#define FB_IOC_GETINFO 1
#define FB_IOC_SET_TEXTMODE 2
#define FB_IOC_SET_GFXMODE 3

#endif
