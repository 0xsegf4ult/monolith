#include <fs/generic.hpp>
#include <fs/ventry.hpp>
#include <fs/vfs.hpp>

#include <dev/device.hpp>
#include <dev/character.hpp>
#include <dev/block/block.hpp>

#include <mm/slab.hpp>

#include <sys/err.hpp>
#include <sys/cred.hpp>
#include <sys/smp.hpp>
#include <sys/task.hpp>

#include <kstd.hpp>
#include <types.hpp>

using namespace vfs;

ventry_t* generic_fs_lookup(ventry_t* parent, const char* path)
{
	ventry_t* current;
	ventry_t* tmp;
	list_for_each_entry_safe(current, tmp, parent->children, sibling)
	{
		reflock_acquire(current->ref);

                if(!strncmp(current->name, path, 64))
                {
                        auto released = reflock_release_or_lock(current->ref);
			if(released)
				spinlock_release(current->ref.lock);

			return current;
                }

                auto released = reflock_release_or_lock(current->ref);
		if(released)
			spinlock_release(current->ref.lock);
        }

        return nullptr;
}

int generic_fs_create(ventry_t* parent, const char* path, mode_t mode)
{
	auto* inode = vnode_new(S_IFREG | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;
	
	auto* task = smp_current_task();
	if(task)
	{
		inode->uid = task->cred.euid;
		inode->gid = task->cred.egid;
	}

        auto* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);

	list_add_tail(parent->children, dirent->sibling);

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
	
	auto* task = smp_current_task();
	if(task)
	{
		inode->uid = task->cred.euid;
		inode->gid = task->cred.egid;
	}

        auto* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);

	parent->node->nlinks++;

	list_add_tail(parent->children, dirent->sibling);

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
	
	auto* task = smp_current_task();
	if(task)
	{
		inode->uid = task->cred.euid;
		inode->gid = task->cred.egid;
	}

        auto* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->node->lock);
	list_add_tail(parent->children, dirent->sibling);
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

	ventry_t* current;
	ventry_t* tmp;
	list_for_each_entry_safe(current, tmp, file->path->children, sibling)
        {
		if(write_head >= buffer + length)
			break;
        
		reflock_acquire(current->ref);
		dirent_info* dirent = reinterpret_cast<dirent_info*>(write_head);

                auto name_len = string_length(current->name) + 1;
                dirent->length = sizeof(dirent_info) + name_len;
                dirent->type = 0;

                write_head += sizeof(dirent_info);
                memcpy(write_head, current->name, name_len);
                write_head += name_len;

                auto released = reflock_release_or_lock(current->ref);
		if(released)
			spinlock_release(current->ref.lock);
        }

        return static_cast<ssize_t>(write_head - buffer);
}	
