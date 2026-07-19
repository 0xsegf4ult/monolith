#pragma once

#include <libk/list.h>
#include <stdint.h>

struct inode_ops;
struct file_ops;
struct super_ops;

enum filesystem_flags : uint32_t
{
	FS_FLAG_NODEV = 1
};

struct filesystem
{
        char name[32];
	uint32_t flags;

        struct inode_ops* iops;
        struct file_ops* fops;
        struct super_ops* sb_ops;

	list_node_t list_node;
};

void filesystem_register(struct filesystem* fs, const char* name);
struct filesystem* filesystem_lookup(const char* name);

