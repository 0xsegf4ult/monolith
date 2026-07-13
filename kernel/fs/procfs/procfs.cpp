#include <fs/procfs/procfs.hpp>
#include <fs/generic.hpp>
#include <fs/lookup.hpp>
#include <fs/vfs.hpp>
#include <fs/super.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>

#include <klog.hpp>
#include <kstd.hpp>
#include <types.hpp>
#include <sys/err.hpp>
#include <sys/task.hpp>

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
	list_add_tail(parent->children, dirent->sibling);
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

	list_del(dentry->sibling);
	spinlock_release(dentry->ref.lock);
	ventry_put(dentry);

	return;
}

ssize_t read_proc_status(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	task_t* target = (task_t*)file->inode->data;
	vm_space* targetvm = target->current_vm_space;
	format_to(string_span{(char*)buffer, length}, "Name: {}\nState: {}\nPid: {}\nPgid: {}\nSid: {}\nUid: {}\nGid: {}\nVirtAnon: {} kB\nRssAnon: {} kB\nVirtFile: {}kB\nRssFile: {} kB\n",
		target->name,
		get_status_name(target->status),
		target->pid,
		target->pgid,
		target->sid,
		target->cred.uid,
		target->cred.gid,
		targetvm->mapped_anon * 4,
		targetvm->resident_anon * 4,
		targetvm->mapped_file * 4,
		targetvm->resident_file * 4
	);		
	return length;
}

static vfs::fs_file_ops proc_status_fops =
{
	.read = read_proc_status
};

ssize_t read_proc_maps(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	task_t* target = (task_t*)file->inode->data;
	vm_space* targetvm = target->current_vm_space;

	size_t remaining = length;
	string_span out_buffer = {(char*)buffer, length};

	mutex_lock(targetvm->lock);
	vm_object* range;
	list_for_each_entry(range, targetvm->objects, list_node)
	{
		if(!out_buffer.size())
			break;

		out_buffer = format_to(out_buffer, "{:016x} - {:016x} {}{}{} {:08x} {}\n",
			range->base,
			range->base + range->length,
			(range->prot & PROT_READ) ? 'r' : '-',
			(range->prot & PROT_WRITE) ? 'w' : '-',
			(range->prot & PROT_EXEC) ? 'x' : '-',
			0,
			range->file ? range->file->path->name : "[anon]"
		);
	}

	mutex_unlock(targetvm->lock);

	return length;
}

static vfs::fs_file_ops proc_maps_fops =
{
	.read = read_proc_maps
};

void procfs_register_process(task_t* task)
{
	char str_buf[64];
	format_to(string_span{&str_buf[0], 64}, "{}", task->tgid);

	procfs_mkdir(str_buf);
	
	format_to(string_span{&str_buf[0], 64}, "{}/status", task->tgid);

	procfs_create(str_buf, &proc_status_fops, (void*)task);

	format_to(string_span{&str_buf[0], 64}, "{}/maps", task->tgid);

	procfs_create(str_buf, &proc_maps_fops, (void*)task);
}

void procfs_unregister_process(task_t* task)
{
	char str_buf[64];
	format_to(string_span{&str_buf[0], 64}, "{}/maps", task->tgid);
	procfs_remove(str_buf);
	format_to(string_span{&str_buf[0], 64}, "{}/status", task->tgid);
	procfs_remove(str_buf);
	format_to(string_span{&str_buf[0], 64}, "{}", task->tgid);
	procfs_remove(str_buf);
}
