#include <fs/ramfs/ramfs.h>
#include <fs/generic.h>
#include <fs/stat.h>
#include <fs/super.h>
#include <fs/vfs.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmm.h>

#include <libk/list.h>
#include <libk/string.h>

#include <sys/mutex.h>

#include <errno.h>
#include <klog.h>
#include <types.h>

// FIXME: should use VFS page cache once that exists

typedef struct
{
	byte* data;
	list_node_t list_node;
} ramfs_page;

typedef struct
{
	list_head_t pages;
} ramfs_data;

ssize_t ramfs_read(struct file_descriptor* file, byte* buffer, size_t length)
{
	mutex_lock(&file->inode->lock);
	ramfs_data* data = (ramfs_data*)file->inode->data;
	if(!data)
	{
		mutex_unlock(&file->inode->lock);
		return 0;
	}
	
	ramfs_page* spage = list_first_entry(&data->pages, ramfs_page, list_node);
	if(file->pos)
	{
		off_t rp = file->pos;
		while(rp >= 4096)
		{
			if(!spage || &spage->list_node == &data->pages)
				return -ENXIO;

			spage = list_next_entry(spage, list_node);
			rp -= 4096;
		}
	}

	size_t orig_l = length;
	while(length && spage)
	{
		off_t page_offset = file->pos % 4096;
		off_t amount = 4096 - page_offset;
		if(length < amount)
			amount = length;

		memcpy(buffer, spage->data + page_offset, amount);
		buffer += amount;
		file->pos += amount;
		length -= amount;
		spage = list_next_entry(spage, list_node);
	}
	mutex_unlock(&file->inode->lock);
	return orig_l - length;
}

ssize_t ramfs_write(struct file_descriptor* file, const byte* buffer, size_t length)
{
	mutex_lock(&file->inode->lock);
	off_t fsize = file->inode->size;
	off_t wr_len = file->pos + length;

	if(wr_len > fsize)
	{
		off_t req = wr_len - fsize;
		while(req)
		{
			physaddr_t dpage = pmm_allocate() + VM_DMAP_BASE;	
			ramfs_page* ipage = kmalloc(sizeof(ramfs_page));
			ipage->data = (byte*)dpage;
			list_node_init(&ipage->list_node);

			if(!file->inode->data)
			{
				file->inode->data = kmalloc(sizeof(ramfs_data));
				list_node_init(&((ramfs_data*)file->inode->data)->pages);
			}

			ramfs_data* idata = (ramfs_data*)file->inode->data;
			list_add_tail(&idata->pages, &ipage->list_node);

			file->inode->data = (void*)idata;
			file->inode->size += 4096;	

			if(req < 4096)
				break;

			req -= 4096;
		}
	}

	ramfs_data* data = (ramfs_data*)file->inode->data;
	ramfs_page* spage = list_first_entry(&data->pages, ramfs_page, list_node);
	if(file->pos)
	{
		off_t wp = file->pos;
		while(wp >= 4096)
		{
			spage = list_next_entry(spage, list_node);
			wp -= 4096;
		}
	}

	size_t orig_l = length;
	while(length && spage)
	{
		off_t page_offset = file->pos % 4096;
		off_t amount = 4096 - page_offset;
		if(length < amount)
			amount = length;

		memcpy(spage->data + page_offset, buffer, amount); 
		if(page_offset + amount < 4096)
			memset(spage->data + page_offset + amount, 0, 4096 - (page_offset + amount));

		file->pos += amount;
		buffer += amount;
		length -= amount;
		spage = list_next_entry(spage, list_node);
	}

	mutex_unlock(&file->inode->lock);
	return orig_l - length;
}

static bool ramfs_vm_fault(struct vm_object* object, virtaddr_t addr, uint32_t flags)
{
	struct vm_space* space = object->space;
	ramfs_data* data = (ramfs_data*)(object->file->inode->data);
	ramfs_page* spage = list_first_entry(&data->pages, ramfs_page, list_node);
	off_t offset = object->offset + (addr - object->base);

	while(offset && spage && &spage->list_node != &data->pages)
	{
		offset -= 0x1000;
		spage = list_next_entry(spage, list_node);
	}

	if(offset)
		return false;	
	
	if(object->prot & PROT_WRITE && (flags & VM_FAULT_WRITE))
	{
		physaddr_t new_page = pmm_allocate();
		mmu_map(space->mmu_root, new_page, addr, object->prot, VM_FLAG_OWNER);
		mmu_invalidate(space->mmu_root, addr, 0x1000);
		memcpy((void*)addr, spage->data, 0x1000);

		space->resident_file++;

		return true;
	}
	else if(object->prot & PROT_READ)
	{
		uint32_t prot = object->prot & (~PROT_WRITE);
		mmu_map(space->mmu_root, (physaddr_t)spage->data - VM_DMAP_BASE, addr, prot, 0);
		return true;
	}
	else
	{
		klog("unhandled VM fault on %p mapped to %s + %p\n", addr, object->file->path->name, object->offset + (addr - object->base));
	}

	return false;
}

static struct vm_object_ops ramfs_vm_ops =
{
	.fault = ramfs_vm_fault
};

static int ramfs_mmap(struct file_descriptor* file, struct vm_object* range)
{	
	if(range->prot & PROT_WRITE)
		range->flags |= VM_FLAG_COW;

	range->vm_ops = &ramfs_vm_ops;
	return 0;
}

static struct inode_ops ramfs_iops =
{
	.lookup = generic_fs_lookup,
	.create = generic_fs_create,
	.mkdir = generic_fs_mkdir,
	.mknod = generic_fs_mknod,
	.getdents = generic_fs_getdents
};

static struct file_ops ramfs_fops =
{
	.read = ramfs_read,
	.write = ramfs_write,
	.mmap = ramfs_mmap,
};

int ramfs_super_init(struct block_device* bdev, struct superblock** out_sb)
{
        struct superblock* sb = kmalloc(sizeof(struct superblock));

        struct vnode* node = vnode_new(S_IFDIR | 0755);
        node->iops = &ramfs_iops;
        node->fops = &ramfs_fops;
	
        struct ventry* dentry = ventry_new("ramfs", node);
        sb->data = (void*)dentry;

        sb->bdev = nullptr;
	*out_sb = sb;
        return 0;
}

int ramfs_super_root(struct superblock* sb, struct ventry** out_root)
{
	*out_root = (struct ventry*)sb->data;
	return 0;
}

static struct super_ops ramfs_sb_ops = 
{
	.init = ramfs_super_init,
	.root = ramfs_super_root
};

void ramfs_init()
{
	struct filesystem* fs = kmalloc(sizeof(struct filesystem));
	fs->flags = FS_FLAG_NODEV;
	fs->iops = &ramfs_iops;
	fs->fops = &ramfs_fops;
	fs->sb_ops = &ramfs_sb_ops;

	filesystem_register(fs, "ramfs");
}

