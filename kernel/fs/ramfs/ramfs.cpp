#include <fs/ramfs/ramfs.hpp>
#include <fs/generic.hpp>
#include <fs/vfs.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <lib/klog.hpp>

#include <sys/err.hpp>

using namespace vfs;

// FIXME: should use VFS page cache once that exists

struct ramfs_page
{
	byte* data;
	ramfs_page* next;
};

struct ramfs_data
{
	ramfs_page* head;
	ramfs_page* tail;
};

ssize_t ramfs_read(file_descriptor_t* file, byte* buffer, size_t length)
{
	mutex_lock(file->inode->lock);
	ramfs_page* spage = reinterpret_cast<ramfs_data*>(file->inode->data)->head;
	if(file->read_pos)
	{
		auto rp = file->read_pos;
		while(rp >= 4096)
		{
			if(!spage)
				return 0;
			spage = spage->next;
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
		spage = spage->next;
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
			auto dpage = pmm_allocate() + mm::direct_mapping_offset;		
			auto* ipage = (ramfs_page*)kmalloc(sizeof(ramfs_page));
			ipage->data = reinterpret_cast<byte*>(dpage);
			ipage->next = nullptr;
		
			bool first_alloc = false;
			if(!file->inode->data)
			{
				file->inode->data = kmalloc(sizeof(ramfs_data));
				first_alloc = true;
			}

			auto* idata = reinterpret_cast<ramfs_data*>(file->inode->data);
			if(first_alloc)
			{
				idata->head = ipage;
				idata->tail = ipage;
			}
			else
			{
				idata->tail->next = ipage;
				idata->tail = ipage;
			}

			file->inode->data = reinterpret_cast<void*>(idata);
			file->inode->size += 4096;	

			if(req < 4096)
				break;

			req -= 4096;
		}
	}

	ramfs_page* spage = reinterpret_cast<ramfs_data*>(file->inode->data)->head;
	if(file->write_pos)
	{
		auto wp = file->write_pos;
		while(wp >= 4096)
		{
			spage = spage->next;
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
		spage = spage->next;
	}

	mutex_unlock(file->inode->lock);
	return orig_l - length;
}

vfilesystem_t* ramfs_create()
{
	auto* fs = (vfilesystem_t*)kmalloc(sizeof(vfilesystem_t));
	fs->iops.lookup = generic_fs_lookup;
	fs->iops.create = generic_fs_create;
	fs->iops.mkdir = generic_fs_mkdir;
	fs->iops.mknod = generic_fs_mknod;
	fs->iops.getdents = generic_fs_getdents;
	fs->fops.open = generic_fs_open;
	fs->fops.close = generic_fs_close;
	fs->fops.read = ramfs_read;
	fs->fops.write = ramfs_write;
	fs->data = nullptr;

	return fs;
}

