#include <net/ipv4/raw.h>
#include <net/ipv4/route.h>
#include <net/ipv4/ipv4.h>
#include <net/ether.h>
#include <net/netdev.h>
#include <net/socket.h>
#include <mm/slab.h>
#include <sys/mutex.h>
#include <libk/list.h>
#include <errno.h>
#include <klog.h>
#include <types.h>

static list_head_t socket_list;
static mutex_t list_lock;

void ipv4_raw_release(struct socket* socket)
{
	list_del(&socket->list_node);
	kfree(socket->data);
}

int ipv4_raw_bind(struct socket* socket, const struct sockaddr* addr, socklen_t addrlen)
{
        struct sockaddr_in* in = (struct sockaddr_in*)addr;
        if(addrlen < sizeof(struct sockaddr_in))
                return -EFAULT;

        if(in->sin_family != AF_INET)
                return -EINVAL;

        struct ipv4_sdata* priv_data = (struct ipv4_sdata*)socket->data;
        priv_data->addr = in->sin_addr.s_addr;
        return 0;
}

ssize_t ipv4_raw_sendto(struct socket* socket, const byte* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{
        if(!len)
                return 0;

        struct sockaddr_in* in = (struct sockaddr_in*)dest_addr;

        if(addrlen < sizeof(struct sockaddr_in))
                return -EFAULT;

        if(in->sin_family != AF_INET)
                return -EINVAL;

        in_addr_t addr = in->sin_addr.s_addr;

        struct netdev* netdev = nullptr;
        struct ipv4_sdata* priv_data = (struct ipv4_sdata*)socket->data;

	in_addr_t route_addr;
        if(addr == INADDR_BROADCAST)
        {
                netdev = priv_data->bind_device;
        	route_addr = netdev->ip_broadcast;
        }
	else
	{
		struct ip_route_entry* route = ip_route_get(addr);
		if(!route)
			return -ENETUNREACH;

		netdev = route->device;
		route_addr = route->gateway;
	}

        if(!netdev)
                return -ENETUNREACH;

        return ipv4_tx_packet(netdev, route_addr, addr, priv_data->protocol, buf, len);
}

static struct socket_ops ipv4_raw_ops =
{
	.release = ipv4_raw_release,
	.bind = ipv4_raw_bind,
	.sendto = ipv4_raw_sendto
};

bool ipv4_raw_rx_packet(struct netdev* netdev, const struct ipv4_packet* packet, size_t length)
{
	bool found = false;

	struct sockaddr_in source;
	source.sin_family = AF_INET;
	source.sin_port = packet->protocol;
	source.sin_addr.s_addr = packet->src_addr;

	mutex_lock(&list_lock);
	struct socket* cur;
	list_for_each_entry(cur, &socket_list, list_node)
	{
		struct ipv4_sdata* priv_data = (struct ipv4_sdata*)cur->data;
		if(priv_data->protocol == packet->protocol)
		{
			found = true;
			socket_rx_packet(cur, (const struct sockaddr*)&source, sizeof(struct sockaddr_in), (byte*)packet + sizeof(struct ipv4_packet), length - sizeof(struct ipv4_packet));
		}
	}
	mutex_unlock(&list_lock);

	return found;
}

void ipv4_raw_create_socket(struct socket* socket, struct ipv4_sdata* priv_data)
{
	socket->ops = &ipv4_raw_ops;

	mutex_lock(&list_lock);
	list_add_tail(&socket_list, &socket->list_node);
	mutex_unlock(&list_lock);
}

void ipv4_init_raw()
{
	list_node_init(&socket_list);
	mutex_init(&list_lock);
}
