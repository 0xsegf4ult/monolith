#pragma once

#include <types.h>

struct netdev;

enum ether_types
{
	ETHER_TYPE_IPV4 = 0x0800,
	ETHER_TYPE_ARP 	= 0x0806
};

extern uint8_t ether_mac_broadcast[6];

struct __attribute__((packed)) ether_packet
{
	uint8_t mac_dst[6];
	uint8_t mac_src[6];
	uint16_t ether_type;
};

void ether_rx_packet(struct netdev* netdev, struct ether_packet* packet, size_t length);
ssize_t ether_tx_packet(struct netdev* netdev, uint8_t* mac_dst, uint16_t type, byte* payload, size_t length);
