#pragma once

#include <libk/list.h>

struct filesystem;
struct block_device;
struct ventry;

struct superblock 
{
        struct filesystem* fs;
        struct block_device* bdev;

	void* data;
};

struct mount
{
        struct filesystem* fs;
        struct superblock* sb;
        struct ventry* mountpoint;
	list_node_t list_node;
};

struct super_ops
{
	int (*init)(struct block_device*, struct superblock**); 
	int (*root)(struct superblock*, struct ventry**);
};

int vfs_mount(const char* src, const char* target, const char* fstype);
