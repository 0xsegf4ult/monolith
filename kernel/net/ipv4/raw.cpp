#include <net/ipv4/raw.hpp>
#include <net/ipv4/route.hpp>
#include <net/ipv4/ipv4.hpp>
#include <net/ether.hpp>
#include <net/netdev.hpp>
#include <net/socket.hpp>
#include <mm/slab.hpp>
#include <sys/err.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>
#include <list.hpp>

static list_head_t socket_list;
static mutex_t list_lock;

void ipv4_raw_release(socket_t* socket)
{
	list_del(socket->list_node);
	kfree(socket->data);
}

int ipv4_raw_bind(socket_t* socket, const sockaddr* addr, socklen_t addrlen)
{
        sockaddr_in* in = (sockaddr_in*)addr;
        if(addrlen < sizeof(sockaddr_in))
                return -EFAULT;

        if(in->sin_family != AF_INET)
                return -EINVAL;

        auto* priv_data = (ipv4_sdata*)socket->data;
        priv_data->addr = in->sin_addr.s_addr;
        return 0;
}

ssize_t ipv4_raw_sendto(socket_t* socket, const byte* buf, size_t len, int flags, const sockaddr* dest_addr, socklen_t addrlen)
{
        if(!len)
                return 0;

        sockaddr_in* in = (sockaddr_in*)dest_addr;

        if(addrlen < sizeof(sockaddr_in))
                return -EFAULT;

        if(in->sin_family != AF_INET)
                return -EINVAL;

        auto addr = in->sin_addr.s_addr;

        netdev_t* netdev = nullptr;
        auto* priv_data = (ipv4_sdata*)socket->data;

	in_addr_t route_addr;
        if(addr == INADDR_BROADCAST)
        {
                netdev = priv_data->bind_device;
        	route_addr = netdev->ip_broadcast;
        }
	else
	{
		auto* route = ip_route_get(addr);
		if(!route)
			return -ENETUNREACH;

		log::debug("ipv4: found route to {:#x} via {:#x}", addr, route->gateway);
		netdev = route->device;
		route_addr = route->gateway;
	}

        if(!netdev)
                return -ENETUNREACH;

        return ipv4_tx_packet(netdev, route_addr, addr, priv_data->protocol, buf, len);
}

static socket_ops ipv4_raw_ops =
{
	.release = ipv4_raw_release,
	.bind = ipv4_raw_bind,
	.sendto = ipv4_raw_sendto
};

bool ipv4_raw_rx_packet(netdev_t* netdev, const ipv4_packet* packet, size_t length)
{
	bool found = false;

	sockaddr_in source;
	source.sin_family = AF_INET;
	source.sin_port = packet->protocol;
	source.sin_addr.s_addr = packet->src_addr;

	mutex_lock(list_lock);
	socket_t* cur;
	list_for_each_entry(cur, socket_list, list_node)
	{
		auto* priv_data = (ipv4_sdata*)cur->data;
		if(priv_data->protocol == packet->protocol)
		{
			found = true;
			socket_rx_packet(cur, (const sockaddr*)&source, sizeof(sockaddr_in), (byte*)packet + sizeof(ipv4_packet), length - sizeof(ipv4_packet));
		}
	}
	mutex_unlock(list_lock);

	return found;
}

void ipv4_raw_create_socket(socket_t* socket, ipv4_sdata* priv_data)
{
	socket->ops = &ipv4_raw_ops;

	mutex_lock(list_lock);
	list_add_tail(socket_list, socket->list_node);
	mutex_unlock(list_lock);
}

void ipv4_init_raw()
{
	list_node_init(socket_list);
	mutex_init(list_lock);
}
