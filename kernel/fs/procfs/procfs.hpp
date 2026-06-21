#pragma once

namespace vfs
{
struct vfilesystem_t;
struct fs_file_ops;
}

vfs::vfilesystem_t* procfs_create();
void procfs_mkdir(const char* path);
void procfs_create(const char* path, vfs::fs_file_ops* fops); 
void procfs_remove(const char* path);

