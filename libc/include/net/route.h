#ifndef _LIBC_NET_ROUTE_H
#define _LIBC_NET_ROUTE_H

#include <stdint.h>
#include <arpa/inet.h>

struct net_route
{
	struct sockaddr_in dest;
	struct sockaddr_in mask;
	struct sockaddr_in gateway;
	uint16_t metric;
};

#endif
