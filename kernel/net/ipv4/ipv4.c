#include <net/ipv4/ipv4.h>
#include <net/ipv4/raw.h>
#include <net/ipv4/route.h>
#include <net/ipv4/udp.h>
#include <net/arp.h>
#include <net/socket.h>
#include <net/netdev.h>
#include <net/ether.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <sys/mutex.h>
#include <libk/string.h>
#include <errno.h>
#include <cpu.h>
#include <klog.h>

int ipv4_ioctl(struct file_descriptor* file, uint64_t op, uint64_t arg)
{
	struct socket* socket = (struct socket*)file->inode->data;
	if(!socket)
		return -ENOTTY;

	struct ipv4_sdata* priv_data = (struct ipv4_sdata*)socket->data;

	switch(op)
	{
	case 1:
	{
		if(!vm_validate_ptr((void*)arg, 32))
			return -EFAULT;

		struct netdev* dev = netdev_lookup((const char*)arg);
		if(!dev)
			return -ENODEV;

		priv_data->bind_device = dev;
		return 0;
	}
	case 2:
	{
                if(!vm_validate_ptr((void*)arg, 6))
			return -EFAULT;

		if(!priv_data->bind_device)
			return -ENODEV;

		memcpy((byte*)arg, &priv_data->bind_device->mac_addr[0], 6);
		return 0;
	}
	case 3:
	{
                if(!vm_validate_ptr((void*)arg, sizeof(struct sockaddr_in)))
			return -EFAULT;

                if(!priv_data->bind_device)
                        return -ENODEV;

		struct sockaddr_in in;
		memcpy(&in, (byte*)arg, sizeof(struct sockaddr_in));
		if(in.sin_family != AF_INET)
			return -EINVAL;
	
		priv_data->bind_device->ip_addr = in.sin_addr.s_addr;
		return 0;
	}
	case 4:
	{
                if(!vm_validate_ptr((void*)arg, sizeof(struct sockaddr_in)))
			return -EFAULT;

                if(!priv_data->bind_device)
                        return -ENODEV;

		struct sockaddr_in in;
		memcpy(&in, (byte*)arg, sizeof(struct sockaddr_in));
		if(in.sin_family != AF_INET)
			return -EINVAL;

		priv_data->bind_device->ip_subnet = in.sin_addr.s_addr;
		return 0;
	}
  	case 5:
        {
		if(!vm_validate_ptr((void*)arg, sizeof(struct rtentry)))
                        return -EFAULT;

                if(!priv_data->bind_device)
                        return -ENODEV;
	
		struct rtentry rt;
		memcpy(&rt, (byte*)arg, sizeof(struct rtentry));
		if(rt.rt_dst.sin_family != AF_INET)
			return -EINVAL;
		if(rt.rt_gateway.sin_family != AF_INET)
			return -EINVAL;	
		if(rt.rt_genmask.sin_family != AF_INET)
			return -EINVAL;

		struct ip_route_entry* entry = kmalloc(sizeof(struct ip_route_entry));
		entry->device = priv_data->bind_device;
		entry->dest = rt.rt_dst.sin_addr.s_addr;
		entry->gateway = rt.rt_gateway.sin_addr.s_addr;
		entry->mask = rt.rt_genmask.sin_addr.s_addr;
		entry->metric = rt.rt_metric;
		entry->mtu = rt.rt_mtu;

		ip_route_add(entry);

		return 0;
	}
	}

	return -EINVAL;
}

static struct inode_ops ipv4_raw_iops =
{
};

static struct file_ops ipv4_raw_fops =
{
	.ioctl = ipv4_ioctl
};

static uint16_t ip_checksum(struct ipv4_packet* packet, size_t size)
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

void ipv4_rx_packet(struct netdev* netdev, const struct ipv4_packet* packet, size_t length)
{
	bool processed = ipv4_raw_rx_packet(netdev, packet, length);

	if(packet->protocol == IPPROTO_UDP)
	{
		udp_rx_packet(netdev, packet, length);
		return;
	}	

	if(!processed)
		klog("net: RX IPv4 packet unknown protocol %x length %x\n", packet->protocol, length);
}

ssize_t ipv4_tx_packet(struct netdev* netdev, in_addr_t route, in_addr_t dst_addr, int protocol, const byte* payload, size_t len)
{
	uint16_t pkt_id = atomic_fetch_add(&netdev->ip_tx_id, 1); 
	struct ipv4_packet* packet = kmalloc(sizeof(struct ipv4_packet) + len);
	packet->version_ihl = (4 << 4) | 5; // IPv4 | IHL 20 bytes
	packet->dscp_ecn = 0;
	packet->length = native_to_be16(sizeof(struct ipv4_packet) + len);
	packet->id = native_to_be16(pkt_id);
	packet->offset = 0;
	packet->ttl = 64;
	packet->protocol = protocol;
	packet->checksum = 0;
	packet->src_addr = netdev->ip_addr;
	packet->dst_addr = dst_addr;
	
	packet->checksum = native_to_be16(ip_checksum(packet, 20));

	memcpy((byte*)packet + sizeof(struct ipv4_packet), payload, len); 

	uint8_t dst_mac[6];
	uint8_t* dsthw = nullptr;

	if(route == netdev->ip_broadcast)
		dsthw = ether_mac_broadcast;
	else
	{
		struct arp_entry* arp_lookup = arp_search(netdev, route);
		if(!arp_lookup)
		{
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

	ssize_t res = ether_tx_packet(netdev, dsthw, ETHER_TYPE_IPV4, (byte*)packet, sizeof(struct ipv4_packet) + len);

	kfree(packet);
	return res;
}

int ipv4_create_socket(struct socket* socket, int type, int protocol)
{
	switch(type)
	{
	case SOCK_DGRAM:
	{
		if(protocol != 0 && protocol != IPPROTO_UDP)
		       return -EINVAL;

		socket->iops = &ipv4_raw_iops;
		socket->fops = &ipv4_raw_fops;
		struct ipv4_sdata* priv_data = kmalloc(sizeof(struct ipv4_sdata));
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
		struct ipv4_sdata* priv_data = kmalloc(sizeof(struct ipv4_sdata));
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
