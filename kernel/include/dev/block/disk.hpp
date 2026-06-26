#pragma once

#include <dev/device.hpp>
#include <types.hpp>

namespace vfs
{
struct fs_file_ops;
};

struct partition_t;
struct blockdev_ops;

struct disk_t
{
	char name[32];
	dev_t dev;
	void* data;

	size_t block_count;
	size_t block_size;

	partition_t* partition_list_head;
	partition_t* partition_list_tail;
	size_t partcount;
};

disk_t* disk_create(dev_t blk, const char* name, void* priv_data, vfs::fs_file_ops* fops, blockdev_ops* bops);
void disk_scan(disk_t* disk);
