#pragma once

#include <fs/ops.h>
#include <fs/filesystem.h>
#include <fs/super.h>
#include <fs/vnode.h>
#include <fs/ventry.h>
#include <libk/list.h>
#include <types.h>
#include <stdatomic.h>

struct stat;

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

enum SEEK_FLAGS
{
        SEEK_SET = 0,
        SEEK_CUR = 1,
        SEEK_END = 2
};

struct file_descriptor
{
	off_t pos;
	struct vnode* inode;
	struct ventry* path;
	int fs_id;
	_Atomic uint32_t refcount;
};

struct vfs_context
{
	struct ventry* root_node;
	list_head_t mounts;
	struct file_descriptor open_files[64];
};

void vfs_init();
struct vfs_context* vfs_context();

int vfs_create(const char* path, mode_t mode);
int vfs_mkdir(const char* path, mode_t mode);
int vfs_mknod(const char* path, mode_t mode, dev_t device);
int vfs_unlink(const char* path);

int vfs_open(const char* path, int flags);
int vfs_openat(int fd, const char* path, int flags);
int vfs_close(int fd);

ssize_t vfs_read(int fd, byte* buffer, size_t len);
ssize_t vfs_write(int fd, const byte* buffer, size_t len);
off_t vfs_seek(int fd, off_t offset, int flags);
int vfs_ioctl(int fd, uint64_t op, uint64_t arg);
int vfs_stat(const char* path, struct stat* output);
int vfs_fstat(int fd, struct stat* output);
ssize_t vfs_getdents(int fd, byte* buffer, size_t length);
int vfs_dup(int fd);
struct file_descriptor* vfs_get_fd(int fd);
