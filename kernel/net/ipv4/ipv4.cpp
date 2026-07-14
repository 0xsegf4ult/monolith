#include <net/ipv4/ipv4.hpp>
#include <net/ipv4/raw.hpp>
#include <net/ipv4/route.hpp>
#include <net/ipv4/udp.hpp>
#include <net/arp.hpp>
#include <net/socket.hpp>
#include <net/netdev.hpp>
#include <net/ether.hpp>
#include <arch/x86_64/arch.hpp>
#include <fs/vfs.hpp>
#include <sys/err.hpp>
#include <sys/mutex.hpp>
#include <mm/slab.hpp>
#include <kstd.hpp>
#include <klog.hpp>

int ipv4_ioctl(vfs::file_descriptor_t* filp, uint64_t op, uint64_t arg)
{
	auto* socket = (socket_t*)filp->inode->data;
	if(!socket)
		return -ENOTTY;

	auto* priv_data = (ipv4_sdata*)socket->data;

	switch(op)
	{
	case 1:
	{
		if(!arg || arg > 0x7fffffffffff)
			return -EFAULT;

		netdev_t* dev = netdev_lookup((const char*)arg);
		if(!dev)
			return -ENODEV;

		priv_data->bind_device = dev;
		return 0;
	}
	case 2:
	{
		if(!arg || arg > 0x7fffffffffff)
                        return -EFAULT;

		if(!priv_data->bind_device)
			return -ENODEV;

		memcpy((byte*)arg, &priv_data->bind_device->mac_addr[0], 6);
		return 0;
	}
	case 3:
	{
		if(!arg || arg > 0x7fffffffffff)
                        return -EFAULT;

                if(!priv_data->bind_device)
                        return -ENODEV;

		sockaddr_in in;
		memcpy(&in, (byte*)arg, sizeof(sockaddr_in));
		if(in.sin_family != AF_INET)
			return -EINVAL;
	
		priv_data->bind_device->ip_addr = in.sin_addr.s_addr;
		return 0;
	}
	case 4:
	{
		if(!arg || arg > 0x7fffffffffff)
                        return -EFAULT;

                if(!priv_data->bind_device)
                        return -ENODEV;

		sockaddr_in in;
		memcpy(&in, (byte*)arg, sizeof(sockaddr_in));
		if(in.sin_family != AF_INET)
			return -EINVAL;

		priv_data->bind_device->ip_subnet = in.sin_addr.s_addr;
		return 0;
	}
  	case 5:
        {
                if(!arg || arg > 0x7fffffffffff)
                        return -EFAULT;

                if(!priv_data->bind_device)
                        return -ENODEV;
	
		net_route rt;
		memcpy(&rt, (byte*)arg, sizeof(net_route));
		if(rt.dest.sin_family != AF_INET)
			return -EINVAL;
		if(rt.mask.sin_family != AF_INET)
			return -EINVAL;
		if(rt.gateway.sin_family != AF_INET)
			return -EINVAL;	


		ip_route_entry* entry = (ip_route_entry*)kmalloc(sizeof(ip_route_entry));
		entry->device = priv_data->bind_device;
		entry->dest = rt.dest.sin_addr.s_addr;
		entry->mask = rt.mask.sin_addr.s_addr;
		entry->gateway = rt.gateway.sin_addr.s_addr;
		entry->metric = rt.metric;

		ip_route_add(entry);

		return 0;
	}
	}

	return -EINVAL;
}

static vfs::fs_inode_ops ipv4_raw_iops =
{
};

static vfs::fs_file_ops ipv4_raw_fops =
{
	.ioctl = ipv4_ioctl
};

static uint16_t ip_checksum(ipv4_packet* packet, size_t size)
{
	uint32_t sum = 0;
	uint16_t* ptr = (uint16_t*)packet;

	for(int i = 0; i < size / 2; i++)
	{
		sum += be16_to_native(ptr[i]);
		if(sum > 0xFFFF)
			sum = (sum >> 16) + (sum & 0xFFFF);
	}

	return ~(sum & 0xFFFF) & 0xFFFF;
}

void ipv4_rx_packet(netdev_t* netdev, const ipv4_packet* packet, size_t length)
{
	bool processed = ipv4_raw_rx_packet(netdev, packet, length);

	if(packet->protocol == IPPROTO_UDP)
	{
		udp_rx_packet(netdev, packet, length);
		return;
	}	

	if(!processed)
		log::debug("net: RX IPv4 packet unknown protocol {:#x} length {:#x}", packet->protocol, length);
}

ssize_t ipv4_tx_packet(netdev_t* netdev, in_addr_t route, in_addr_t dst_addr, int protocol, const byte* payload, size_t len)
{
	uint16_t pkt_id = atomic_fetch_add(&netdev->ip_tx_id, 1); 
	ipv4_packet* packet = (ipv4_packet*)kmalloc(sizeof(ipv4_packet) + len);
	packet->version_ihl = (4 << 4) | 5; // IPv4 | IHL 20 bytes
	packet->dscp_ecn = 0;
	packet->length = native_to_be16(sizeof(ipv4_packet) + len);
	packet->id = native_to_be16(pkt_id);
	packet->offset = 0;
	packet->ttl = 64;
	packet->protocol = protocol;
	packet->checksum = 0;
	packet->src_addr = netdev->ip_addr;
	packet->dst_addr = dst_addr;
	
	packet->checksum = native_to_be16(ip_checksum(packet, 20));

	memcpy((byte*)packet + sizeof(ipv4_packet), payload, len); 

	uint8_t dst_mac[6];
	uint8_t* dsthw = nullptr;

	if(route == netdev->ip_broadcast)
		dsthw = ether_mac_broadcast;
	else
	{
		auto* arp_lookup = arp_search(netdev, route);
		if(!arp_lookup)
		{
			log::debug("cant reach IP {:#x}", route);
			kfree(packet);
			return -ENETUNREACH;
		}

		memcpy(dst_mac, arp_lookup->mac, 6);
		dsthw = dst_mac;
	}

	if(!dsthw)
	{
		kfree(packet);
		return -ENETUNREACH;
	}

	ssize_t res = ether_send_packet(netdev, dsthw, ETHER_TYPE_IPV4, (byte*)packet, sizeof(ipv4_packet) + len);

	kfree(packet);
	return res;
}

int ipv4_create_socket(socket_t* socket, int type, int protocol)
{
	switch(type)
	{
	case SOCK_DGRAM:
	{
		if(protocol != 0 && protocol != IPPROTO_UDP)
		       return -EINVAL;

		socket->iops = &ipv4_raw_iops;
		socket->fops = &ipv4_raw_fops;
		auto priv_data = (ipv4_sdata*)kmalloc(sizeof(ipv4_sdata));
		priv_data->bind_device = nullptr;
		priv_data->protocol = protocol;	
		priv_data->addr = 0;
		priv_data->port = 0;	
		socket->data = priv_data;
		udp_create_socket(socket, priv_data);
		return 0;	
	}
	case SOCK_RAW:
	{
		if(protocol < 0 || protocol > 255)
			return -EINVAL;

		socket->iops = &ipv4_raw_iops;
		socket->fops = &ipv4_raw_fops;
		auto priv_data = (ipv4_sdata*)kmalloc(sizeof(ipv4_sdata));
		priv_data->bind_device = nullptr;	
		priv_data->protocol = protocol;	
		priv_data->addr = 0;
		priv_data->port = 0;
		socket->data = priv_data;
		ipv4_raw_create_socket(socket, priv_data);
		return 0;
	}
	}
		
	return -EINVAL;
}

void ipv4_init()
{
	register_socket_domain(AF_INET, ipv4_create_socket);
	ipv4_init_route();
	ipv4_init_raw();
	ipv4_init_udp();
}
