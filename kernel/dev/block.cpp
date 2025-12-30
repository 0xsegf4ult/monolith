#include <dev/block.hpp>
#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <mm/slab.hpp>

static block_device_t* devices = nullptr;
static vfs::fs_ops default_block_fops = {};

block_device_t* blockdev_alloc(dev_t dev)
{
	auto* device = (block_device_t*)kmalloc(sizeof(block_device_t));
	device->dev = dev;
	device->ops = &default_block_fops;
	device->data = nullptr;
	device->next = devices;
	devices = device;
	return device;
}

block_device_t* blockdev_get(dev_t dev)
{
	block_device_t* cur = devices;

	while(cur)
	{
		if(cur->dev == dev)
			return cur;
		
		cur = cur->next;
	}

	return nullptr;
}
