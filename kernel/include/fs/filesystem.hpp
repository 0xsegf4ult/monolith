#pragma once

#include <types.hpp>

namespace vfs
{

struct fs_inode_ops;
struct fs_file_ops;
struct fs_super_ops;

enum vfilesystem_flags : uint32_t
{
	FS_FLAG_NODEV = 1
};

struct vfilesystem_t
{
        char name[32];
	uint32_t flags;

        fs_inode_ops* iops;
        fs_file_ops* fops;
        fs_super_ops* sb_ops;

        vfilesystem_t* next;
};

void register_fs(vfilesystem_t* fs, const char* name);
vfilesystem_t* lookup_fs(const char* name);

}
