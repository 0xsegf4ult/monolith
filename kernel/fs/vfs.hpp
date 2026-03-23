#pragma once

#include <fs/ops.hpp>
#include <lib/types.hpp>
#include <dev/device.hpp>

namespace vfs
{

enum class vnode_type : uint8_t
{
	directory,
	file,
	link,
	char_device,
	block_device
};

struct vfilesystem_t
{
	fs_ops ops;
	void* data;	
};

struct ventry_t
{
	char name[64];
	vnode_t* node;
	ventry_t* parent;
	ventry_t* children;
	ventry_t* sibling;
};

struct vnode_t
{
	vnode_type type;
	size_t size;
	void* data;
	fs_ops* ops;
	dev_t dev;
};

struct mountpoint_t
{
	
};

struct file_descriptor_t
{
	size_t read_pos;
	size_t write_pos;
	vnode_t* inode;
	ventry_t* path;
	int fs_id;
};

struct context_t
{
	ventry_t* root_node;	
	file_descriptor_t open_files[64];
};

void init();
ventry_t* get_root_dentry();

int create(const char* path);
int mkdir(const char* path);
int mknod(const char* path, char type, dev_t device);

struct lookup_result
{
	ventry_t* result;
	ventry_t* parent;
	const char* basename;
};

lookup_result lookup_at(ventry_t* parent, const char* path);
lookup_result lookup(const char* path);

struct stat_t
{
	vnode_type type;
	size_t size;
};

struct dirent_info
{
	uint16_t length;
	vnode_type type;
};

int open(const char* path, int flags = 0);
int close(int fd);
ssize_t read(int fd, byte* buffer, size_t len);
ssize_t write(int fd, const byte* buffer, size_t len);
ssize_t seek(int fd, ssize_t offset, int flags = 0);
int ioctl(int fd, uint64_t op, uint64_t arg = 0);
int stat(const char* path, stat_t* output);
ssize_t getdents(int fd, byte* buffer, size_t length);

}
