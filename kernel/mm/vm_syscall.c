#include <mm/vm_syscall.h>
#include <mm/vmm.h>
#include <fs/vfs.h>
#include <sched/task.h>
#include <sys/smp.h>
#include <errno.h>
#include <types.h>

void* sys_mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset)
{
	if(addr || !size || offset & 0xFFF)
		return (void*)EINVAL;

	if(!(flags & MAP_PRIVATE) && !(flags & MAP_SHARED))
		return (void*)EINVAL;

	if((flags & MAP_PRIVATE) && (flags & MAP_SHARED))
		return (void*)EINVAL;

	struct task* task = smp_current_task();
	bool is_anon = (flags & MAP_ANONYMOUS);
	if(is_anon)
	{
		if(fd >= 0)
			return (void*)EINVAL;

		if(flags & MAP_SHARED)
			return (void*)ENOTSUP;

		return (void*)vm_space_map(task->current_vm_space,
		(vm_mapping_info)
		{
			.length = size,
			.prot = (prot & 7) | PROT_USER
		});
	}
	else
	{
		if(fd < 0)
			return (void*)EBADF;

		int v_fd = task->open_files[fd];
		if(v_fd < 0)
			return (void*)EBADF;

		struct file_descriptor* file = vfs_get_fd(v_fd);
		if(!file->inode->fops->mmap)
			return (void*)ENODEV;

		return (void*)vm_space_map(task->current_vm_space,
		(vm_mapping_info)
		{
			.length = size,
			.prot = (prot & 7) | PROT_USER,
			.flags = VM_FLAG_FILE,
			.offset = offset,
			.fd = v_fd
		});
	}
}

int sys_munmap(void* addr, size_t size)
{
	if(!size || (virtaddr_t)addr & 0xFFF)
		return -EINVAL;

	if(!vm_validate_ptr(addr, size))
		return -EINVAL;

	vm_space_unmap(smp_current_task()->current_vm_space, (virtaddr_t)addr, size);
	return 0;
}

int sys_mprotect(void* addr, size_t size, int prot)
{
	return -ENOSYS;
}
