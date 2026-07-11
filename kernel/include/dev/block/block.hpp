#pragma once

#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <types.hpp>
#include <list.hpp>

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
	list_node_t list_node;
};

block_device_t* blockdev_get(dev_t dev);
block_device_t* blockdev_alloc(dev_t dev);
