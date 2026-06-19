#include <fs/procfs/procfs.hpp>
#include <fs/generic.hpp>
#include <fs/vfs.hpp>
#include <mm/slab.hpp>

#include <lib/types.hpp>

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

	kfree(dentry->node);
	dentry->node = nullptr;

	if(dentry->parent && dentry->parent->children == dentry)
	{
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
