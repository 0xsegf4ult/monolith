#pragma once

#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <lib/types.hpp>

struct block_device_t 
{
	dev_t dev;
	vfs::fs_ops* ops;
	void* data;
	block_device_t* next;
};

block_device_t* blockdev_get(dev_t dev);
block_device_t* blockdev_alloc(dev_t dev);
