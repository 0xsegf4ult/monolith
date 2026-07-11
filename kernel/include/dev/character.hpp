#pragma once

#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <types.hpp>
#include <list.hpp>

struct char_device_t
{
	dev_t dev;
	vfs::fs_file_ops* fops;
	void* data;
	list_node_t list_node;
};

char_device_t* chardev_get(dev_t dev);
char_device_t* chardev_alloc(dev_t dev);
