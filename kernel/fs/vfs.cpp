#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

#include <fs/vfs.hpp>
#include <fs/vnode.hpp>
#include <fs/ventry.hpp>
#include <fs/lookup.hpp>
#include <fs/ramfs/ramfs.hpp>

#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <sys/err.hpp>
#include <sys/thread.hpp>

namespace vfs
{

static context_t* context = nullptr;

void init()
{
	context = (context_t*)kmalloc(sizeof(context_t));
	dcache_init();

	ramfs_init();
	auto* ramfs = lookup_fs("ramfs");
	if(!ramfs)
		panic("failed to init VFS root");

	auto* rnode = vnode_new(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
	rnode->iops = ramfs->iops;
	rnode->fops = ramfs->fops;

	context->root_node = ventry_new("/", rnode);
	context->mounts = (mount_t*)kmalloc(sizeof(mount_t));
	context->root_node->mount = context->mounts;
	context->mounts->mountpoint = context->root_node;
	context->mounts->fs = ramfs;
	context->mounts->sb = (superblock_t*)kmalloc(sizeof(superblock_t));
	context->mounts->sb->fs = context->mounts->fs;
	context->mounts->sb->data = (void*)context->root_node;
	context->mounts->sb->bdev = nullptr;

	context->mounts->next = nullptr;

	memset(context->open_files, 0, 64 * sizeof(file_descriptor_t));
}

context_t* get()
{
	return context;
}

ventry_t* get_root_dentry()
{
	return context->root_node;
}

int create(const char* path, mode_t mode)
{
	ventry_t* parent = nullptr;
	auto status = lookup(path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return status;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	ventry_t* result = nullptr;
	status = lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return -EEXIST;

	if(!parent->node->iops->create)
		return -ENOENT;

	return parent->node->iops->create(parent, basename, mode);	
}

int mkdir(const char* path, mode_t mode)
{
	ventry_t* parent = nullptr;
	auto status = lookup(path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return status;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	ventry_t* result = nullptr;
	status = lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return -EEXIST;

	if(!parent->node->iops->mkdir)
		return -ENOENT;

	return parent->node->iops->mkdir(parent, basename, mode & 0777);
}

int mknod(const char* path, mode_t mode, dev_t device)
{
	ventry_t* parent = nullptr;
	auto status = lookup(path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return status;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	ventry_t* result = nullptr;
	status = lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return -EEXIST;

	if(!parent->node->iops->mknod)
		return -ENOENT;

	return parent->node->iops->mknod(parent, basename, mode, device);
}

int unlink(const char* path)
{
	ventry_t* dentry = nullptr;
	auto status = lookup(path, &dentry, 0);
	if(status < 0)
		return status;

	if(S_ISDIR(dentry->node->mode))
	{
		return -EISDIR;
	}

	if(dentry->node->nlinks <= 1)
	{
		dentry->node->nlinks = 0;
		vnode_put(dentry->node);
	}
	else
		dentry->node->nlinks--;

	spinlock_acquire(dentry->ref.lock);
	dentry->node = nullptr;

	if(dentry->parent && dentry->parent->children == dentry)
	{
		if(dentry->sibling_next)
			dentry->sibling_next->sibling_prev = nullptr;
		dentry->parent->children = dentry->sibling_next;
	}
	else if(dentry->sibling_prev)
	{
		dentry->sibling_prev->sibling_next = dentry->sibling_next;
	}

	spinlock_release(dentry->ref.lock);
	ventry_put(dentry);

	return 0;
}

int open(const char* path, int flags)
{
	ventry_t* query = nullptr;
	auto status = lookup(path, &query, 0);
	if(status < 0)
	{
		if(status != -ENOENT)
			return status;

		if(flags & O_CREAT)
		{
			auto c_res = create(path, 0666);
			if(c_res < 0)
				return c_res;
		
			status = lookup(path, &query, 0);
		}
		else
			return -ENOENT;
	}

	ventry_ref(query);
	auto* node = query->node;
	if(!node)
	{
		ventry_put(query);
		return -EBADF;
	}

	int fs_id = 0;
	if(node->fops->open)
		fs_id = node->fops->open(node, flags);
	
	if(fs_id < 0)
	{
		ventry_put(query);
		return fs_id;
	}

	vnode_ref(node);
	
	int fd = -1;
	for(int i = 0; i < 64; i++)
	{
		if(context->open_files[i].inode == nullptr)
		{
			fd = i;
			context->open_files[i].read_pos = 0;
			context->open_files[i].write_pos = 0;
			context->open_files[i].inode = node;
			context->open_files[i].path = query;
			context->open_files[i].fs_id = fs_id;
			context->open_files[i].refcount = 1;
			break;
		}
	}

	return fd;
}

int openat(ventry_t* dir, const char* path, int flags)
{
	if(flags)
		return -EINVAL;

	ventry_t* query;
	auto status = lookup_at(dir, path, &query, 0);
	if(status < 0)
		return status;

	ventry_ref(query);
	auto* node = query->node;
	if(!node)
	{
		ventry_put(query);
		return -EBADF;
	}

	int fs_id = 0;
	if(node->fops->open)
		fs_id = node->fops->open(node, flags);

	if(fs_id < 0)
	{
		ventry_put(query);
		return fs_id;
	}

	vnode_ref(node);

	int s_fd = -1;
	for(int i = 0; i < 64; i++)
	{
		if(context->open_files[i].inode == nullptr)
		{
			s_fd = i;
			context->open_files[i].read_pos = 0;
			context->open_files[i].write_pos = 0;
			context->open_files[i].inode = node;
			context->open_files[i].path = query;
			context->open_files[i].fs_id = fs_id;
			context->open_files[i].refcount = 1;
			break;
		}
	}

	return s_fd;
}

int openat(int fd, const char* path, int flags)
{
	return openat(context->open_files[fd].path, path, flags);
}

ssize_t read(int fd, byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	if(!inode)
		return -EBADF;

	if(S_ISDIR(inode->mode))
	       return -EISDIR;	

	if(!inode->fops->read)
		return -EINVAL;

	return inode->fops->read(&context->open_files[fd], buffer, length);
}

ssize_t write(int fd, const byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	if(!inode)
		return -EBADF;

	if(S_ISDIR(inode->mode))
	       return -EISDIR;	

	if(!inode->fops->write)
		return -EINVAL;

	return inode->fops->write(&context->open_files[fd], buffer, length);
}

int close(int fd)
{
	auto* node = context->open_files[fd].inode;
	if(!node)
		return -EBADF;
	
	context->open_files[fd].refcount--;
	if(context->open_files[fd].refcount > 0)
		return 0;

	int cres = 0;
	if(node->fops->close)
		cres = node->fops->close(context->open_files[fd].fs_id);
	
	if(cres < 0)
		return cres;

	vnode_put(node);
	context->open_files[fd].read_pos = 0;
	context->open_files[fd].write_pos = 0;
	context->open_files[fd].inode = nullptr;

	ventry_put(context->open_files[fd].path);
	context->open_files[fd].path = nullptr;
	context->open_files[fd].fs_id = -1;
	return 0;
}

ssize_t seek(int fd, ssize_t offset, int flags)
{
	if(context->open_files[fd].inode == nullptr)
		return -EBADF;

	if(offset < 0)
		offset = 0;

	context->open_files[fd].read_pos = offset;
	context->open_files[fd].write_pos = offset;

	return offset;
}

int ioctl(int fd, uint64_t op, uint64_t arg)
{
	auto* inode = context->open_files[fd].inode;
	if(inode == nullptr)
		return -EBADF;

	if(!inode->fops->ioctl)
		return -ENOTTY;

	return inode->fops->ioctl(&context->open_files[fd], op, arg);
}

int stat(const char* path, stat_t* output)
{
	ventry_t* query = nullptr;
	auto status = lookup(path, &query, 0);
	if(status < 0)
		return status;

	auto* node = query->node;
	
	output->st_dev = node->dev;
	output->st_mode = node->mode;
	output->st_nlink = node->nlinks;
	output->st_uid = node->uid;
	output->st_gid = node->gid;
	output->st_size = node->size;

	return 0;
}

int fstat(int fd, stat_t* output)
{
	auto* node = context->open_files[fd].inode;
	output->st_dev = node->dev;
	output->st_mode = node->mode;
	output->st_nlink = node->nlinks;
	output->st_uid = node->uid;
	output->st_gid = node->gid;
	output->st_size = node->size;
	return 0;
}

ssize_t getdents(int fd, byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	if(!inode)
		return -EBADF;

	if(!S_ISDIR(inode->mode))
		return -ENOTDIR;

	if(!inode->iops->getdents)
		return -EINVAL;

	return inode->iops->getdents(&context->open_files[fd], buffer, length);	
}	

int dup(int fd)
{
	auto* inode = context->open_files[fd].inode;
	if(!inode)
		return -EBADF;

	context->open_files[fd].refcount++;

	return fd;
}

file_descriptor_t& get_open_fd(int fd)
{
	return context->open_files[fd];
}

}
