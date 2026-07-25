#include <net/socket_sys.h>
#include <net/socket.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <mm/vmm.h>
#include <sched/task.h>
#include <sys/smp.h>
#include <errno.h>
#include <types.h>

int sys_socket(int domain, int type, int protocol)
{
	struct socket* sock = nullptr;
	int status = socket_create(&sock, domain, type, protocol);
	if(status < 0)
		return status;

	int s_fd = socket_open(sock);
	if(s_fd < 0)
	{
		socket_put(sock);
		return s_fd;
	}

	struct task* task = smp_current_task();
	int fd = -1;
	for(int i = 0; i < 32; i++)
	{
		if(task->open_files[i] < 0)
		{
			fd = i;
			break;
		}
	}

	if(fd < 0)
	{
		vfs_close(s_fd);
		return -EMFILE;
	}

	task->open_files[fd] = s_fd;
	return fd;
}

int sys_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
	if(!vm_validate_ptr(addr, addrlen))
		return -EFAULT;

	struct task* task = smp_current_task();
	if(sockfd < 0 || task->open_files[sockfd] < 0)
		return -EBADF;

	struct file_descriptor* file = vfs_get_fd(task->open_files[sockfd]);
	if(!S_ISSOCK(file->inode->mode))
		return -ENOTSOCK;

	return socket_bind((struct socket*)file->inode->data, addr, addrlen);
}

ssize_t sys_recvfrom(int sockfd, byte* buffer, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen)
{
	if(!vm_validate_ptr(buffer, len))
		return -EFAULT;

	if(!vm_validate_ptr(src_addr, sizeof(struct sockaddr)))
		return -EFAULT;

	if(!vm_validate_ptr(addrlen, sizeof(socklen_t)))
		return -EFAULT;

	struct task* task = smp_current_task();
	if(sockfd < 0 || task->open_files[sockfd] < 0)
		return -EBADF;

	struct file_descriptor* file = vfs_get_fd(task->open_files[sockfd]);
	if(!S_ISSOCK(file->inode->mode))
		return -ENOTSOCK;

	return socket_recvfrom((struct socket*)file->inode->data, buffer, len, flags, src_addr, addrlen);
}

ssize_t sys_sendto(int sockfd, const byte* buffer, size_t len, int flags, const struct sockaddr* dst_addr, socklen_t addrlen)
{
	if(!vm_validate_ptr(buffer, len))
		return -EFAULT;

	if(!vm_validate_ptr(dst_addr, addrlen))
		return -EFAULT;

	struct task* task = smp_current_task();
	if(sockfd < 0 || task->open_files[sockfd] < 0)
		return -EBADF;

	struct file_descriptor* file = vfs_get_fd(task->open_files[sockfd]);
	if(!S_ISSOCK(file->inode->mode))
		return -ENOTSOCK;

	return socket_sendto((struct socket*)file->inode->data, buffer, len, flags, dst_addr, addrlen);
}
