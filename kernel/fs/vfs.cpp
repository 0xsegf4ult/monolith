#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

#include <fs/vfs.hpp>
#include <fs/ramfs.hpp>

#include <mm/layout.hpp>
#include <mm/pmm.hpp>
#include <mm/slab.hpp>

#include <lib/klog.hpp>
#include <lib/kstd.hpp>
#include <lib/types.hpp>

#include <sys/err.hpp>
#include <sys/process.hpp>

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

ventry_t* get_root_dentry()
{
	return context->root_node;
}

int create(const char* path)
{
	auto parent = lookup(path, LOOKUP_PARENT);
	if(!parent.result)
		return -ENOENT;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	auto query = lookup_at(parent.result, basename, 0);
	if(query.result)
		return -EEXIST;

	return parent.result->node->ops->create(parent.result, basename);	
}

int mkdir(const char* path)
{
	auto parent = lookup(path, LOOKUP_PARENT);
	if(!parent.result)
		return -ENOENT;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	auto query = lookup_at(parent.result, basename, 0);
	if(query.result)
		return -EEXIST;

	return parent.result->node->ops->mkdir(parent.result, basename);
}

int mknod(const char* path, char type, dev_t device)
{
	auto parent = lookup(path, LOOKUP_PARENT);
	if(!parent.result)
		return -ENOENT;

	auto plen = string_length(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	auto query = lookup_at(parent.result, basename, 0);
	if(query.result)
		return -EEXIST;

	return parent.result->node->ops->mknod(parent.result, basename, type, device);
}

lookup_result lookup_at(ventry_t* parent, const char* path, int flags)
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
	ventry_t* c_parent = nullptr;
	const char* basename = nullptr;

	for(int i = 0; i < len; i++)
	{
		if(cbuffer[i] == '\0')
			continue;

		if(!current)
			break;

		if(current->node->type != vnode_type::directory)
			break;

		basename = &path[i];

		char* component = &cbuffer[i];
		size_t clen = string_length(component);
		bool is_last = (i + clen) == len;
		if(!is_last)
		{
			int j;
			for(j = i + clen; j < len && component[j] == '\0'; ++j) {}
			is_last = (j == len);
		}

		if(is_last && (flags & LOOKUP_PARENT))
			break;

		ventry_t* next;
		
		if(clen == 1 && component[0] == '.')
			next = current;
		else if(clen == 2 && component[0] == '.' && component[1] == '.')
		{
			if(i + clen >= len)
				c_parent = current;
			next = current->parent;
		}
		else
		{
			next = current->node->ops->lookup(current, component);
		}

		i += clen;
		current = next;
	}

	kfree(cbuffer);
	return {current, basename};
}

lookup_result lookup(const char* path, int flags)
{
	if(path[0] == '/')
		return lookup_at(get_root_dentry(), path,  flags);
	else
		return lookup_at(smp_current_cpu()->get_current_process()->cwd, path, flags);
}

int open(const char* path, int flags)
{
	auto query = lookup(path, 0);
	if(!query.result)
	{
		if(flags & O_CREAT)
		{
			auto c_res = create(path);
			if(c_res < 0)
				return c_res;
		
			query = lookup(path, 0);
		}
		else
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
			context->open_files[i].path = query.result;
			context->open_files[i].fs_id = fs_id;
			break;
		}
	}
	
	return fd;
}

int openat(int fd, const char* path, int flags)
{
	if(flags)
		return -EINVAL;

	auto query = lookup_at(context->open_files[fd].path, path, 0);
	if(!query.result)
		return -ENOENT;

	auto* node = query.result->node;

	int fs_id = 0 ;
	if(node->ops->open)
		fs_id = node->ops->open(node, flags);

	if(fs_id < 0)
		return fs_id;

	int s_fd = -1;
	for(int i = 0; i < 64; i++)
	{
		if(context->open_files[i].inode == nullptr)
		{
			s_fd = i;
			context->open_files[i].read_pos = 0;
			context->open_files[i].write_pos = 0;
			context->open_files[i].inode = node;
			context->open_files[i].path = query.result;
			context->open_files[i].fs_id = fs_id;
			break;
		}
	}

	return s_fd;
}

ssize_t read(int fd, byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	if(inode->type == vnode_type::directory)
	       return -EISDIR;	

	return inode->ops->read(&context->open_files[fd], buffer, length);
}

ssize_t write(int fd, const byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	if(inode->type == vnode_type::directory)
	       return -EISDIR;	
	
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
	context->open_files[fd].path = nullptr;
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

int stat(const char* path, stat_t* output)
{
	auto query = lookup(path, 0);
	if(!query.result)
	{
		return -ENOENT;
	}

	auto* node = query.result->node;
	
	output->type = node->type;
	output->size = node->size;

	return 0;
}

int fstat(int fd, stat_t* output)
{
	auto* node = context->open_files[fd].inode;
	output->type = node->type;
	output->size = node->size;
	return 0;
}

ssize_t getdents(int fd, byte* buffer, size_t length)
{
	auto* inode = context->open_files[fd].inode;
	if(!inode)
		return -EBADF;

	if(inode->type != vnode_type::directory)
		return -ENOTDIR;

	return inode->ops->getdents(&context->open_files[fd], buffer, length);	
}	

}
