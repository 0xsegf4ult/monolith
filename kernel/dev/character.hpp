#pragma once

#include <dev/device.hpp>
#include <fs/ops.hpp>
#include <lib/types.hpp>

struct char_device_t
{
	dev_t dev;
	vfs::fs_file_ops* fops;
	void* data;
	char_device_t* next;
};

char_device_t* chardev_get(dev_t dev);
char_device_t* chardev_alloc(dev_t dev);
