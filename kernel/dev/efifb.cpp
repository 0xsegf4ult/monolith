#include <dev/efifb.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>

#include <fs/vfs.hpp>

#include <mm/layout.hpp>
#include <mm/vmm.hpp>

#include <lib/types.hpp>
#include <lib/klog.hpp>
#include <lib/kstd.hpp>

extern uint8_t font_data[];

static efifb_framebuffer fb;

void efifb_init(efifb_framebuffer framebuffer)
{
	auto* dev = chardev_alloc(dev_t{6, 0});
	dev->data = (void*)&fb;

	vfs::mknod("/dev/fb0", S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, dev_t{6, 0});	

	fb = framebuffer;
	log::info("efifb: fb0: {}x{}", fb.width, fb.height);
	fb.wc = fb.width / 8;
	fb.hc = fb.height / 16;
	fb.bpp = (fb.bpp + 7) / 8;

	//FIXME: map as WC
	vm_map_range(fb.address, fb.address + mm::direct_mapping_offset, fb.height * fb.pitch, vm_write);
	fb.address += mm::direct_mapping_offset;
		
	fb.cursor_x = 0;
	fb.cursor_y = 0;
}

void efifb_check_scroll()
{
	if(fb.cursor_x >= fb.wc)
	{
		fb.cursor_x = 0;
		fb.cursor_y++;
	}

	if(fb.cursor_y >= fb.hc)
	{
		uint32_t* second_line = (uint32_t*)(fb.address + (16 * fb.pitch));
		size_t sz = (fb.hc - 1) * 16 * fb.pitch;
		memcpy((void*)fb.address, second_line, sz);

		uint32_t* last_line = (uint32_t*)(fb.address + sz);
		for(int i = 0; i < 16; i++)
		{
			for(int j = 0; j < fb.width; j++)
				last_line[j] = 0;

			last_line = (uint32_t*)((virtaddr_t)last_line + fb.pitch);
		}

		fb.cursor_x = 0;
		fb.cursor_y = fb.hc - 1; 
	}
}

void efifb_putchar(char c)
{
	if(c == '\r')
	{
		fb.cursor_x = 0;
		return;
	}
	else if(c == '\n')
	{
		fb.cursor_x = 0;
		fb.cursor_y++;

		efifb_check_scroll();
		return;
	}
	else if(c == '\b')
	{
		if(fb.cursor_x >= 1)
			fb.cursor_x--;

		c = ' ';
	}

	auto x = fb.cursor_x * 8;
	auto y = fb.cursor_y * 16;
	auto* ptr = reinterpret_cast<uint32_t*>(fb.address + (y * fb.pitch) + (x * fb.bpp));
	uint8_t* font_ptr = font_data + c * 16;

	for(int y = 0; y < 16; y++)
	{
		int mask = 1 << 7;
		for(int x = 0; x < 8; x++)
		{
			ptr[x] = *font_ptr & mask ? 0xBFBFBF : 0;
			mask >>= 1;
		}
		font_ptr++;
		ptr = (uint32_t*)((virtaddr_t)(ptr) + fb.pitch);
	}

	fb.cursor_x++;
	efifb_check_scroll();
}

void efifb_write(const char* string, size_t length)
{
	for(size_t i = 0; i < length; i++)
	{
		if(!string[i])
			break;

		efifb_putchar(string[i]);
	}
}
