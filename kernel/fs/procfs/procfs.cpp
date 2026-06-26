#include <fs/procfs/procfs.hpp>
#include <fs/generic.hpp>
#include <fs/lookup.hpp>
#include <fs/vfs.hpp>
#include <fs/super.hpp>
#include <mm/slab.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>
#include <sys/thread.hpp>
#include <sys/err.hpp>

using namespace vfs;

static fs_inode_ops procfs_iops =
{
	.lookup = generic_fs_lookup,
	.create = nullptr,
	.mkdir = nullptr,
	.mknod = nullptr,
	.getdents = generic_fs_getdents
};

static fs_file_ops procfs_fops =
{
	.open = generic_fs_open,
	.close = generic_fs_close,
	.read = nullptr,
	.write = nullptr
};

static superblock_t* g_procfs = nullptr;

int procfs_super_init(block_device_t* bdev, superblock_t** out_sb)
{
	if(g_procfs)
		return -EBUSY;

	auto* sb = (superblock_t*)kmalloc(sizeof(superblock_t));

	auto* node = vnode_new(S_IFDIR | 0755);
	node->iops = &procfs_iops;
        node->fops = &procfs_fops;

        auto* dentry = ventry_new("proc", node);
	sb->data = (void*)dentry;

	sb->bdev = nullptr;
	g_procfs = sb;
	*out_sb = sb;
	return 0;
}

int procfs_super_root(superblock_t* sb, ventry_t** out_root)
{
	*out_root = (ventry_t*)sb->data;
	return 0;
}

static fs_super_ops procfs_sb_ops
{
	.init = procfs_super_init,
	.root = procfs_super_root
};

void procfs_init()
{
	auto* fs = (vfilesystem_t*)kmalloc(sizeof(vfilesystem_t));
	fs->flags = FS_FLAG_NODEV;
	fs->iops = &procfs_iops;
	fs->fops = &procfs_fops;
	fs->sb_ops = &procfs_sb_ops;

	register_fs(fs, "procfs");
}

void procfs_mkdir(const char* path)
{
	if(!g_procfs)
		return;

	ventry_t* parent = nullptr;
	auto status = vfs::lookup_at((ventry_t*)g_procfs->data, path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return;

	ventry_t* result = nullptr;
	status = lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return;

	generic_fs_mkdir(parent, basename, 0755);
}

void procfs_create(const char* path, vfs::fs_file_ops* fops, void* priv_data)
{
	if(!g_procfs)
		return;

	ventry_t* parent = nullptr;
	auto status = vfs::lookup_at((ventry_t*)g_procfs->data, path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return;

	ventry_t* result = nullptr;
	status = lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return;

	auto* inode = vnode_new(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH);
        inode->iops = parent->node->iops;
	inode->fops = fops;
	inode->data = priv_data;

        auto* dirent = ventry_new(basename, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);

        if(parent->children)
	{
		parent->children->sibling_prev = dirent;
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;

        mutex_unlock(parent->node->lock);
}

void procfs_remove(const char* path)
{
	if(!g_procfs)
		return;

	ventry_t* dentry = nullptr;
	auto status = vfs::lookup_at((ventry_t*)g_procfs->data, path, &dentry, 0);
	if(status < 0)
		return;

	spinlock_acquire(dentry->ref.lock);
	if(S_ISDIR(dentry->node->mode))
	{
		
	}

	if(dentry->parent && dentry->parent->node)
	{
		dentry->parent->node->nlinks--;
	}

	kfree(dentry->node);
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

	return;
}

const char* get_status_name(thread_status status)
{
	switch(status)
	{
	case thread_status::ready:
		return "I (idle)";
	case thread_status::running:
		return "R (running)";
	case thread_status::sleeping:
		return "S (sleeping)";
	case thread_status::terminated:
		return "Z (zombie)";
	default:
		return "?";
	}
}

ssize_t read_proc_status(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	thread_t* target = (thread_t*)file->inode->data;
	format_to(string_span{(char*)buffer, length}, "Name: {}\nState: {}\nPid: {}\nUid: {}\nGid: {}\n",
		target->name,
		get_status_name(target->status),
		target->pid,
		target->cred.uid,
		target->cred.gid
	);		
	return length;
}

static vfs::fs_file_ops proc_status_fops =
{
	.read = read_proc_status
};

void procfs_register_thread(thread_t* thr)
{
	char str_buf[64];
	format_to(string_span{&str_buf[0], 64}, "{}", thr->pid);

	procfs_mkdir(str_buf);
	
	format_to(string_span{&str_buf[0], 64}, "{}/status", thr->pid);

	procfs_create(str_buf, &proc_status_fops, (void*)thr);
}

void procfs_unregister_thread(thread_t* thr)
{
	char str_buf[64];
	format_to(string_span{&str_buf[0], 64}, "{}/status", thr->pid);
	procfs_remove(str_buf);
	format_to(string_span{&str_buf[0], 64}, "{}", thr->pid);
	procfs_remove(str_buf);
}
