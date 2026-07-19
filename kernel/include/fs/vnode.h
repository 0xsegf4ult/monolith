#pragma once

#include <sys/mutex.h>
#include <types.h>
#include <stdatomic.h>

struct inode_ops;
struct file_ops;
struct superblock;

struct vnode
{
	mode_t mode;
	size_t size;
	uid_t uid;
	gid_t gid;
	dev_t dev;

	nlink_t nlink;
	_Atomic uint32_t ref;

	void* data;
	struct superblock* sb;
	struct inode_ops* iops;
	struct file_ops* fops;
	mutex_t lock;
};

struct vnode* vnode_new(mode_t mode);
void vnode_free(struct vnode* node);
void vnode_ref(struct vnode* node);
void vnode_put(struct vnode* node);
