#pragma once

namespace vfs
{
struct vfilesystem_t;
}

vfs::vfilesystem_t* procfs_create();
void procfs_mkdir(const char* path);
void procfs_remove(const char* path);

