#include <fs/ramfs/ramfs.hpp>
#include <fs/generic.hpp>
#include <fs/vfs.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <kstd.hpp>
#include <types.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/mmu.hpp>
#include <arch/x86_64/smp.hpp>

#include <klog.hpp>

#include <sys/err.hpp>
#include <sys/task.hpp>
#include <list.hpp>

using namespace vfs;

// FIXME: should use VFS page cache once that exists

struct ramfs_page
{
	byte* data;
	list_node_t list_node;
};

struct ramfs_data
{
	list_head_t pages;
};

ssize_t ramfs_read(file_descriptor_t* file, byte* buffer, size_t length)
{
	mutex_lock(file->inode->lock);
	ramfs_data* data = reinterpret_cast<ramfs_data*>(file->inode->data);
	if(!data)
	{
		mutex_unlock(file->inode->lock);
		return 0;
	}
	
	ramfs_page* spage = list_first_entry(&data->pages, ramfs_page, list_node);
	if(file->read_pos)
	{
		auto rp = file->read_pos;
		while(rp >= 4096)
		{
			if(!spage || &spage->list_node == &data->pages)
				return -ENXIO;

			spage = list_next_entry(spage, list_node);
			rp -= 4096;
		}
	}

	auto orig_l = length;
	while(length && spage)
	{
		auto page_offset = file->read_pos % 4096;
		auto amount = 4096 - page_offset;
		if(length < amount)
			amount = length;

		memcpy(buffer, spage->data + page_offset, amount);
		buffer += amount;
		file->read_pos += amount;
		length -= amount;
		spage = list_next_entry(spage, list_node);
	}
	mutex_unlock(file->inode->lock);
	return orig_l - length;
}

ssize_t ramfs_write(file_descriptor_t* file, const byte* buffer, size_t length)
{
	mutex_lock(file->inode->lock);
	auto fsize = file->inode->size;
	auto wr_len = file->write_pos + length;

	if(wr_len > fsize)
	{
		auto req = wr_len - fsize;
		while(req)
		{
			auto dpage = pmm_allocate() + VM_DMAP_BASE;	
			auto* ipage = (ramfs_page*)kmalloc(sizeof(ramfs_page));
			ipage->data = reinterpret_cast<byte*>(dpage);
			list_node_init(ipage->list_node);

			if(!file->inode->data)
			{
				file->inode->data = kmalloc(sizeof(ramfs_data));
				list_node_init(((ramfs_data*)file->inode->data)->pages);
			}

			auto* idata = reinterpret_cast<ramfs_data*>(file->inode->data);
			list_add_tail(idata->pages, ipage->list_node);

			file->inode->data = reinterpret_cast<void*>(idata);
			file->inode->size += 4096;	

			if(req < 4096)
				break;

			req -= 4096;
		}
	}

	ramfs_data* data = (ramfs_data*)file->inode->data;
	ramfs_page* spage = list_first_entry(&data->pages, ramfs_page, list_node);
	if(file->write_pos)
	{
		auto wp = file->write_pos;
		while(wp >= 4096)
		{
			spage = list_next_entry(spage, list_node);
			wp -= 4096;
		}
	}

	auto orig_l = length;
	while(length && spage)
	{
		auto page_offset = file->write_pos % 4096;
		auto amount = 4096 - page_offset;
		if(length < amount)
			amount = length;

		memcpy(spage->data + page_offset, buffer, amount); 
		file->write_pos += amount;
		buffer += amount;
		length -= amount;
		spage = list_next_entry(spage, list_node);
	}

	mutex_unlock(file->inode->lock);
	return orig_l - length;
}

static bool ramfs_vm_fault(vm_object* object, virtaddr_t addr, uint32_t flags)
{
	auto* space = object->space;
	auto* data = (ramfs_data*)(object->file->inode->data);
	ramfs_page* spage = list_first_entry(&data->pages, ramfs_page, list_node);
	off_t offset = object->offset;

	while(offset && spage && &spage->list_node != &data->pages)
	{
		offset -= 0x1000;
		spage = list_next_entry(spage, list_node);
	}

	if(offset)
		return false;	
	
	if(object->prot & PROT_WRITE && (flags & VM_FAULT_WRITE))
	{
		log::debug("write to COW mapped file, reallocating");
		auto new_page = pmm_allocate();
		mmu_map(space->mmu_root, new_page + VM_DMAP_BASE, addr, object->prot, VM_FLAG_OWNER);
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

	return false;
}

static vm_object_ops ramfs_vm_ops =
{
	.fault = ramfs_vm_fault
};

static int ramfs_mmap(file_descriptor_t* file, vm_object* range)
{	
	if(range->prot & PROT_WRITE)
		range->flags |= VM_FLAG_COW;

	range->vm_ops = &ramfs_vm_ops;
	return 0;
}

static fs_inode_ops ramfs_iops =
{
	.lookup = generic_fs_lookup,
	.create = generic_fs_create,
	.mkdir = generic_fs_mkdir,
	.mknod = generic_fs_mknod,
	.getdents = generic_fs_getdents
};

static fs_file_ops ramfs_fops =
{
	.open = generic_fs_open,
	.close = generic_fs_close,
	.read = ramfs_read,
	.write = ramfs_write,
	.mmap = ramfs_mmap,
};

int ramfs_super_init(block_device_t* bdev, superblock_t** out_sb)
{
        auto* sb = (superblock_t*)kmalloc(sizeof(superblock_t));

        auto* node = vnode_new(S_IFDIR | 0755);
        node->iops = &ramfs_iops;
        node->fops = &ramfs_fops;
	
        auto* dentry = ventry_new("ramfs", node);
        sb->data = (void*)dentry;

        sb->bdev = nullptr;
	*out_sb = sb;
        return 0;
}

int ramfs_super_root(superblock_t* sb, ventry_t** out_root)
{
	*out_root = (ventry_t*)sb->data;
	return 0;
}

static fs_super_ops ramfs_sb_ops
{
	.init = ramfs_super_init,
	.root = ramfs_super_root
};

void ramfs_init()
{
	auto* fs = (vfilesystem_t*)kmalloc(sizeof(vfilesystem_t));
	fs->flags = FS_FLAG_NODEV;
	fs->iops = &ramfs_iops;
	fs->fops = &ramfs_fops;
	fs->sb_ops = &ramfs_sb_ops;

	register_fs(fs, "ramfs");
}

