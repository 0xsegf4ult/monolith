#pragma once

#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <lib/types.hpp>

struct block_device_t;
typedef ssize_t (*pread_blocks_t)(block_device_t*, byte*, size_t, size_t);

struct blockdev_ops
{
	pread_blocks_t pread_blocks = nullptr;
};

struct block_device_t 
{
	dev_t dev;
	vfs::fs_file_ops* fops;
	blockdev_ops* bops;
	void* data;
	block_device_t* next;
};

block_device_t* blockdev_get(dev_t dev);
block_device_t* blockdev_alloc(dev_t dev);
