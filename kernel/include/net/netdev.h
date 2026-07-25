#pragma once

#include <libk/list.h>
#include <types.h>
#include <stdatomic.h>

struct netdev;

struct netdev_ops
{
	ssize_t (*send)(struct netdev* netdev, byte* buffer, size_t size);
};

struct netdev
{
	char name[24];
	uint8_t mac_addr[6];

	uint32_t ip_addr;
	uint32_t ip_subnet;
	uint32_t ip_broadcast;

	_Atomic uint16_t ip_tx_id;
	
	void* data;
	struct netdev_ops* ops;

	list_node_t list_node;
};

void netdev_init();
struct netdev* netdev_lookup(const char* name);
struct netdev* netdev_create();
