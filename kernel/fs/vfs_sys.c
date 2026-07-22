#include <fs/vfs_sys.h>
#include <fs/stat.h>
#include <fs/super.h>
#include <fs/vfs.h>

#include <mm/vmm.h>

#include <sched/task.h>
#include <sys/smp.h>

#include <errno.h>
#include <types.h>

int sys_open(const char* path, int flags)
{
	if(!path)
		return -EINVAL;

	if(!vm_validate_ptr(path, PATH_MAX))
		return -EFAULT;

	struct task* task = smp_current_task();
	int fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] == -1)
		{
			fd = i;
			break;
		}
	}

	if(fd < 0)
		return -EMFILE;
	
	int v_fd = vfs_open(path, flags);
	if(v_fd < 0)
		return v_fd;

	task->open_files[fd] = v_fd;
	return fd;
}

int sys_openat(int fd, const char* path, int flags)
{
	struct task* task = smp_current_task();
	if(!path)
		return -EINVAL;

	if(!vm_validate_ptr(path, PATH_MAX))
		return -EFAULT;

	int user_fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] == -1)
		{
			user_fd = i;
			break;
		}
	}

	if(user_fd < 0)
		return -EMFILE;

	int sys_fd = fd;
	if(sys_fd != AT_FDCWD)
	{
		if(sys_fd < 0 || task->open_files[sys_fd] < 0)
			return -EBADF;

		sys_fd = task->open_files[sys_fd];
	}

	int v_fd = vfs_openat(sys_fd, path, flags);
	if(v_fd < 0)
		return v_fd;

	task->open_files[user_fd] = v_fd;
	return user_fd;
}

int sys_close(int fd)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	int v_fd = task->open_files[fd];
	task->open_files[fd] = -1;

	return vfs_close(v_fd);
}

ssize_t sys_read(int fd, byte* buffer, size_t length)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	if(!vm_validate_ptr(buffer, length))
		return -EFAULT;

	return vfs_read(task->open_files[fd], buffer, length);
}

ssize_t sys_write(int fd, const byte* buffer, size_t length)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	if(!vm_validate_ptr(buffer, length))
		return -EFAULT;

	return vfs_write(task->open_files[fd], buffer, length);
}

off_t sys_seek(int fd, off_t offset, int flags)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	return vfs_seek(task->open_files[fd], offset, flags);
}

int sys_dup(int fd)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	int newfd = -EMFILE;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] < 0)
		{
			newfd = i;
			break;
		}
	}

	if(newfd < 0)
		return newfd;

	task->open_files[newfd] = vfs_dup(task->open_files[fd]);
	return newfd;
}

int sys_ioctl(int fd, uint64_t op, uint64_t arg)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	return vfs_ioctl(task->open_files[fd], op, arg);
}

int sys_stat(const char* path, struct stat* buffer)
{
	if(!vm_validate_ptr(path, PATH_MAX))
		return -EFAULT;

	if(!vm_validate_ptr(buffer, sizeof(struct stat)))
		return -EFAULT;

	return vfs_stat(path, buffer);
}

int sys_fstat(int fd, struct stat* buffer)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	if(!vm_validate_ptr(buffer, sizeof(struct stat)))
		return -EFAULT;

	return vfs_fstat(task->open_files[fd], buffer);
}

ssize_t sys_getdents(int fd, byte* buffer, size_t length)
{
	struct task* task = smp_current_task();
	if(fd < 0 || task->open_files[fd] < 0)
		return -EBADF;

	if(!vm_validate_ptr(buffer, length))
		return -EFAULT;

	return vfs_getdents(task->open_files[fd], buffer, length);
}

int sys_mkdir(const char* path, mode_t mode)
{
	if(!vm_validate_ptr(path, PATH_MAX))
		return -EFAULT;

	return vfs_mkdir(path, mode);
}

int sys_mount(const char* source, const char* target, const char* fsname)
{
	if(source && !vm_validate_ptr(source, PATH_MAX))
		return -EFAULT;

	if(!vm_validate_ptr(target, PATH_MAX))
		return -EFAULT;

	if(!vm_validate_ptr(fsname, 32))
		return -EFAULT;

	return vfs_mount(source, target, fsname);
}

int sys_umount(const char* target)
{
	return -ENOSYS;
}
