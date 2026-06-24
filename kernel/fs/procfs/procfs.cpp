#include <fs/procfs/procfs.hpp>
#include <fs/generic.hpp>
#include <fs/vfs.hpp>
#include <mm/slab.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>
#include <sys/thread.hpp>

using namespace vfs;

static vfilesystem_t* g_procfs = nullptr;

vfilesystem_t* procfs_create()
{
	if(g_procfs)
		return nullptr;

	auto* fs = (vfilesystem_t*)kmalloc(sizeof(vfilesystem_t));
	fs->iops.lookup = generic_fs_lookup;
	fs->iops.create = nullptr;
	fs->iops.mkdir = nullptr;
	fs->iops.mknod = nullptr;
	fs->iops.getdents = generic_fs_getdents;
	fs->fops.open = generic_fs_open;
	fs->fops.close = generic_fs_close;
	fs->fops.read = nullptr;
	fs->fops.write = nullptr;
	fs->data = nullptr;
	g_procfs = fs;

	auto* node = vnode_new(S_IFDIR | 0755);
	node->iops = &fs->iops;
        node->fops = &fs->fops;

        auto* dentry = ventry_new("proc", node);
	fs->root = dentry;

	return fs;
}

void procfs_mkdir(const char* path)
{
	if(!g_procfs)
		return;

	auto parent = vfs::lookup_at(g_procfs->root, path, LOOKUP_PARENT);

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return;

	auto query = lookup_at(parent.result, basename, 0);
	if(query.result)
		return;

	generic_fs_mkdir(parent.result, basename, 0755);
}

void procfs_create(const char* path, vfs::fs_file_ops* fops, void* priv_data)
{
	if(!g_procfs)
		return;

	auto parent = vfs::lookup_at(g_procfs->root, path, LOOKUP_PARENT);

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return;

	auto query = lookup_at(parent.result, basename, 0);
	if(query.result)
		return;

	auto* inode = vnode_new(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH);
        inode->iops = parent.result->node->iops;
	inode->fops = fops;
	inode->data = priv_data;

        auto* dirent = ventry_new(basename, inode);
        dirent->parent = parent.result;

        mutex_lock(parent.result->node->lock);

        if(parent.result->children)
	{
		parent.result->children->sibling_prev = dirent;
                dirent->sibling_next = parent.result->children;
	}

        parent.result->children = dirent;

        mutex_unlock(parent.result->node->lock);
}

void procfs_remove(const char* path)
{
	if(!g_procfs)
		return;

	auto query = vfs::lookup_at(g_procfs->root, path, 0);
	if(!query.result)
		return;

	auto* dentry = query.result;
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
