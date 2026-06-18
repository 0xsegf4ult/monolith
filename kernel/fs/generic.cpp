#include <fs/generic.hpp>
#include <fs/vfs.hpp>
#include <dev/device.hpp>
#include <dev/character.hpp>
#include <dev/block.hpp>

#include <mm/slab.hpp>

#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <sys/err.hpp>

using namespace vfs;

ventry_t* generic_fs_lookup(ventry_t* parent, const char* path)
{
	auto* current = parent->children;
        while(current != nullptr)
        {
                mutex_lock(current->lock);

                if(!strncmp(current->name, path, 64))
                {
                        mutex_unlock(current->lock);
                        return current;
                }

                auto* sibling = current->sibling_next;
                mutex_unlock(current->lock);
                current = sibling;
        }

        return nullptr;
}

int generic_fs_create(ventry_t* parent, const char* path, mode_t mode)
{
	auto* inode = create_node(S_IFREG | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;

        auto* dirent = create_dentry(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->lock);

        if(parent->children)
	{
		mutex_lock(parent->children->lock);
		parent->children->sibling_prev = dirent;
		mutex_unlock(parent->children->lock);
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;

        mutex_unlock(parent->lock);

        return 0;
}

int generic_fs_mkdir(ventry_t* parent, const char* path, mode_t mode)
{ 
	auto* inode = create_node(S_IFDIR | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;
	inode->nlinks++;

        auto* dirent = create_dentry(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->lock);

	parent->node->nlinks++;

        if(parent->children)
	{
		mutex_lock(parent->children->lock);
		parent->children->sibling_prev = dirent;
		mutex_unlock(parent->children->lock);
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;

        mutex_unlock(parent->lock);

        return 0;
}

int generic_fs_mknod(ventry_t* parent, const char* path, mode_t mode, dev_t dev)
{
 	vnode_t* inode = create_node(mode);
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

        auto* dirent = create_dentry(path, inode);
        dirent->parent = parent;

        mutex_lock(parent->lock);

        if(parent->children)
	{
		mutex_lock(parent->children->lock);
		parent->children->sibling_prev = dirent;
		mutex_unlock(parent->children->lock);
                dirent->sibling_next = parent->children;
	}

        parent->children = dirent;
        mutex_unlock(parent->lock);

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
                mutex_lock(dentry->lock);
                dirent_info* dirent = reinterpret_cast<dirent_info*>(write_head);

                auto name_len = string_length(dentry->name) + 1;
                dirent->length = sizeof(dirent_info) + name_len;
                dirent->type = 0;

                write_head += sizeof(dirent_info);
                memcpy(write_head, dentry->name, name_len);
                write_head += name_len;

                auto sibling = dentry->sibling_next;
                mutex_unlock(dentry->lock);
                dentry = sibling;
        }

        return static_cast<ssize_t>(write_head - buffer);
}	
