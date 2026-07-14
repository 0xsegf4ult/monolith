#ifndef _LIBC_SYS_SOCKET_H
#define _LIBC_SYS_SOCKET_H

#include <sys/types.h>
#include <sys/syscall.h>

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

typedef uint16_t sa_family_t;
typedef size_t socklen_t;

struct sockaddr
{
	sa_family_t sa_family;
	char sa_data[14];
};

static inline int socket(int domain, int type, int protocol)
{
	return (int)_syscall(SYS_SOCKET, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol, 0, 0, 0);
}

static inline int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
	return (int)_syscall(SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0, 0);
}

static inline ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen)
{
	return (ssize_t)_syscall(SYS_RECVFROM, (uint64_t)sockfd, (uint64_t)buf, (uint64_t)len, (uint64_t)flags, (uint64_t)src_addr, (uint64_t)addrlen);
}

static inline ssize_t sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
	return (ssize_t)_syscall(SYS_SENDTO, (uint64_t)sockfd, (uint64_t)buf, (uint64_t)len, (uint64_t)flags, (uint64_t)dest_addr, (uint64_t)addrlen);
}

#endif
