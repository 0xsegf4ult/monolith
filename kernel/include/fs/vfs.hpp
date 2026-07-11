#pragma once

#include <fs/ops.hpp>
#include <fs/filesystem.hpp>
#include <fs/super.hpp>
#include <fs/vnode.hpp>
#include <fs/ventry.hpp>
#include <types.hpp>
#include <dev/device.hpp>
#include <sys/mutex.hpp>
#include <sys/cred.hpp>
#include <sys/stat.hpp>
#include <stdatomic.h>
#include <sys/reflock.hpp>
#include <list.hpp>

namespace vfs
{

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
	list_head_t mounts;
	file_descriptor_t open_files[64];
};

void init();

context_t* get();
ventry_t* get_root_dentry();

int create(const char* path, mode_t mode);
int mkdir(const char* path, mode_t mode);
int mknod(const char* path, mode_t mode, dev_t device);
int unlink(const char* path);

struct stat_t 
{
	dev_t st_dev;
	mode_t st_mode;
	uint32_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	size_t st_size;
};

struct dirent_info
{
	uint16_t length;
	uint8_t type;
};

enum OPEN_FLAGS
{
	O_NONBLOCK = 0x0001,
	O_NDELAY = O_NONBLOCK,
	O_CLOEXEC = 0x0002,
	O_RDONLY = 0x0004,
	O_WRONLY = 0x0008,
	O_RDWR = O_RDONLY | O_WRONLY,
	O_APPEND = 0x0010,
	O_CREAT = 0x0020,
	O_DSYNC = 0x0040,
	O_EXCL = 0x0080,
	O_NOCTTY = 0x0100,
	O_RSYNC = 0x0200,
	O_SYNC = 0x0400,
	O_TRUNC = 0x0800,
	O_CLOFORK = 0x1000,
	O_ACCMODE = O_RDONLY | O_WRONLY | O_RDWR
};

int open(vnode_t* node, int flags, ventry_t* path);
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
