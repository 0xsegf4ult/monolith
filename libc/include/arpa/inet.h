#ifndef _LIBC_ARPA_INET_H
#define _LIBC_ARPA_INET_H

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

#include <sys/socket.h>
#include <stdint.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

#define INADDR_ANY 		((in_addr_t) 0x00000000)
#define INADDR_BROADCAST	((in_addr_t) 0xFFFFFFFF)
#define INADDR_NONE		((in_addr_t) 0xFFFFFFFF)

#define IN_LOOPBACKNET		127
#define INADDR_LOOPBACK		((in_addr_t) 0x7F000001)

struct in_addr
{
	in_addr_t s_addr;
};

struct sockaddr_in
{
	sa_family_t sin_family;
	in_port_t sin_port;
	struct in_addr sin_addr;
	uint8_t sin_zero[sizeof(struct sockaddr) - sizeof(sa_family_t) - sizeof(in_port_t) - sizeof(struct in_addr)];
};

#define INET_ADDRSTRLEN 16

int inet_pton(int af, const char* s, void* in);
char* inet_ntop(int af, const void* src, char* dst, socklen_t size);

#define ntohl(x) __builtin_bswap32(x)
#define ntohs(x) __builtin_bswap16(x)
#define htonl(x) __builtin_bswap32(x)
#define htons(x) __builtin_bswap16(x)

#endif
