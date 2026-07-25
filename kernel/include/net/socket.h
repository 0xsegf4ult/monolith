#pragma once

#include <sched/waitqueue.h>
#include <sys/mutex.h>
#include <libk/list.h>
#include <types.h>

struct vnode;
struct file_ops;
struct inode_ops;
struct socket;

enum socket_domain_types
{
	AF_UNIX = 1,
	AF_INET = 2,
	AF_INET6 = 10
};

enum socket_type
{
	SOCK_STREAM = 1,
	SOCK_DGRAM = 2,
	SOCK_RAW = 3
};

enum socket_protocol
{
	IPPROTO_IP = 0,
	IPPROTO_ICMP = 1,
	IPPROTO_TCP = 6,
	IPPROTO_UDP = 17
};

typedef int (*socket_create_t)(struct socket* sock, int type, int protocol);
struct socket_domain
{
	int domain;
	socket_create_t create;
	list_node_t list_node;
};

typedef uint16_t sa_family_t;
typedef size_t socklen_t;

struct sockaddr
{
	sa_family_t sa_family;
	char sa_data[14];
};


struct socket_ops
{
	void (*release)(struct socket* sock);
	int (*bind)(struct socket* sock, const struct sockaddr* addr, socklen_t addrlen);
	ssize_t (*sendto)(struct socket* sock, const byte* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);
};

struct recv_queue_entry
{
	list_node_t list_node;
	size_t length;
	socklen_t addrlen;
	struct sockaddr source;
};

struct socket
{
	struct vnode* vnode;
	struct inode_ops* iops;
	struct file_ops* fops;
	struct socket_ops* ops;
	
	void* data;
	list_node_t list_node;
	struct socket* next; // hashtable
		
	list_head_t recv_queue;
	wait_queue recv_waitqueue;
	mutex_t recv_lock;

	bool bound;
};

void register_socket_domain(int domain, socket_create_t create);
int socket_create(struct socket** out_sock, int domain, int type, int protocol);
int socket_open(struct socket* sock);
void socket_put(struct socket* sock);

int socket_bind(struct socket* sock, const struct sockaddr* addr, socklen_t addrlen);
void socket_rx_packet(struct socket* sock, const struct sockaddr* source, socklen_t addrlen, const byte* payload, size_t len);
ssize_t socket_recvfrom(struct socket* sock, byte* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
ssize_t socket_sendto(struct socket* sock, const byte* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);
