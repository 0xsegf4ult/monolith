#pragma once

#include <fs/vfs.h>
#include <types.h>

struct ventry* generic_fs_lookup(struct ventry* parent, const char* path);
int generic_fs_create(struct ventry* parent, const char* path, mode_t mode);
int generic_fs_mkdir(struct ventry* parent, const char* path, mode_t mode);
int generic_fs_mknod(struct ventry* parent, const char* path, mode_t mode, dev_t dev);
ssize_t generic_fs_getdents(struct file_descriptor* file, byte* buffer, size_t length); 
