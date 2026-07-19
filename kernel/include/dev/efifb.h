#pragma once

#include <types.h>

struct efifb_framebuffer
{
	virtaddr_t address;
	size_t width;
	size_t height;
	size_t pitch;
	uint32_t bpp;
	uint32_t wc;
	uint32_t hc;

	uint32_t cursor_x;
	uint32_t cursor_y;
	bool gfx_mode;
};

typedef struct 
{
	size_t width;
	size_t height;
	size_t pitch;
	uint32_t bpp;
} fbinfo_t;

enum efifb_ioctl_op
{
	FB_IOC_GETINFO = 1,
	FB_IOC_SET_TEXTMODE = 2,
	FB_IOC_SET_GFXMODE = 3
};

void efifb_init(struct efifb_framebuffer* fb);
void efifb_write(const char* string, size_t length);
