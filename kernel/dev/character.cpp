#include <dev/character.hpp>
#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <mm/slab.hpp>
#include <list.hpp>

static list_head_t devices = {&devices, &devices};
static vfs::fs_file_ops default_char_fops = {};

char_device_t* chardev_alloc(dev_t dev)
{
	auto* device = (char_device_t*)kmalloc(sizeof(char_device_t));
	device->dev = dev;
	device->fops = &default_char_fops;
	device->data = nullptr;
	list_node_init(device->list_node);
	list_add_tail(devices, device->list_node);
	return device;
}

char_device_t* chardev_get(dev_t dev)
{
	char_device_t* cur;

	list_for_each_entry(cur, devices, list_node)
	{
		if(cur->dev == dev)
			return cur;
	}

	return nullptr;
}
