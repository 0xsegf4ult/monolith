#include <net/socket.hpp>
#include <fs/vfs.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <sys/err.hpp>
#include <sys/task.hpp>
#include <sys/scheduler.hpp>
#include <sys/signal.hpp>
#include <sys/smp.hpp>
#include <sys/waitqueue.hpp>
#include <klog.hpp>
#include <kstd.hpp>
#include <types.hpp>
#include <panic.hpp>

static list_head_t domain_list = {&domain_list, &domain_list};

void register_socket_domain(int domain, socket_create_t create)
{
	auto* dom = (socket_domain_t*)kmalloc(sizeof(socket_domain_t));
	dom->domain = domain;
	dom->create = create;
	list_node_init(dom->list_node);
	list_add_tail(domain_list, dom->list_node);
}

int socket_create(socket_t** out_sock, int domain, int type, int protocol)
{
	auto* socket = (socket_t*)kmalloc(sizeof(socket_t));
	if(!socket)
		return -ENOMEM;

	socket->data = nullptr;
	socket_domain_t* tmp;
	socket_domain_t* dom = nullptr;
	list_for_each_entry(tmp, domain_list, list_node)
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

	auto status = dom->create(socket, type, protocol);
	if(status < 0)
	{
		kfree(socket);
		return status;
	}

	auto* inode = vfs::vnode_new(S_IFSOCK | 0775);
	inode->data = socket;
	inode->iops = socket->iops;
	inode->fops = socket->fops;
	socket->vnode = inode;
	socket->bound = false;
	socket->next = nullptr;

	list_node_init(socket->recv_queue);
	wait_queue_init(socket->recv_waitqueue);
	mutex_init(socket->recv_lock);

	*out_sock = socket;
	return 0;
}

int socket_open(socket_t* socket)
{
	return vfs::open(socket->vnode, 0, nullptr);
}

int socket_bind(socket_t* sock, const sockaddr* addr, socklen_t addrlen)
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

void socket_put(socket_t* sock)
{
	wait_queue_wake(sock->recv_waitqueue);

	mutex_lock(sock->recv_lock);
	
	recv_queue_entry* tmp;
	recv_queue_entry* entry;
	list_for_each_entry_safe(entry, tmp, sock->recv_queue, list_node)
	{
		list_del(entry->list_node);
		kfree(entry);
	}

	if(sock->ops->release)
		sock->ops->release(sock);

	mutex_unlock(sock->recv_lock);
	kfree(sock);
}

void socket_rx_packet(socket_t* sock, const sockaddr* source, socklen_t addrlen, const byte* payload, size_t len)
{
	auto* entry = (recv_queue_entry*)kmalloc(sizeof(recv_queue_entry) + len);
	list_node_init(entry->list_node);
	entry->length = len;
	entry->addrlen = addrlen;
	memcpy(&entry->source, source, sizeof(sockaddr));
	memcpy((byte*)entry + sizeof(recv_queue_entry), payload, len);

	log::debug("socket_rx_packet: payload length {:#x}", len);	
	mutex_lock(sock->recv_lock);
	
	list_add_tail(sock->recv_queue, entry->list_node);
	wait_queue_wake(sock->recv_waitqueue);
	mutex_unlock(sock->recv_lock);
}

ssize_t socket_recvfrom(socket_t* sock, byte* buf, size_t len, int flags, sockaddr* src_addr, socklen_t* addrlen)
{
	mutex_lock(sock->recv_lock);
	if(list_empty(sock->recv_queue))
	{
		auto* task = smp_current_task();
		ssize_t wret = 0;
		task_status exp_status = TASK_RUNNING;
		while(1)
		{
			wait_queue_register(sock->recv_waitqueue, task->wait);
			if(!atomic_compare_exchange_strong(&task->status, &exp_status, TASK_INTR_SLEEPING))
			{
				if(exp_status != TASK_INTR_SLEEPING)
					panic("wq_register cmpxchg failed");
			}

			if(!list_empty(sock->recv_queue))
				break;

			if(signal_pending(task))
			{
				wret = -EINTR;
				break;
			}

			mutex_unlock(sock->recv_lock);
			sched_yield();
			mutex_lock(sock->recv_lock);
		}
		wait_queue_unregister(task->wait);
		exp_status = TASK_INTR_SLEEPING;
		atomic_compare_exchange_strong(&task->status, &exp_status, TASK_RUNNING);
		if(wret < 0)
		{
			mutex_unlock(sock->recv_lock);
			return wret;
		}
	}

	auto* entry = list_first_entry(&sock->recv_queue, recv_queue_entry, list_node);
	list_del(entry->list_node);

	mutex_unlock(sock->recv_lock);

	size_t readlen = entry->length;
	if(readlen > len)
		readlen = len;

	memcpy(buf, (byte*)entry + sizeof(recv_queue_entry), readlen);
	memcpy(src_addr, &entry->source, sizeof(sockaddr));
	memcpy(addrlen, &entry->addrlen, sizeof(socklen_t));

	kfree(entry);
	return readlen;
}

ssize_t socket_sendto(socket_t* sock, const byte* buf, size_t len, int flags, const sockaddr* dest_addr, socklen_t addrlen)
{
	if(sock->ops->sendto)
		return sock->ops->sendto(sock, buf, len, flags, dest_addr, addrlen);
		
	return -ENOTSUP;	
}
