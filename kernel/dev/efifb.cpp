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

	vfs::mknod("/dev/fb0", 'c', dev_t{6, 0});	

	fb = framebuffer;
	log::info("efifb: fb0: {}x{}", fb.width, fb.height);

	//FIXME: map as WC
	vm_map_range(fb.address, fb.address + mm::direct_mapping_offset, fb.width * fb.height * fb.bpp, vm_write);
	fb.address += mm::direct_mapping_offset;
	fb.bpp /= 4;
		
	fb.cursor_x = 0;
	fb.cursor_y = 0;
}

void efifb_scroll()
{
	fb.cursor_y -= 16;
	size_t offset = (fb.height - 16) * fb.width;
	log::debug("copy {:#x} - {:#x} fbscroll", (uint32_t*)(fb.address) + (fb.width * 16), (uint32_t*)(fb.address) + (fb.width * 16) + offset);
	memcpy((uint32_t*)fb.address, (uint32_t*)(fb.address) + (fb.width * 16), offset);
	log::debug("memset clearfb");
	memset((uint32_t*)(fb.address) + offset, 0, fb.width * 16);
}

void efifb_putchar(char c)
{
	bool done = false;

	if(c == '\n')
	{
		fb.cursor_x = 0;
		fb.cursor_y += 16;
		done = true;
	}
	else if(c == '\b')
	{
		if(fb.cursor_x >= 8)
			fb.cursor_x -= 8;

		c = ' ';
	}

	while(fb.cursor_y >= fb.height - 16)
		efifb_scroll();

	if(done)
		return;

	auto* ptr = reinterpret_cast<uint32_t*>(fb.address) + (fb.cursor_y * fb.width) + fb.cursor_x;
	uint8_t* font_ptr = font_data + c * 16;

	for(int y = 0; y < 16; y++)
	{
		int mask = 1 << 7;
		for(int x = 0; x < 8; x++)
		{
			(*ptr++) = *font_ptr & mask ? 0xCCCCCC : 0x0;
			mask >>= 1;
		}
		font_ptr++;
		ptr += (fb.width - 8);
	}

	fb.cursor_x += 8;

	if(fb.cursor_x >= fb.width)
	{
		fb.cursor_x = 0;
		fb.cursor_y += 16;
	}
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
