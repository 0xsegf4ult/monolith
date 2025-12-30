#include <fs/vfs.hpp>
#include <fs/ramfs.hpp>

#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

namespace vfs
{

static context_t* context = nullptr;

void init()
{
	context = (context_t*)(pmm_allocate() + mm::direct_mapping_offset);

	auto* ramfs = ramfs_create();

	auto* rnode = (vnode_t*)kmalloc(sizeof(vnode_t));
	rnode->type = vnode_type::directory;
	rnode->size = 0;
	rnode->data = nullptr;
	rnode->ops = &ramfs->ops;

	context->root_node = (ventry_t*)kmalloc(sizeof(ventry_t));
	context->root_node->name[0] = '/';
	context->root_node->name[1] = '\0';
	context->root_node->node = rnode;
	context->root_node->parent = nullptr;
	context->root_node->children = nullptr;

	memset(context->open_files, 0, 64 * sizeof(file_descriptor_t));
}

int create(const char* path)
{
	auto query = lookup(context->root_node, path);

	if(query.result)
		return -1;

	if(!query.parent)
		return -1;

	return query.parent->node->ops->create(query.parent, query.basename);	
}

int mkdir(const char* path)
{
	auto query = lookup(context->root_node, path);

	if(query.result)
		return -1;

	if(!query.parent)
		return -1;

	return query.parent->node->ops->mkdir(query.parent, query.basename);
}

int mknod(const char* path, char type, dev_t device)
{
	log::debug("mknod {}", path);
	auto query = lookup(context->root_node, path);

	if(query.result)
		return -1;

	if(!query.parent)
		return -1;

	return query.parent->node->ops->mknod(query.parent, query.basename, type, device);
}

lookup_result lookup(ventry_t* parent, const char* path)
{
	auto len = string_length(path);

	char* cbuffer = (char*)kmalloc(len + 1);
	strncpy(cbuffer, path, len + 1);
	
	for(int i = 0; i < len; i++)
	{
		if(cbuffer[i] == '/')
			cbuffer[i] = '\0';
	}

	ventry_t* current = parent;
	ventry_t* c_parent = parent->parent;
	const char* basename = nullptr;

	for(int i = 0; i < len; i++)
	{
		if(cbuffer[i] == '\0')
			continue;

		if(!current)
			break;

		c_parent = current;
		basename = &path[i];

		char* component = &cbuffer[i];
		size_t clen = string_length(component);

		auto* next = current->node->ops->lookup(current, component);

		i += clen;
		current = next;
	}

	kfree(cbuffer);
	return {current, c_parent, basename};
}

int open(const char* path, int flags)
{
	auto query = lookup(context->root_node, path);
	if(!query.result)
	{
		return -1;
	}

	auto fs_id = query.result->node->ops->open(query.result->node, flags);
	if(fs_id < 0)
		return -1;

	int fd = -1;
	for(int i = 0; i < 64; i++)
	{
		if(context->open_files[i].inode == nullptr)
		{
			fd = i;
			context->open_files[i].read_pos = 0;
			context->open_files[i].write_pos = 0;
			context->open_files[i].inode = query.result->node;
			context->open_files[i].fs_id = fs_id;
			break;
		}
	}
	
	return fd;
}

size_t read(int fd, byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	return inode->ops->read(&context->open_files[fd], buffer, length);
}

size_t write(int fd, const byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	return inode->ops->write(&context->open_files[fd], buffer, length);
}

int close(int fd)
{
	if(context->open_files[fd].inode == nullptr)
		return -1;

	auto cres = context->open_files[fd].inode->ops->close(context->open_files[fd].fs_id);
	if(cres < 0)
		return -1;

	context->open_files[fd].read_pos = 0;
	context->open_files[fd].write_pos = 0;
	context->open_files[fd].inode = nullptr;
	context->open_files[fd].fs_id = -1;
	return 0;
}

size_t seek(int fd, size_t offset, int flags)
{
	if(context->open_files[fd].inode == nullptr)
		return 0;

	context->open_files[fd].read_pos = offset;
	context->open_files[fd].write_pos = offset;

	return offset;
}

}
