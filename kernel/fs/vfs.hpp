#pragma once

#include <fs/ops.hpp>
#include <lib/types.hpp>
#include <dev/device.hpp>
#include <sys/mutex.hpp>
#include <sys/cred.hpp>
#include <sys/stat.hpp>

namespace vfs
{

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
	mutex_t lock;
};

struct vnode_t
{
	mode_t mode;
	size_t size;
	uid_t uid;
	gid_t gid;
	dev_t dev;
	
	void* data;
	fs_ops* ops;
	mutex_t lock;
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
	uint32_t refcount;
};

struct context_t
{
	ventry_t* root_node;	
	file_descriptor_t open_files[64];
};

void init();
ventry_t* get_root_dentry();
vnode_t* create_node(mode_t mode);
ventry_t* create_dentry(const char* name, vnode_t* node);

int create(const char* path, mode_t mode);
int mkdir(const char* path, mode_t mode);
int mknod(const char* path, mode_t mode, dev_t device);

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
