#pragma once

#include <net/ipv4/ipv4.h>
#include <libk/list.h>
#include <types.h>

struct netdev;

struct rtentry
{
	uint64_t rt_pad1;
	struct sockaddr_in rt_dst;
	struct sockaddr_in rt_gateway;
	struct sockaddr_in rt_genmask;
	uint16_t rt_flags;
	int16_t rt_pad2;
	uint64_t rt_pad3;
	uint8_t rt_tos;
	uint8_t rt_class;
	int16_t rt_pad4[3];
	int16_t rt_metric;
	char* rt_dev;
	uint64_t rt_mtu;
	uint64_t rt_window;
	uint16_t rt_irtt;
};

struct ip_route_entry
{
        struct netdev* device;
        in_addr_t dest;
        in_addr_t gateway;
        in_addr_t mask;
        uint16_t metric;
	uint16_t mtu;
	list_node_t list_node;
};

void ip_route_add(struct ip_route_entry* entry);
struct ip_route_entry* ip_route_get(in_addr_t addr);
void ipv4_init_route();
