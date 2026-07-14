#pragma once

#include <types.hpp>
#include <stdatomic.h>
#include <list.hpp>

struct netdev_t;

struct netdev_ops
{
	ssize_t (*send)(netdev_t* netdev, byte* buffer, size_t size);
};

struct netdev_t
{
	char name[24];
	uint8_t mac_addr[6];

	uint32_t ip_addr;
	uint32_t ip_subnet;
	uint32_t ip_broadcast;

	atomic_ushort ip_tx_id;
	
	void* data;
	netdev_ops* ops;

	list_node_t list_node;
};

void netdev_init();
netdev_t* netdev_lookup(const char* name);
netdev_t* netdev_create();
