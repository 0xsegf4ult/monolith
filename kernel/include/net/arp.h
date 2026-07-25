#pragma once

#include <types.h>

struct netdev;

enum arp_opcode : uint16_t
{
	ARP_OPCODE_REQUEST = 0x0001,
	ARP_OPCODE_REPLY   = 0x0002
};

constexpr uint16_t arp_htype_ethernet = 0x1;
constexpr uint16_t arp_ptype_ipv4 = 0x0800;

struct __attribute__((packed)) arp_packet
{
	uint16_t htype;
	uint16_t ptype;
	uint8_t hlen;
	uint8_t plen;
	uint16_t opcode;

	uint8_t src_hw[6];
	uint32_t src_proto;
	uint8_t dst_hw[6];
	uint32_t dst_proto;
};

struct arp_entry
{
	uint32_t address;
	uint16_t htype;
	uint8_t mac[6];
	struct netdev* netdev;
	struct arp_entry* next;
};

struct arp_entry* arp_get_entry(uint32_t address);
struct arp_entry* arp_search(struct netdev* netdev, uint32_t address);
void arp_rx_packet(struct netdev* netdev, struct arp_packet* packet, size_t length);
void arp_init();
