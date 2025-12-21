#include <fs/ramfs.hpp>
#include <fs/vfs.hpp>
#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <lib/klog.hpp>

using namespace vfs;

ventry_t* ramfs_lookup(ventry_t* parent, const char* path)
{
	auto* current = parent->children;
	while(current != nullptr)
	{
		if(!strncmp(current->name, path, 64))
			return current;

		current = current->sibling; 
	}

	return nullptr;
}

int ramfs_create(ventry_t* parent, const char* path)
{
	auto* inode = (vnode_t*)kmalloc(sizeof(vnode_t));
	inode->type = vnode_type::file;
	inode->size = 0;
	inode->data = nullptr;
	inode->fs = parent->node->fs;

	auto* dirent = (ventry_t*)kmalloc(sizeof(ventry_t));
	strncpy(dirent->name, path, 64);
	auto namel = string_length(dirent->name);
	if(dirent->name[namel - 1] == '/')
		dirent->name[namel - 1] = '\0';

	dirent->node = inode;
	dirent->parent = parent;
	dirent->children = nullptr;
	dirent->sibling = nullptr;

	if(parent->children)
		dirent->sibling = parent->children;

	parent->children = dirent;
	return 0;
}

int ramfs_mkdir(ventry_t* parent, const char* path)
{
	auto* inode = (vnode_t*)kmalloc(sizeof(vnode_t));
	inode->type = vnode_type::directory;
	inode->size = 0;
	inode->data = nullptr;
	inode->fs = parent->node->fs;

	auto* dirent = (ventry_t*)kmalloc(sizeof(ventry_t));
	strncpy(dirent->name, path, 64);
	auto namel = string_length(dirent->name);
	if(dirent->name[namel - 1] == '/')
		dirent->name[namel - 1] = '\0';
	dirent->node = inode;
	dirent->parent = parent;
	dirent->children = nullptr;
	dirent->sibling = nullptr;

	if(parent->children)
		dirent->sibling = parent->children;

	parent->children = dirent;

	return 0;
}

int ramfs_open(vnode_t* node, int flags)
{
	return 0;
}

int ramfs_close(int fd)
{
	return 0;
}

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

size_t ramfs_read(file_descriptor_t* file, byte* buffer, size_t length)
{
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
		auto amount = 4096;
		if(length < 4096)
			amount = length;

		auto page_offset = file->read_pos % 4096;
		memcpy(buffer, spage->data + page_offset, amount);
		file->read_pos += amount;
		length -= amount;
		spage = spage->next;
	}
	return orig_l - length;
}

size_t ramfs_write(file_descriptor_t* file, const byte* buffer, size_t length)
{
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
		auto amount = 4096;
		if(length < 4096)
			amount = length;

		auto page_offset = file->write_pos % 4096;
		memcpy(spage->data, buffer + page_offset, amount); 
		file->write_pos += amount;
		length -= amount;
		spage = spage->next;
	}

	return orig_l - length;
}

vfilesystem_t* ramfs_create()
{
	auto* fs = (vfilesystem_t*)kmalloc(sizeof(vfilesystem_t));
	fs->lookup = ramfs_lookup;
	fs->create = ramfs_create;
	fs->mkdir = ramfs_mkdir;
	fs->open = ramfs_open;
	fs->close = ramfs_close;
	fs->read = ramfs_read;
	fs->write = ramfs_write;
	fs->data = nullptr;

	return fs;
}

