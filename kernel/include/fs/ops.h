#pragma once

#include <types.h>

struct vm_object;
struct ventry;
struct vnode;
struct file_descriptor;

struct file_ops
{
	int (*open)(struct vnode*, int);
	int (*close)(int);
	ssize_t (*read)(struct file_descriptor*, byte*, size_t);
	ssize_t (*write)(struct file_descriptor*, const byte*, size_t);
	int (*ioctl)(struct file_descriptor*, uint64_t, uint64_t);
	int (*mmap)(struct file_descriptor*, struct vm_object*);
};

struct inode_ops
{
	struct ventry* (*lookup)(struct ventry*, const char*);
	int (*mkdir)(struct ventry*, const char*, mode_t);
	int (*create)(struct ventry*, const char*, mode_t);
	int (*mknod)(struct ventry*, const char*, mode_t, dev_t); 
	ssize_t (*getdents)(struct file_descriptor*, byte*, size_t); 
};
