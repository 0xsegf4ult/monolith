#include <dev/efifb.h>
#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <mm/vmm.h>
#include <sys/device.h>
#include <libk/string.h>
#include <errno.h>
#include <klog.h>
#include <types.h>

extern uint8_t font_data[];

static struct efifb_framebuffer* fb;

ssize_t efifb_file_write(struct file_descriptor* file, const byte* buffer, size_t length)
{
	struct efifb_framebuffer* fb = (struct efifb_framebuffer*)(chardev_lookup(file->inode->dev)->data);
	if(fb->gfx_mode)
	{
		memcpy((void*)fb->address, buffer, length);
	}
	else
	{
		//FIXME: using static fb, only one can exist for now
		efifb_write((const char*)buffer, length);
	}
	
	return length;
}	

int efifb_ioctl(struct file_descriptor* file, uint64_t op, uint64_t arg)
{
	struct efifb_framebuffer* fb = (struct efifb_framebuffer*)(chardev_lookup(file->inode->dev)->data);
	if(!fb)
		return -ENODEV;

	switch(op)
	{
	case FBIOCGETINFO:
	{
		fbinfo_t info;
		info.width = fb->width;
		info.height = fb->height;
		info.bpp = fb->bpp;
		info.pitch = fb->pitch;

		memcpy((void*)arg, &info, sizeof(fbinfo_t));

		return 0;
	}
	case FBIOCSETTXT:
	{
		fb->gfx_mode = false;
		return 0;
	}
	case FBIOCSETGFX:
	{
		fb->gfx_mode = true;
		return 0;
	}
	}

	return -EINVAL;
}

static int efifb_mmap(struct file_descriptor* file, struct vm_object* vm)
{
	struct efifb_framebuffer* fb = (struct efifb_framebuffer*)(chardev_lookup(file->inode->dev)->data);
	if(!fb)
		return -ENODEV;

	vm->flags |= VM_FLAG_DEVICE;
	mmu_map_range(vm->space->mmu_root, fb->address - VM_DMAP_BASE, vm->base, vm->length, vm->prot | PROT_WRITECOMBINE, 0);
	return 0;
}

static struct file_ops efifb_fops = 
{
	.write = efifb_file_write,
	.ioctl = efifb_ioctl,
	.mmap = efifb_mmap
};

void efifb_init(struct efifb_framebuffer* framebuffer)
{
	fb = framebuffer;
	klog("efifb: fb0: %ux%u\n", fb->width, fb->height);
	
	dev_t id = make_dev(6, 0);
	
	struct char_device* dev = chardev_new(id);
	dev->data = (void*)fb;
	dev->fops = &efifb_fops;

	vfs_mknod("/dev/fb0", S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, id);	

	fb->wc = fb->width / 8;
	fb->hc = fb->height / 16;
	fb->bpp = (fb->bpp + 7) / 8;
	fb->gfx_mode = false;

	mmu_map_range(vm_get_kernel_space()->mmu_root, fb->address, fb->address + VM_DMAP_BASE, fb->height * fb->pitch, PROT_READ | PROT_WRITE | PROT_WRITECOMBINE, 0);
	fb->address += VM_DMAP_BASE;
		
	fb->cursor_x = 0;
	fb->cursor_y = 0;
}

void efifb_check_scroll()
{
	if(fb->cursor_x >= fb->wc)
	{
		fb->cursor_x = 0;
		fb->cursor_y++;
	}

	if(fb->cursor_y >= fb->hc)
	{
		uint32_t* second_line = (uint32_t*)(fb->address + (16 * fb->pitch));
		size_t sz = (fb->hc - 1) * 16 * fb->pitch;
		memcpy((void*)fb->address, second_line, sz);

		uint32_t* last_line = (uint32_t*)(fb->address + sz);
		for(int i = 0; i < 16; i++)
		{
			for(int j = 0; j < fb->width; j++)
				last_line[j] = 0;

			last_line = (uint32_t*)((virtaddr_t)last_line + fb->pitch);
		}

		fb->cursor_x = 0;
		fb->cursor_y = fb->hc - 1; 
	}
}

void efifb_putchar(char c)
{
	bool no_advance = false;

	if(c == '\r')
	{
		fb->cursor_x = 0;
		return;
	}
	else if(c == '\n')
	{
		fb->cursor_x = 0;
		fb->cursor_y++;

		efifb_check_scroll();
		return;
	}
	else if(c == 0x7f)
	{
		if(fb->cursor_x >= 1)
		{
			fb->cursor_x--;
			no_advance = true;
		}

		c = ' ';
	}

	uint32_t x = fb->cursor_x * 8;
	uint32_t y = fb->cursor_y * 16;
	uint32_t* ptr = (uint32_t*)(fb->address + (y * fb->pitch) + (x * fb->bpp));
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
		ptr = (uint32_t*)((virtaddr_t)(ptr) + fb->pitch);
	}

	if(!no_advance)
		fb->cursor_x++;
	efifb_check_scroll();
}

void efifb_write(const char* string, size_t length)
{
	if(fb->gfx_mode)
		return;

	for(size_t i = 0; i < length; i++)
	{
		if(!string[i])
			break;

		efifb_putchar(string[i]);
	}
}
