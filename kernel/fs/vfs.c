#include <fs/vfs.h>
#include <fs/lookup.h>
#include <fs/stat.h>
#include <fs/vnode.h>
#include <fs/ventry.h>
#include <fs/ramfs/ramfs.h>

#include <mm/slab.h>

#include <sched/task.h>
#include <sys/smp.h>

#include <libk/list.h>
#include <libk/string.h>

#include <errno.h>
#include <types.h>
#include <panic.h>

static struct vfs_context context = {};

void vfs_init()
{
	ramfs_init();
	
	struct filesystem* ramfs = filesystem_lookup("ramfs");
	if(!ramfs)
		panic("failed to init VFS root");

	struct vnode* rnode = vnode_new(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
	rnode->iops = ramfs->iops;
	rnode->fops = ramfs->fops;

	context.root_node = ventry_new("/", rnode);
	
	struct mount* rootfs = kmalloc(sizeof(struct mount));
	rootfs->mountpoint = context.root_node;
	rootfs->fs = ramfs;
	rootfs->sb = kmalloc(sizeof(struct superblock));
	rootfs->sb->fs = ramfs;
	rootfs->sb->data = (void*)context.root_node;
	rootfs->sb->bdev = nullptr;
	context.root_node->mount = rootfs;

	list_node_init(&context.mounts);
	list_add_tail(&context.mounts, &rootfs->list_node);

	memset(context.open_files, 0, 64 * sizeof(struct file_descriptor));
}

struct vfs_context* vfs_context()
{
	return &context;
}

int vfs_create(const char* path, mode_t mode)
{
	struct ventry* parent = nullptr;
	int status = vfs_lookup(path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return status;

	size_t plen = strlen(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	struct ventry* result = nullptr;
	status = vfs_lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return -EEXIST;

	if(!parent->node->iops->create)
		return -ENOENT;

	return parent->node->iops->create(parent, basename, mode);	
}

int vfs_mkdir(const char* path, mode_t mode)
{
	struct ventry* parent = nullptr;
	int status = vfs_lookup(path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return status;

	size_t plen = strlen(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	struct ventry* result = nullptr;
	status = vfs_lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return -EEXIST;

	if(!parent->node->iops->mkdir)
		return -ENOENT;

	return parent->node->iops->mkdir(parent, basename, mode & 0777);
}

int vfs_mknod(const char* path, mode_t mode, dev_t device)
{
	struct ventry* parent = nullptr;
	int status = vfs_lookup(path, &parent, LOOKUP_PARENT);
	if(status < 0)
		return status;

	size_t plen = strlen(path);
	const char* basename = path;
	for(size_t i = 0; i < plen - 1; i++)
	{
		if(path[i] == '/')
			basename = &path[i + 1];
	}
	if(!basename)
		return -EINVAL;

	struct ventry* result = nullptr;
	status = vfs_lookup_at(parent, basename, &result, 0);
	if(status >= 0)
		return -EEXIST;

	if(!parent->node->iops->mknod)
		return -ENOENT;

	return parent->node->iops->mknod(parent, basename, mode, device);
}

int vfs_unlink(const char* path)
{
	struct ventry* dentry = nullptr;
	int status = vfs_lookup(path, &dentry, 0);
	if(status < 0)
		return status;

	if(S_ISDIR(dentry->node->mode))
	{
		return -EISDIR;
	}

	if(dentry->node->nlink <= 1)
	{
		dentry->node->nlink = 0;
		vnode_put(dentry->node);
	}
	else
		dentry->node->nlink--;

	spinlock_acquire(&dentry->ref.lock);

	dentry->node = nullptr;
	list_del(&dentry->sibling);

	spinlock_release(&dentry->ref.lock);
	ventry_put(dentry);

	return 0;
}

void vfs_ref_file(struct file_descriptor* file)
{
	atomic_fetch_add(&file->refcount, 1u);
}

int vfs_put_file(struct file_descriptor* file)
{
	uint32_t last = atomic_fetch_add(&file->refcount, -1);
	if(last > 1)
		return 0;

	int cres = 0;
	if(file->inode->fops->close)
		cres = file->inode->fops->close(file);
	
	if(cres < 0)
		return cres;

	file->pos = 0;

	vnode_put(file->inode);
	file->inode = nullptr;

	if(file->path)
	{
		ventry_put(file->path);
		file->path = nullptr;
	}

	file->fs_id = -1;
}

int vfs_open_internal(struct vnode* node, int flags, struct ventry* path)
{
	int fs_id = 0;
	if(node->fops->open)
		fs_id = node->fops->open(node, flags);

	if(fs_id < 0)
		return fs_id;

	vnode_ref(node);
	
	int fd = -ENFILE;
	for(int i = 0; i < 64; i++)
	{
		if(context.open_files[i].refcount == 0)
		{
			fd = i;
			context.open_files[i].pos = 0;
			context.open_files[i].inode = node;
			context.open_files[i].path = path;
			context.open_files[i].fs_id = fs_id;
			atomic_store(&context.open_files[i].refcount, 1);
			break;
		}
	}

	return fd;
}

int vfs_open(const char* path, int flags)
{
	struct ventry* query = nullptr;
	int status = vfs_lookup(path, &query, 0);
	if(status < 0)
	{
		if(status != -ENOENT)
			return status;

		if(flags & O_CREAT)
		{
			int c_res = vfs_create(path, 0666);
			if(c_res < 0)
				return c_res;

			status = vfs_lookup(path, &query, 0);
		}
		else
			return -ENOENT;
	}

	ventry_ref(query);
	struct vnode* node = query->node;
	if(!node)
	{
		ventry_put(query);
		return -EBADF;
	}

	int fd = vfs_open_internal(node, flags, query);
	if(fd < 0)
		ventry_put(query);

	return fd;
}

int vfs_openat(int fd, const char* path, int flags)
{
	struct ventry* dir;
       	if(fd == AT_FDCWD)
		dir = smp_current_task()->cwd;
	else 
		dir = context.open_files[fd].path;

	struct ventry* query = nullptr;
	int status = vfs_lookup_at(dir, path, &query, 0);
	if(status < 0)
		return status;

	ventry_ref(query);
	struct vnode* node = query->node;
	if(!node)
	{
		ventry_put(query);
		return -EBADF;
	}

	int s_fd = vfs_open_internal(node, flags, query);
	if(s_fd < 0)
		ventry_put(query);

	return s_fd;
}

ssize_t vfs_read(int fd, byte* buffer, size_t length)
{
	struct vnode* inode = context.open_files[fd].inode;
	if(!inode)
		return -EBADF;

	if(S_ISDIR(inode->mode))
	       return -EISDIR;	

	if(!inode->fops->read)
		return -EINVAL;

	return inode->fops->read(&context.open_files[fd], buffer, length);
}

ssize_t vfs_write(int fd, const byte* buffer, size_t length)
{
	struct vnode* inode = context.open_files[fd].inode;
	if(!inode)
		return -EBADF;

	if(S_ISDIR(inode->mode))
	       return -EISDIR;	

	if(!inode->fops->write)
		return -EINVAL;

	return inode->fops->write(&context.open_files[fd], buffer, length);
}

int vfs_close(int fd)
{
	struct vnode* node = context.open_files[fd].inode;
	if(!node)
		return -EBADF;
	

	int cres = vfs_put_file(&context.open_files[fd]);	
	if(cres < 0)
		return cres;

	return 0;
}

off_t vfs_seek(int fd, off_t offset, int flags)
{
	struct file_descriptor* file = &context.open_files[fd];

	mode_t mode = file->inode->mode;
	if(S_ISFIFO(mode) || S_ISSOCK(mode))
		return -ESPIPE;

	switch(flags)
	{
	case SEEK_SET:
		file->pos = offset;
		break;
	case SEEK_CUR:
		file->pos += offset;
		break;
	case SEEK_END:
		file->pos = offset + file->inode->size;
		break;
	}

	return file->pos;
}

int vfs_ioctl(int fd, uint64_t op, uint64_t arg)
{
	struct vnode* inode = context.open_files[fd].inode;
	if(inode == nullptr)
		return -EBADF;

	if(!inode->fops->ioctl)
		return -ENOTTY;

	return inode->fops->ioctl(&context.open_files[fd], op, arg);
}

static void stat_fill(struct vnode* node, struct stat* output)
{
	output->st_dev = 0;
	output->st_ino = 0;
	output->st_nlink = node->nlink;
	output->st_mode = node->mode;
	output->st_uid = node->uid;
	output->st_gid = node->gid;
	output->st_rdev = node->dev;
	output->st_size = node->size;
	output->st_blksize = 0;
	output->st_blocks = 0;
}

int vfs_stat(const char* path, struct stat* output)
{
	struct ventry* query = nullptr;
	int status = vfs_lookup(path, &query, 0);
	if(status < 0)
		return status;

	stat_fill(query->node, output);	

	return 0;
}

int vfs_fstat(int fd, struct stat* output)
{
	stat_fill(context.open_files[fd].inode, output);
	return 0;
}

ssize_t vfs_getdents(int fd, byte* buffer, size_t length)
{
	struct vnode* inode = context.open_files[fd].inode;
	
	if(!inode)
		return -EBADF;

	if(!S_ISDIR(inode->mode))
		return -ENOTDIR;

	if(!inode->iops->getdents)
		return -EINVAL;

	return inode->iops->getdents(&context.open_files[fd], buffer, length);	
}	

int vfs_dup(int fd)
{
	struct vnode* inode = context.open_files[fd].inode;
	if(!inode)
		return -EBADF;

	vfs_ref_file(&context.open_files[fd]);
	return fd;
}

struct file_descriptor* vfs_get_fd(int fd)
{
	return &context.open_files[fd];
}
