#include <dev/character.hpp>
#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <mm/slab.hpp>

static char_device_t* devices = nullptr;
static vfs::fs_ops default_char_fops = {};

char_device_t* chardev_alloc(dev_t dev)
{
	auto* device = (char_device_t*)kmalloc(sizeof(char_device_t));
	device->dev = dev;
	device->ops = &default_char_fops;
	device->data = nullptr;
	device->next = devices;
	devices = device;
	return device;
}

char_device_t* chardev_get(dev_t dev)
{
	char_device_t* cur = devices;

	while(cur)
	{
		if(cur->dev == dev)
			return cur;
		
		cur = cur->next;
	}

	return nullptr;
}
