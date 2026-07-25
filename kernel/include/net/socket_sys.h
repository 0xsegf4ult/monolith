#pragma once

#include <net/socket.h>
#include <types.h>

struct sockaddr;

int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
ssize_t sys_recvfrom(int sockfd, byte* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);
ssize_t sys_sendto(int sockfd, const byte* buffer, size_t len, int flags, const struct sockaddr* dst_addr, socklen_t addrlen);
