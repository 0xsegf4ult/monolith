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
};

void efifb_init(efifb_framebuffer fb);
void efifb_write(const char* string, size_t length);
