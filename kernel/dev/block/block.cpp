#include <dev/block/block.hpp>
#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <mm/slab.hpp>

static list_head_t devices = {&devices, &devices};
static vfs::fs_file_ops default_block_fops = {};

block_device_t* blockdev_alloc(dev_t dev)
{
	auto* device = (block_device_t*)kmalloc(sizeof(block_device_t));
	device->dev = dev;
	device->fops = &default_block_fops;
	device->data = nullptr;
	list_node_init(device->list_node);
	list_add_tail(devices, device->list_node);
	return device;
}

block_device_t* blockdev_get(dev_t dev)
{
	block_device_t* cur;

	list_for_each_entry(cur, devices, list_node)
	{
		if(cur->dev == dev)
			return cur;
	}

	return nullptr;
}
