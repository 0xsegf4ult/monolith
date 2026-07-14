#pragma once

#include <sys/mutex.hpp>
#include <sys/waitqueue.hpp>
#include <types.hpp>
#include <list.hpp>

namespace vfs
{
	struct vnode_t;
	struct fs_inode_ops;
	struct fs_file_ops;
};

struct socket_t;

enum socket_domain
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

typedef int (*socket_create_t)(socket_t* sock, int type, int protocol);
struct socket_domain_t
{
	int domain;
	socket_create_t create;
	list_node_t list_node;
};

using sa_family_t = uint16_t;
using socklen_t = size_t;

struct sockaddr
{
	sa_family_t sa_family;
	char sa_data[14];
};

typedef void (*socket_release_t)(socket_t* sock);
typedef int (*socket_bind_t)(socket_t* sock, const sockaddr* addr, socklen_t addrlen);
typedef ssize_t (*socket_sendto_t)(socket_t* sock, const byte* buf, size_t len, int flags, const sockaddr* dest_addr, socklen_t addrlen);

struct socket_ops
{
	socket_release_t release = nullptr;
	socket_bind_t bind = nullptr;
	socket_sendto_t sendto = nullptr;
};

struct recv_queue_entry
{
	list_node_t list_node;
	size_t length;
	socklen_t addrlen;
	sockaddr source;
};

struct socket_t
{
	vfs::vnode_t* vnode;
	vfs::fs_inode_ops* iops;
	vfs::fs_file_ops* fops;
	socket_ops* ops;
	
	void* data;
	list_node_t list_node;
	socket_t* next; // hashtable
		
	list_head_t recv_queue;
	wait_queue recv_waitqueue;
	mutex_t recv_lock;

	bool bound;
};

void register_socket_domain(int domain, socket_create_t create);
int socket_create(socket_t** out_sock, int domain, int type, int protocol);
int socket_open(socket_t* sock);
void socket_put(socket_t* sock);

int socket_bind(socket_t* sock, const sockaddr* addr, socklen_t addrlen);
void socket_rx_packet(socket_t* sock, const sockaddr* source, socklen_t addrlen, const byte* payload, size_t len);
ssize_t socket_recvfrom(socket_t* sock, byte* buf, size_t len, int flags, sockaddr* src_addr, socklen_t* addrlen);
ssize_t socket_sendto(socket_t* sock, const byte* buf, size_t len, int flags, const sockaddr* dest_addr, socklen_t addrlen);
