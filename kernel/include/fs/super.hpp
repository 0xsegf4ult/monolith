#pragma once

#include <list.hpp>

struct block_device_t;

namespace vfs
{

struct ventry_t;
struct vfilesystem_t;

struct superblock_t
{
        vfilesystem_t* fs;
        block_device_t* bdev;

	void* data;
};

struct mount_t
{
        vfilesystem_t* fs;
        superblock_t* sb;
        ventry_t* mountpoint;
	list_node_t list_node;
};

typedef int (*sb_init_t)(block_device_t*, superblock_t**); 
typedef int (*sb_root_t)(superblock_t*, ventry_t**);

struct fs_super_ops
{
	sb_init_t init = nullptr;
	sb_root_t root = nullptr;
};

int mount(const char* src, const char* target, const char* fstype);

}
