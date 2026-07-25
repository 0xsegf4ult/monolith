#include <net/ether.h>
#include <net/arp.h>
#include <net/netdev.h>
#include <net/ipv4/ipv4.h>
#include <mm/slab.h>
#include <libk/string.h>
#include <cpu.h>
#include <klog.h>
#include <types.h>

uint8_t ether_mac_broadcast[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

void ether_rx_packet(struct netdev* netdev, struct ether_packet* packet, size_t length)
{
	if(!memcmp(packet->mac_dst, netdev->mac_addr, 6) || !memcmp(packet->mac_dst, ether_mac_broadcast, 6))
	{
		auto type = be16_to_native(packet->ether_type);
		switch(type)
		{
		case ETHER_TYPE_ARP:
			arp_rx_packet(netdev, (struct arp_packet*)((byte*)packet + sizeof(struct ether_packet)), length - sizeof(struct ether_packet));
			break;
		case ETHER_TYPE_IPV4:
			ipv4_rx_packet(netdev, (const struct ipv4_packet*)((byte*)packet + sizeof(struct ether_packet)), length - sizeof(struct ether_packet));
			break;
		default:
			klog("net: RX unknown packet EtherType %x length %x\n", type, length);
			break;
		}
	}
}

ssize_t ether_tx_packet(struct netdev* netdev, uint8_t* mac_dst, uint16_t type, byte* payload, size_t length)
{
	struct ether_packet* packet = kmalloc(sizeof(struct ether_packet) + length);
	memcpy(packet->mac_dst, mac_dst, 6);
	memcpy(packet->mac_src, netdev->mac_addr, 6);
	packet->ether_type = native_to_be16(type);

	memcpy(((byte*)packet) + sizeof(struct ether_packet), payload, length);

	ssize_t sent = netdev->ops->send(netdev, (byte*)packet, sizeof(struct ether_packet) + length);
	kfree(packet);
	return sent;
}
