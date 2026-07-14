#pragma once

#include <net/socket.hpp>
#include <types.hpp>

using in_addr_t = uint32_t;
using in_port_t = uint16_t;

constexpr const in_addr_t INADDR_ANY = 0x00000000;
constexpr const in_addr_t INADDR_BROADCAST = 0xFFFFFFFF;

struct in_addr
{
	in_addr_t s_addr;
};

struct sockaddr_in
{
	sa_family_t sin_family;
	in_port_t sin_port;
	in_addr sin_addr;
	uint8_t sin_zero[8];
};

struct netdev_t;

struct __attribute__((packed)) ipv4_packet
{
	uint8_t version_ihl;
	uint8_t dscp_ecn;
	uint16_t length;
	uint16_t id;
	uint16_t offset;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	in_addr_t src_addr;
	in_addr_t dst_addr;
};

struct ipv4_sdata
{
	netdev_t* bind_device;
	in_addr_t addr;
	in_port_t port;
	int protocol;
};

void ipv4_rx_packet(netdev_t* netdev, const ipv4_packet* packet, size_t length);
ssize_t ipv4_tx_packet(netdev_t* netdev, in_addr_t route, in_addr_t dst_addr, int protocol, const byte* payload, size_t len);
void ipv4_init();
