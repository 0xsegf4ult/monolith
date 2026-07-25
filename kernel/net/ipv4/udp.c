#include <net/ipv4/udp.h>
#include <net/ipv4/ipv4.h>
#include <net/ipv4/route.h>
#include <net/ether.h>
#include <net/netdev.h>
#include <net/socket.h>
#include <mm/slab.h>
#include <sys/mutex.h>
#include <libk/string.h>
#include <errno.h>
#include <klog.h>
#include <types.h>
#include <stdatomic.h>

constexpr uint16_t ephemeral_port_start = 49152; 

static struct socket* udp_table[256] = {0};
static mutex_t udp_table_lock;
static _Atomic uint16_t next_eph_port = ephemeral_port_start;

static uint32_t ip_port_hashf(in_addr_t addr, in_port_t port)
{
	uint32_t hash = 17;
	hash = hash * 5 + addr;
	hash = hash * 5 + port;
	return hash % 256;
}

static struct socket* udp_table_get(in_addr_t addr, in_port_t port)
{
	uint32_t hash = ip_port_hashf(addr, port);

	mutex_lock(&udp_table_lock);
	struct socket* entry = udp_table[hash];
	while(entry)
	{
		struct ipv4_sdata* priv_data = (struct ipv4_sdata*)entry->data;
		if(priv_data->addr == addr && priv_data->port == port)
			break;

		entry = entry->next;
	}
	mutex_unlock(&udp_table_lock);
	return entry;
}

static void udp_table_insert(struct socket* socket, in_addr_t addr, in_port_t port)
{
	uint32_t hash = ip_port_hashf(addr, port);

	mutex_lock(&udp_table_lock);
	socket->next = udp_table[hash];
	udp_table[hash] = socket;
	mutex_unlock(&udp_table_lock);
}

static void udp_table_remove(struct socket* socket, in_addr_t addr, in_port_t port)
{
	uint32_t hash = ip_port_hashf(addr, port);

	mutex_lock(&udp_table_lock);
	struct socket** cur = &udp_table[hash];
	while(*cur)
	{
		if(*cur == socket)
		{
			*cur = socket->next;
			socket->next = nullptr;
			break;
		}
		cur = &(*cur)->next;
	}
	mutex_unlock(&udp_table_lock);
}

static bool udp_register(struct socket* socket, struct ipv4_sdata* priv_data)
{
	struct socket* query = udp_table_get(priv_data->addr, priv_data->port);
	if(query)
		return false;

	udp_table_insert(socket, priv_data->addr, priv_data->port);
	return true;
}

static uint16_t udp_get_ephemeral_port()
{
	uint16_t port = 0;

	for(;;)
	{
		uint16_t next_port = atomic_fetch_add(&next_eph_port, 1);
		uint16_t nport = native_to_be16(next_port);
		struct socket* query = udp_table_get(0, nport);
		
		if(!query)
		{
			port = nport;
			break;
		}
	}

	return port;
}

void udp_release(struct socket* socket)
{
	if(!socket->bound)
		return;

	struct ipv4_sdata* priv_data = (struct ipv4_sdata*)socket->data;
	if(!udp_table_get(priv_data->addr, priv_data->port))
	{
		klog("UDP: tried to release unregistered socket\n");
		return;
	}

	udp_table_remove(socket, priv_data->addr, priv_data->port);
	kfree(priv_data);
}

int udp_bind(struct socket* socket, const struct sockaddr* addr, socklen_t addrlen)
{       
        struct sockaddr_in* in = (struct sockaddr_in*)addr;
        if(addrlen < sizeof(struct sockaddr_in))
                return -EFAULT;

        if(in->sin_family != AF_INET)
                return -EINVAL;
        
        struct ipv4_sdata* priv_data = (struct ipv4_sdata*)socket->data;
        priv_data->addr = in->sin_addr.s_addr;
        priv_data->port = in->sin_port;
        
	if(!udp_register(socket, priv_data))
	{
		priv_data->port = 0;
		socket->bound = false;
		return -EADDRINUSE;
	}
	
	return 0;
}
       
ssize_t udp_sendto(struct socket* socket, const byte* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen)
{       
        if(!len)
                return 0;

        struct sockaddr_in* in = (struct sockaddr_in*)dest_addr;
                
        if(addrlen < sizeof(struct sockaddr_in))
                return -EFAULT;
                
        if(in->sin_family != AF_INET)
                return -EINVAL; 
                
        in_addr_t addr = in->sin_addr.s_addr;
        in_port_t port = in->sin_port;
        
        struct netdev* netdev = nullptr;
        struct ipv4_sdata* priv_data = (struct ipv4_sdata*)socket->data;
                
        if(be16_to_native(priv_data->port) == 0)
        {
                priv_data->addr = INADDR_ANY;
		priv_data->port = udp_get_ephemeral_port();        

		if(priv_data->port)
			udp_table_insert(socket, priv_data->addr, priv_data->port);
		else
			return -EADDRINUSE;	
	}       
                
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

        struct udp_packet* packet = kmalloc(sizeof(struct udp_packet) + len);
        packet->src_port = priv_data->port;
        packet->dst_port = port;
        packet->length = native_to_be16(sizeof(struct udp_packet) + len);
        packet->checksum = 0;
        memcpy((byte*)packet + sizeof(struct udp_packet), buf, len);

        ssize_t sent = ipv4_tx_packet(netdev, route_addr, addr, IPPROTO_UDP, (const byte*)packet, len + sizeof(struct udp_packet));
        kfree(packet);
        return sent;
}

static struct socket_ops udp_ops =
{
	.release = udp_release,
	.bind = udp_bind,
	.sendto = udp_sendto
};

void udp_rx_packet(struct netdev* netdev, const struct ipv4_packet* packet, size_t length)
{
	if(length < sizeof(struct ipv4_packet) + sizeof(struct udp_packet))
		return;

	const struct udp_packet* udp_data = (const struct udp_packet*)((byte*)packet + sizeof(struct ipv4_packet));
	struct socket* socket = udp_table_get(packet->dst_addr, udp_data->dst_port); 
	if(!socket)
		socket = udp_table_get(0, udp_data->dst_port);

	if(!socket)
		return;

	struct sockaddr_in source;
	source.sin_family = AF_INET;
	source.sin_port = udp_data->src_port;
	source.sin_addr.s_addr = packet->src_addr;

	socket_rx_packet(socket, (const struct sockaddr*)&source, sizeof(struct sockaddr_in), (byte*)udp_data + sizeof(struct udp_packet), length - sizeof(struct ipv4_packet) - sizeof(struct udp_packet));	
}

void udp_create_socket(struct socket* socket, struct ipv4_sdata* priv_data)
{
	socket->ops = &udp_ops;
}

void ipv4_init_udp()
{
	mutex_init(&udp_table_lock);
}
