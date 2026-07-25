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

struct posix_dent
{
	ino_t d_ino;
	off_t d_off;
	uint16_t d_reclen;
	uint8_t d_type;
	char d_name[1024];
};

enum OPEN_FLAGS
{
	O_RDONLY	= 0x000000,
	O_WRONLY	= 0x000001,
	O_RDWR		= 0x000002,
	O_CREAT		= 0x000040,
	O_EXCL		= 0x000080,
	O_NOCTTY 	= 0x000100,
	O_TRUNC		= 0x000200,
	O_APPEND	= 0x000400,
	O_NONBLOCK	= 0x000800,
	O_DSYNC		= 0x001000,
	O_ASYNC		= 0x002000,
	O_DIRECT	= 0x004000,
	O_NOFOLLOW	= 0x020000,
	O_CLOEXEC	= 0x080000,
	O_SYNC		= 0x101000,
	O_RSYNC		= 0x101000,
	O_PATH		= 0x200000,
	O_ACCMODE	= O_RDWR | O_WRONLY | O_PATH
};

enum SEEK_FLAGS
{
        SEEK_SET = 0,
        SEEK_CUR = 1,
        SEEK_END = 2
};

constexpr size_t PATH_MAX = 256;
constexpr int AT_FDCWD = -100;

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

void vfs_ref_file(struct file_descriptor* file);
int vfs_put_file(struct file_descriptor* file);

int vfs_open_internal(struct vnode* node, int flags, struct ventry* ventry);
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
