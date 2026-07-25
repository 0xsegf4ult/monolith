#include <net/socket.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <sched/task.h>
#include <sched/scheduler.h>
#include <sched/signal.h>
#include <sched/waitqueue.h>
#include <sys/smp.h>
#include <libk/string.h>
#include <errno.h>
#include <klog.h>
#include <types.h>
#include <panic.h>

static list_head_t domain_list = {&domain_list, &domain_list};

void register_socket_domain(int domain, socket_create_t create)
{
	struct socket_domain* dom = kmalloc(sizeof(struct socket_domain));
	dom->domain = domain;
	dom->create = create;
	list_node_init(&dom->list_node);
	list_add_tail(&domain_list, &dom->list_node);
}

int socket_create(struct socket** out_sock, int domain, int type, int protocol)
{
	struct socket* socket = kmalloc(sizeof(struct socket));
	if(!socket)
		return -ENOMEM;

	socket->data = nullptr;
	struct socket_domain* tmp;
	struct socket_domain* dom = nullptr;
	list_for_each_entry(tmp, &domain_list, list_node)
	{
		if(tmp->domain == domain)
		{
			dom = tmp;
			break;
		}
	}

	if(!dom)
	{
		kfree(socket);
		return -EAFNOSUPPORT;
	}

	int status = dom->create(socket, type, protocol);
	if(status < 0)
	{
		kfree(socket);
		return status;
	}

	struct vnode* inode = vnode_new(S_IFSOCK | 0775);
	inode->data = socket;
	inode->iops = socket->iops;
	inode->fops = socket->fops;
	socket->vnode = inode;
	socket->bound = false;
	socket->next = nullptr;

	list_node_init(&socket->recv_queue);
	wait_queue_init(&socket->recv_waitqueue);
	mutex_init(&socket->recv_lock);

	*out_sock = socket;
	return 0;
}

int socket_open(struct socket* socket)
{
	return vfs_open_internal(socket->vnode, 0, nullptr);
}

int socket_bind(struct socket* sock, const struct sockaddr* addr, socklen_t addrlen)
{
	if(sock->bound)
		return -EINVAL;

	if(sock->ops->bind)
	{
		sock->bound = true;
		return sock->ops->bind(sock, addr, addrlen);
	}
		
	return -ENOTSUP;
}

void socket_put(struct socket* sock)
{
	wait_queue_wake(&sock->recv_waitqueue);

	mutex_lock(&sock->recv_lock);
	
	struct recv_queue_entry* tmp;
	struct recv_queue_entry* entry;
	list_for_each_entry_safe(entry, tmp, &sock->recv_queue, list_node)
	{
		list_del(&entry->list_node);
		kfree(entry);
	}

	if(sock->ops->release)
		sock->ops->release(sock);

	mutex_unlock(&sock->recv_lock);
	kfree(sock);
}

void socket_rx_packet(struct socket* sock, const struct sockaddr* source, socklen_t addrlen, const byte* payload, size_t len)
{
	struct recv_queue_entry* entry = kmalloc(sizeof(struct recv_queue_entry) + len);
	list_node_init(&entry->list_node);
	entry->length = len;
	entry->addrlen = addrlen;
	memcpy(&entry->source, source, sizeof(struct sockaddr));
	memcpy((byte*)entry + sizeof(struct recv_queue_entry), payload, len);

	mutex_lock(&sock->recv_lock);
	
	list_add_tail(&sock->recv_queue, &entry->list_node);
	wait_queue_wake(&sock->recv_waitqueue);
	mutex_unlock(&sock->recv_lock);
}

ssize_t socket_recvfrom(struct socket* sock, byte* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen)
{
	mutex_lock(&sock->recv_lock);
	if(list_empty(&sock->recv_queue))
	{
		struct task* task = smp_current_task();
		ssize_t wret = 0;
		task_status exp_status = TASK_RUNNING;
		while(1)
		{
			wait_queue_register(&sock->recv_waitqueue, &task->wait);
			if(!atomic_compare_exchange_strong(&task->status, &exp_status, TASK_INTR_SLEEPING))
			{
				if(exp_status != TASK_INTR_SLEEPING)
					panic("wq_register cmpxchg failed");
			}

			if(!list_empty(&sock->recv_queue))
				break;

			if(signal_pending(task))
			{
				wret = -EINTR;
				break;
			}

			mutex_unlock(&sock->recv_lock);
			sched_yield();
			mutex_lock(&sock->recv_lock);
		}
		wait_queue_unregister(&task->wait);
		exp_status = TASK_INTR_SLEEPING;
		atomic_compare_exchange_strong(&task->status, &exp_status, TASK_RUNNING);
		if(wret < 0)
		{
			mutex_unlock(&sock->recv_lock);
			return wret;
		}
	}

	struct recv_queue_entry* entry = list_first_entry(&sock->recv_queue, struct recv_queue_entry, list_node);
	list_del(&entry->list_node);

	mutex_unlock(&sock->recv_lock);

	size_t readlen = entry->length;
	if(readlen > len)
		readlen = len;

	memcpy(buf, (byte*)entry + sizeof(struct recv_queue_entry), readlen);
	memcpy(src_addr, &entry->source, sizeof(struct sockaddr));
	memcpy(addrlen, &entry->addrlen, sizeof(socklen_t));

	kfree(entry);
	return readlen;
}

ssize_t socket_sendto(struct socket* sock, const byte* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
	if(sock->ops->sendto)
		return sock->ops->sendto(sock, buf, len, flags, dest_addr, addrlen);
		
	return -ENOTSUP;	
}
