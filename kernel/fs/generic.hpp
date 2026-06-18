#pragma once

#include <fs/vfs.hpp>
#include <dev/device.hpp>
#include <lib/types.hpp>

vfs::ventry_t* generic_fs_lookup(vfs::ventry_t* parent, const char* path);
int generic_fs_create(vfs::ventry_t* parent, const char* path, mode_t mode);
int generic_fs_mkdir(vfs::ventry_t* parent, const char* path, mode_t mode);
int generic_fs_mknod(vfs::ventry_t* parent, const char* path, mode_t mode, dev_t dev);
int generic_fs_open(vfs::vnode_t* node, int flags);
int generic_fs_close(int fd);
ssize_t generic_fs_getdents(vfs::file_descriptor_t* file, byte* buffer, size_t length); 
