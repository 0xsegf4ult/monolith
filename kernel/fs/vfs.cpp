#include <fs/vfs.hpp>
#include <fs/ramfs.hpp>

#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <sys/err.hpp>

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
		return -EEXIST;

	if(!query.parent)
		return -ENOENT;

	return query.parent->node->ops->create(query.parent, query.basename);	
}

int mkdir(const char* path)
{
	auto query = lookup(context->root_node, path);

	if(query.result)
		return -EEXIST;

	if(!query.parent)
		return -ENOENT;

	return query.parent->node->ops->mkdir(query.parent, query.basename);
}

int mknod(const char* path, char type, dev_t device)
{
	auto query = lookup(context->root_node, path);

	if(query.result)
		return -EEXIST;

	if(!query.parent)
		return -ENOENT;

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
		return -ENOENT;
	}

	auto* node = query.result->node;

	int fs_id = 0;
	if(node->ops->open)
		fs_id = node->ops->open(node, flags);
	
	if(fs_id < 0)
		return fs_id;

	int fd = -1;
	for(int i = 0; i < 64; i++)
	{
		if(context->open_files[i].inode == nullptr)
		{
			fd = i;
			context->open_files[i].read_pos = 0;
			context->open_files[i].write_pos = 0;
			context->open_files[i].inode = node;
			context->open_files[i].fs_id = fs_id;
			break;
		}
	}
	
	return fd;
}

ssize_t read(int fd, byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	return inode->ops->read(&context->open_files[fd], buffer, length);
}

ssize_t write(int fd, const byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	return inode->ops->write(&context->open_files[fd], buffer, length);
}

int close(int fd)
{
	auto* node = context->open_files[fd].inode;
	if(!node)
		return -EBADF;

	int cres = 0;
	if(node->ops->close)
		cres = node->ops->close(context->open_files[fd].fs_id);
	
	if(cres < 0)
		return cres;

	context->open_files[fd].read_pos = 0;
	context->open_files[fd].write_pos = 0;
	context->open_files[fd].inode = nullptr;
	context->open_files[fd].fs_id = -1;
	return 0;
}

ssize_t seek(int fd, ssize_t offset, int flags)
{
	if(context->open_files[fd].inode == nullptr)
		return -EBADF;

	if(offset < 0)
		offset = 0;

	context->open_files[fd].read_pos = offset;
	context->open_files[fd].write_pos = offset;

	return offset;
}

int ioctl(int fd, uint64_t op, uint64_t arg)
{
	auto* inode = context->open_files[fd].inode;
	if(inode == nullptr)
		return -EBADF;

	if(!inode->ops->ioctl)
		return -ENOTTY;

	return inode->ops->ioctl(&context->open_files[fd], op, arg);
}

}
