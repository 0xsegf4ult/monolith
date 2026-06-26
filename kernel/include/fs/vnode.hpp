#pragma once

#include <dev/device.hpp>
#include <types.hpp>
#include <sys/cred.hpp>
#include <sys/mutex.hpp>
#include <sys/stat.hpp>
#include <stdatomic.h>

namespace vfs
{

struct fs_inode_ops;
struct fs_file_ops;
struct superblock_t;

struct vnode_t
{
	mode_t mode;
	size_t size;
	uid_t uid;
	gid_t gid;
	dev_t dev;

	uint32_t nlinks;
	atomic_uint ref;

	void* data;
	superblock_t* sb;
	fs_inode_ops* iops;
	fs_file_ops* fops;
	mutex_t lock;
};

vnode_t* vnode_new(mode_t mode);
void vnode_free(vnode_t* node);
void vnode_ref(vnode_t* node);
void vnode_put(vnode_t* node);

}
