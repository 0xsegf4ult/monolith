#pragma once

#include <lib/types.hpp>
#include <dev/device.hpp>

namespace vfs
{

struct ventry_t;
struct vnode_t;
struct file_descriptor_t;

typedef ventry_t* (*fs_lookup_t)(ventry_t*, const char*);
typedef int (*fs_mkdir_t)(ventry_t*, const char*);
typedef int (*fs_create_t)(ventry_t*, const char*);
typedef int (*fs_mknod_t)(ventry_t*, const char*, char, dev_t); 
typedef int (*fs_open_t)(vnode_t*, int);
typedef int (*fs_close_t)(int);
typedef ssize_t (*fs_read_t)(file_descriptor_t*, byte*, size_t);
typedef ssize_t (*fs_write_t)(file_descriptor_t*, const byte*, size_t);
typedef int (*fs_ioctl_t)(file_descriptor_t*, uint64_t, uint64_t);
typedef ssize_t (*fs_getdents_t)(file_descriptor_t*, byte*, size_t); 

struct fs_ops
{
	fs_lookup_t lookup = nullptr;
	fs_create_t create = nullptr;
	fs_mkdir_t mkdir = nullptr;
	fs_mknod_t mknod = nullptr;
	fs_open_t open = nullptr;	
	fs_close_t close = nullptr;
	fs_read_t read = nullptr;
	fs_write_t write = nullptr;
	fs_ioctl_t ioctl = nullptr;
	fs_getdents_t getdents = nullptr;
};

}
