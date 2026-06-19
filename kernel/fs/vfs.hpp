#pragma once

#include <fs/ops.hpp>
#include <fs/vnode.hpp>
#include <fs/ventry.hpp>
#include <lib/types.hpp>
#include <dev/device.hpp>
#include <sys/mutex.hpp>
#include <sys/cred.hpp>
#include <sys/stat.hpp>
#include <stdatomic.h>
#include <sys/reflock.hpp>

namespace vfs
{

struct vfilesystem_t
{
	fs_inode_ops iops;
	fs_file_ops fops;
	ventry_t* root;
	void* data;	
};

struct mount_t
{
	vfilesystem_t* fs;	
	ventry_t* mountpoint;
	mount_t* next;
};

struct file_descriptor_t
{
	size_t read_pos;
	size_t write_pos;
	vnode_t* inode;
	ventry_t* path;
	int fs_id;
	atomic_uint refcount;
};

struct context_t
{
	ventry_t* root_node;	
	mount_t* mounts;
	file_descriptor_t open_files[64];
};

void init();
ventry_t* get_root_dentry();
int mount(const char* path, vfilesystem_t* fs);

int create(const char* path, mode_t mode);
int mkdir(const char* path, mode_t mode);
int mknod(const char* path, mode_t mode, dev_t device);
int unlink(const char* path);

enum LOOKUP_FLAGS
{
	LOOKUP_PARENT = 1
};

struct lookup_result
{
	ventry_t* result;
	const char* basename;
};

lookup_result lookup_at(ventry_t* parent, const char* path, int flags);
lookup_result lookup(const char* path, int flags);

struct stat_t
{
	mode_t mode;
	uint32_t nlinks;
	size_t size;
};

struct dirent_info
{
	uint16_t length;
	uint8_t type;
};

enum OPEN_FLAGS
{
	O_CREAT = 1
};

int open(const char* path, int flags = 0);
int openat(ventry_t* dir, const char* path, int flags = 0);
int openat(int fd, const char* path, int flags = 0);
int close(int fd);
ssize_t read(int fd, byte* buffer, size_t len);
ssize_t write(int fd, const byte* buffer, size_t len);
ssize_t seek(int fd, ssize_t offset, int flags = 0);
int ioctl(int fd, uint64_t op, uint64_t arg = 0);
int stat(const char* path, stat_t* output);
int fstat(int fd, stat_t* output); 
ssize_t getdents(int fd, byte* buffer, size_t length);
int dup(int fd);
file_descriptor_t& get_open_fd(int fd);

}
