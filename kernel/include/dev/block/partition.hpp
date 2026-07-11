#pragma once

#include <types.hpp>
#include <list.hpp>

struct disk_t;

struct partition_t
{
	disk_t* parent;
	size_t start_lba;
	size_t block_count;

	list_node_t list_node;
};

partition_t* partition_create(disk_t* parent, size_t start_block, size_t block_count, size_t index);
