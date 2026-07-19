#pragma once

#include <types.h>

struct file_ops;

struct char_device
{
	dev_t dev;
	void* data;
	struct file_ops* fops;
	struct char_device* next;
};

struct char_device* chardev_new(dev_t dev);
struct char_device* chardev_lookup(dev_t dev);

struct block_device;
struct blockdev_ops
{
	ssize_t (*pread_blocks)(struct block_device*, byte*, size_t, off_t);
};

struct block_device
{
	dev_t dev;
	void* data;
	struct file_ops* fops;
	struct blockdev_ops* bd_ops;
	struct block_device* next;
};

struct block_device* blockdev_new(dev_t dev);
struct block_device* blockdev_lookup(dev_t dev);

static inline dev_t make_dev(uint32_t major, uint32_t minor)
{
	return ((uint64_t)major << 32) | minor;
}

static inline uint32_t dev_major(dev_t dev)
{
	return (dev >> 32) & 0xFFFFFFFF;
}

static inline uint32_t dev_minor(dev_t dev)
{
	return dev & 0xFFFFFFFF;
}
