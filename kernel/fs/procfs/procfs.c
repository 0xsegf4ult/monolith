#include <fs/procfs/procfs.h>
#include <fs/generic.h>
#include <fs/lookup.h>
#include <fs/stat.h>
#include <fs/super.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <sched/task.h>
#include <libk/string.h>
#include <libk/vsprintf.h>

#include <types.h>
#include <errno.h>

static struct inode_ops procfs_iops =
{
	.lookup = generic_fs_lookup,
	.getdents = generic_fs_getdents
};

static struct file_ops procfs_fops =
{
	.read = nullptr,
	.write = nullptr
};

static struct superblock* g_procfs = nullptr;

int procfs_super_init(struct block_device* bdev, struct superblock** out_sb)
{
	*out_sb = g_procfs;
	return 0;
}

int procfs_super_root(struct superblock* sb, struct ventry** out_root)
{
	*out_root = (struct ventry*)sb->data;
	return 0;
}

static struct super_ops procfs_sb_ops = 
{
	.init = procfs_super_init,
	.root = procfs_super_root
};

void procfs_init()
{
	struct filesystem* fs = kmalloc(sizeof(struct filesystem));
	fs->flags = FS_FLAG_NODEV;
	fs->iops = &procfs_iops;
	fs->fops = &procfs_fops;
	fs->sb_ops = &procfs_sb_ops;

	filesystem_register(fs, "procfs");
	
	struct superblock* sb = kmalloc(sizeof(struct superblock));
	struct vnode* node = vnode_new(S_IFDIR | 0755);
	node->iops = &procfs_iops;
        node->fops = &procfs_fops;

        struct ventry* dentry = ventry_new("proc", node);
	sb->data = (void*)dentry;

	sb->bdev = nullptr;
	g_procfs = sb;
}

void procfs_mkdir(const char* path)
{
	if(!g_procfs)
		return;

	struct ventry* parent = nullptr;
	int status = vfs_lookup_at((struct ventry*)g_procfs->data, path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return;

	size_t plen = strlen(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return;

	struct ventry* result = nullptr;
	status = vfs_lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return;

	generic_fs_mkdir(parent, basename, 0755);
}

void procfs_create(const char* path, struct file_ops* fops, void* priv_data)
{
	if(!g_procfs)
		return;

	struct ventry* parent = nullptr;
	int status = vfs_lookup_at((struct ventry*)g_procfs->data, path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return;

	size_t plen = strlen(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return;

	struct ventry* result = nullptr;
	status = vfs_lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return;

	struct vnode* inode = vnode_new(S_IFREG | S_IRUSR | S_IRGRP | S_IROTH);
        inode->iops = parent->node->iops;
	inode->fops = fops;
	inode->data = priv_data;

        struct ventry* dirent = ventry_new(basename, inode);
        dirent->parent = parent;

        mutex_lock(&parent->node->lock);
	list_add_tail(&parent->children, &dirent->sibling);
        mutex_unlock(&parent->node->lock);
}

void procfs_remove(const char* path)
{
	if(!g_procfs)
		return;

	struct ventry* dentry = nullptr;
	int status = vfs_lookup_at((struct ventry*)g_procfs->data, path, &dentry, 0);
	if(status < 0)
		return;

	spinlock_acquire(&dentry->ref.lock);
	if(S_ISDIR(dentry->node->mode))
	{
		
	}

	if(dentry->parent && dentry->parent->node)
	{
		dentry->parent->node->nlink--;
	}

	kfree(dentry->node);
	dentry->node = nullptr;

	list_del(&dentry->sibling);
	spinlock_release(&dentry->ref.lock);
	ventry_put(dentry);

	return;
}

ssize_t read_proc_status(struct file_descriptor* file, byte* buffer, size_t length)
{
	struct task* target = (struct task*)file->inode->data;
	struct vm_space* targetvm = target->current_vm_space;

	//FIXME: respect length
	sprintf(buffer, "Name: %s\nState: %s\nPid: %d\nPgid: %d\nSid: %d\nUid: %u\nGid: %u\nVirtAnon: %u kB\nRssAnon: %u kB\nVirtFile: %u kB\nRssFile: %u kB\n",
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

static struct file_ops proc_status_fops =
{
	.read = read_proc_status
};

ssize_t read_proc_maps(struct file_descriptor* file, byte* buffer, size_t length)
{
	struct task* target = (struct task*)file->inode->data;
	struct vm_space* targetvm = target->current_vm_space;

	size_t remaining = length;
	char* out_buffer = buffer;

	mutex_lock(&targetvm->lock);
	struct vm_object* range;
	list_for_each_entry(range, &targetvm->objects, list_node)
	{
		if(out_buffer >= (char*)buffer + length)
			break;

		ssize_t written = sprintf(out_buffer, "%016p - %016p %c%c%c %08x %s\n",
			range->base,
			range->base + range->length,
			(range->prot & PROT_READ) ? 'r' : '-',
			(range->prot & PROT_WRITE) ? 'w' : '-',
			(range->prot & PROT_EXEC) ? 'x' : '-',
			range->offset,
			range->file ? range->file->path->name : "[anon]"
		);
		out_buffer += (written - 1);
	}

	mutex_unlock(&targetvm->lock);

	return length;
}

static struct file_ops proc_maps_fops =
{
	.read = read_proc_maps
};

void procfs_register_process(struct task* task)
{
	char str_buf[64];
	
	sprintf(str_buf, "%d", task->tgid);
	procfs_mkdir(str_buf);
	
	sprintf(str_buf, "%d/status", task->tgid);
	procfs_create(str_buf, &proc_status_fops, (void*)task);

	sprintf(str_buf, "%d/maps", task->tgid);
	procfs_create(str_buf, &proc_maps_fops, (void*)task);
}

void procfs_unregister_process(struct task* task)
{
	char str_buf[64];
	sprintf(str_buf, "%d/maps", task->tgid);
	procfs_remove(str_buf);
	sprintf(str_buf, "%d/status", task->tgid);
	procfs_remove(str_buf);
	sprintf(str_buf, "%d", task->tgid);
	procfs_remove(str_buf);
}
