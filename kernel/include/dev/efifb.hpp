#pragma once

#include <types.hpp>

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
	bool gfx_mode = false;
};

struct fbinfo_t
{
	size_t width;
	size_t height;
	size_t pitch;
	uint32_t bpp;
};

enum efifb_ioctl_op
{
	FB_IOC_GETINFO = 1,
	FB_IOC_SET_TEXTMODE = 2,
	FB_IOC_SET_GFXMODE = 3
};

void efifb_init(efifb_framebuffer fb);
void efifb_write(const char* string, size_t length);
