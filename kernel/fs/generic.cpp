#include <fs/generic.hpp>
#include <fs/ventry.hpp>
#include <fs/vfs.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>
#include <dev/block/block.hpp>

#include <mm/slab.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <sys/err.hpp>
#include <sys/thread.hpp>
#include <sys/cred.hpp>

using namespace vfs;

ventry_t* generic_fs_lookup(ventry_t* parent, const char* path)
{
	auto* current = parent->children;
        while(current != nullptr)
        {
		reflock_acquire(current->ref);

                if(!strncmp(current->name, path, 64))
                {
                        auto released = reflock_release_or_lock(current->ref);
			if(released)
				spinlock_release(current->ref.lock);

			return current;
                }

                auto* sibling = current->sibling_next;
                auto released = reflock_release_or_lock(current->ref);
		if(released)
			spinlock_release(current->ref.lock);
		current = sibling;
        }

        return nullptr;
}

int generic_fs_create(ventry_t* parent, const char* path, mode_t mode)
{
	auto* inode = vnode_new(S_IFREG | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;
	
	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr)
	{
		inode->uid = thr->cred.euid;
		inode->gid = thr->cred.egid;
	}

        auto* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);

        if(parent->children)
	{
		parent->children->sibling_prev = dirent;
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;

        mutex_unlock(parent->node->lock);

	dcache_insert(dirent);

        return 0;
}

int generic_fs_mkdir(ventry_t* parent, const char* path, mode_t mode)
{ 
	auto* inode = vnode_new(S_IFDIR | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;
	inode->nlinks++;
	
	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr)
	{
		inode->uid = thr->cred.euid;
		inode->gid = thr->cred.egid;
	}

        auto* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);

	parent->node->nlinks++;

        if(parent->children)
	{
		parent->children->sibling_prev = dirent;
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;

        mutex_unlock(parent->node->lock);

	dcache_insert(dirent);

        return 0;
}

int generic_fs_mknod(ventry_t* parent, const char* path, mode_t mode, dev_t dev)
{
 	vnode_t* inode = vnode_new(mode);
	inode->iops = parent->node->iops;
        if(S_ISCHR(mode))
                inode->fops = chardev_get(dev)->fops;
        else if(S_ISBLK(mode))
                inode->fops = blockdev_get(dev)->fops;
        else
	{
		kfree(inode);
                return -EINVAL;
	}

        inode->dev = dev;
	
	auto* thr = smp_current_cpu()->get_current_thread();
	if(thr)
	{
		inode->uid = thr->cred.euid;
		inode->gid = thr->cred.egid;
	}

        auto* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);

        if(parent->children)
	{
		parent->children->sibling_prev = dirent;
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;
        mutex_unlock(parent->node->lock);

	dcache_insert(dirent);

        return 0;
}

int generic_fs_open(vfs::vnode_t* node, int flags)
{
	return 0;
}

int generic_fs_close(int fd)
{
	return 0;
}

ssize_t generic_fs_getdents(file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* dentry = file->path->children;

        byte* write_head = buffer;

	if(write_head < buffer + length)
	{
                dirent_info* dirent = reinterpret_cast<dirent_info*>(write_head);
		dirent->length = sizeof(dirent_info) + 2;
		dirent->type = 0;

		write_head += sizeof(dirent_info);
		memcpy(write_head, ".", 2);
		write_head += 2;
	}

	if(write_head < buffer + length)
	{
		dirent_info* dirent = reinterpret_cast<dirent_info*>(write_head);
		dirent->length = sizeof(dirent_info) + 3;
		dirent->type = 0;

		write_head += sizeof(dirent_info);
		memcpy(write_head, "..", 3);
		write_head += 3;
	}

        while(dentry && write_head < buffer + length)
        {
                reflock_acquire(dentry->ref);
		dirent_info* dirent = reinterpret_cast<dirent_info*>(write_head);

                auto name_len = string_length(dentry->name) + 1;
                dirent->length = sizeof(dirent_info) + name_len;
                dirent->type = 0;

                write_head += sizeof(dirent_info);
                memcpy(write_head, dentry->name, name_len);
                write_head += name_len;

                auto sibling = dentry->sibling_next;
                auto released = reflock_release_or_lock(dentry->ref);
		if(released)
			spinlock_release(dentry->ref.lock);
		dentry = sibling;
        }

        return static_cast<ssize_t>(write_head - buffer);
}	
