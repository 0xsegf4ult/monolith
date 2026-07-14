#pragma once

#include <net/ipv4/ipv4.hpp>
#include <types.hpp>
#include <list.hpp>

struct netdev_t;

struct net_route
{
	sockaddr_in dest;
	sockaddr_in mask;
	sockaddr_in gateway;
	uint16_t metric;
};

struct ip_route_entry
{
        netdev_t* device;
        in_addr_t dest;
        in_addr_t mask;
        in_addr_t gateway;
        uint16_t metric;
	list_node_t list_node;
};

void ip_route_add(ip_route_entry* entry);
ip_route_entry* ip_route_get(in_addr_t addr);
void ipv4_init_route();
