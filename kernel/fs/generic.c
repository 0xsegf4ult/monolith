#include <fs/generic.h>
#include <fs/stat.h>
#include <fs/ventry.h>
#include <fs/vfs.h>

#include <mm/slab.h>

#include <sched/task.h>
#include <sys/cred.h>
#include <sys/device.h>
#include <sys/smp.h>

#include <libk/list.h>
#include <libk/string.h>

#include <errno.h>
#include <types.h>

#include <klog.h>

struct ventry* generic_fs_lookup(struct ventry* parent, const char* path)
{
	struct ventry* current;
	struct ventry* tmp;
	list_for_each_entry_safe(current, tmp, &parent->children, sibling)
	{
		ventry_ref(current);

                if(strncmp(current->name, path, 64) == 0)
                {
			ventry_put(current);
			return current;
                }

		ventry_put(current);
        }

        return nullptr;
}

int generic_fs_create(struct ventry* parent, const char* path, mode_t mode)
{
	struct vnode* inode = vnode_new(S_IFREG | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;
	
	struct task* task = smp_current_task();
	if(task)
	{
		inode->uid = task->cred.euid;
		inode->gid = task->cred.egid;
	}

        struct ventry* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(&parent->node->lock);
	list_add_tail(&parent->children, &dirent->sibling);
        mutex_unlock(&parent->node->lock);

	dcache_insert(dirent);

        return 0;
}

int generic_fs_mkdir(struct ventry* parent, const char* path, mode_t mode)
{ 
	struct vnode* inode = vnode_new(S_IFDIR | mode);
        inode->iops = parent->node->iops;
	inode->fops = parent->node->fops;
	inode->nlink++;
	
	struct task* task = smp_current_task();
	if(task)
	{
		inode->uid = task->cred.euid;
		inode->gid = task->cred.egid;
	}

        struct ventry* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(&parent->node->lock);

	parent->node->nlink++;
	list_add_tail(&parent->children, &dirent->sibling);

        mutex_unlock(&parent->node->lock);

	dcache_insert(dirent);

        return 0;
}

int generic_fs_mknod(struct ventry* parent, const char* path, mode_t mode, dev_t dev)
{
 	struct vnode* inode = vnode_new(mode);
	inode->iops = parent->node->iops;
       	if(S_ISCHR(mode))
                inode->fops = chardev_lookup(dev)->fops;
        else if(S_ISBLK(mode))
                inode->fops = blockdev_lookup(dev)->fops;
        else
	{
		kfree(inode);
                return -EINVAL;
	}

        inode->dev = dev;
	
	struct task* task = smp_current_task();
	if(task)
	{
		inode->uid = task->cred.euid;
		inode->gid = task->cred.egid;
	}

        struct ventry* dirent = ventry_new(path, inode);
        dirent->parent = parent;

        mutex_lock(&parent->node->lock);
	list_add_tail(&parent->children, &dirent->sibling);
        mutex_unlock(&parent->node->lock);

	dcache_insert(dirent);

        return 0;
}

ssize_t generic_fs_getdents(struct file_descriptor* file, byte* buffer, size_t length)
{
        byte* write_head = buffer;

	if(write_head < buffer + length)
	{
                struct dirent_info* dirent = (struct dirent_info*)write_head;
		dirent->length = sizeof(struct dirent_info) + 2;
		dirent->type = 0;

		write_head += sizeof(struct dirent_info);
		memcpy(write_head, ".", 2);
		write_head += 2;
	}

	if(write_head < buffer + length)
	{
		struct dirent_info* dirent = (struct dirent_info*)write_head;
		dirent->length = sizeof(struct dirent_info) + 3;
		dirent->type = 0;

		write_head += sizeof(struct dirent_info);
		memcpy(write_head, "..", 3);
		write_head += 3;
	}

	struct ventry* current;
	struct ventry* tmp;
	list_for_each_entry_safe(current, tmp, &file->path->children, sibling)
        {
		if(write_head >= buffer + length)
			break;
        
		ventry_ref(current);
		reflock_acquire(&current->ref);
		struct dirent_info* dirent = (struct dirent_info*)write_head;

                size_t name_len = strlen(current->name) + 1;
                dirent->length = sizeof(struct dirent_info) + name_len;
                dirent->type = 0;

                write_head += sizeof(struct dirent_info);
                memcpy(write_head, current->name, name_len);
                write_head += name_len;

        	ventry_put(current);
	}

        return (ssize_t)(write_head - buffer);
}	
