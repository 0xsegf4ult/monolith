#pragma once

#include <lib/types.hpp>
struct disk_t;

struct partition_t
{
	disk_t* parent;
	size_t start_lba;
	size_t block_count;

	partition_t* next;
};

partition_t* partition_create(disk_t* parent, size_t start_block, size_t block_count, size_t index);
