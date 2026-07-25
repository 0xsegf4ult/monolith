#pragma once

#include <libk/list.h>
#include <types.h>

struct file_ops;
struct blockdev_ops;

struct disk
{
	char name[32];
	dev_t dev;
	void* data;

	size_t block_count;
	size_t block_size;

	list_head_t partitions;
	size_t partcount;
};

struct partition
{
	struct disk* parent;
	size_t start_lba;
	size_t block_count;

	list_node_t list_node;
};

struct disk* disk_create(dev_t blk, const char* name, void* priv_data, struct file_ops* fops, struct blockdev_ops* bops);
void disk_scan(struct disk* disk);
struct partition* disk_create_partition(struct disk* parent, size_t start_block, size_t block_count, size_t index);
