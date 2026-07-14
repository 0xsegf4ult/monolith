#include <net/ether.hpp>
#include <net/arp.hpp>
#include <net/netdev.hpp>
#include <net/ipv4/ipv4.hpp>
#include <mm/slab.hpp>
#include <kstd.hpp>
#include <klog.hpp>

#include <arch/x86_64/arch.hpp>

uint8_t ether_mac_broadcast[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

void ether_recv_packet(netdev_t* netdev, ether_packet* packet, size_t length)
{
	if(!memcmp(packet->mac_dst, netdev->mac_addr, 6) || !memcmp(packet->mac_dst, ether_mac_broadcast, 6))
	{
		auto type = be16_to_native(packet->ether_type);
		switch(type)
		{
		case ETHER_TYPE_ARP:
			arp_recv_packet(netdev, (arp_packet*)((byte*)packet + sizeof(ether_packet)), length - sizeof(ether_packet));
			break;
		case ETHER_TYPE_IPV4:
			ipv4_rx_packet(netdev, (const ipv4_packet*)((byte*)packet + sizeof(ether_packet)), length - sizeof(ether_packet));
			break;
		default:
			log::debug("net: RX unknown packet EtherType {:#x} length {:#x}", type, length);
			break;
		}
	}
}

ssize_t ether_send_packet(netdev_t* netdev, uint8_t* mac_dst, uint16_t type, byte* payload, size_t length)
{
	ether_packet* packet = (ether_packet*)kmalloc(sizeof(ether_packet) + length);
	memcpy(packet->mac_dst, mac_dst, 6);
	memcpy(packet->mac_src, netdev->mac_addr, 6);
	packet->ether_type = native_to_be16(type);

	memcpy(((byte*)packet) + sizeof(ether_packet), payload, length);

	auto sent = netdev->ops->send(netdev, (byte*)packet, sizeof(ether_packet) + length);
	kfree(packet);
	return sent;
}
