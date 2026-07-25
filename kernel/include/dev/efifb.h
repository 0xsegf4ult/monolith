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
	FBIOCGETINFO 	= 1
};

void efifb_init(struct efifb_framebuffer* fb);
void efifb_write(const char* string, size_t length);
