#include <net/ipv4/udp.hpp>
#include <net/ipv4/ipv4.hpp>
#include <net/ipv4/route.hpp>
#include <net/ether.hpp>
#include <net/netdev.hpp>
#include <net/socket.hpp>
#include <arch/x86_64/arch.hpp>
#include <mm/slab.hpp>
#include <sys/err.hpp>
#include <sys/mutex.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <types.hpp>

constexpr uint16_t ephemeral_port_start = 49152; 

struct udp_table_t
{
	socket_t* hash_table[256];
	mutex_t lock;
	atomic_ushort next_eph_port;
};

static udp_table_t* udp_table = nullptr;

constexpr uint32_t ip_port_hashf(in_addr_t addr, in_port_t port)
{
	uint32_t hash = 17;
	hash = hash * 5 + addr;
	hash = hash * 5 + port;
	return hash % 256;
}

static socket_t* udp_table_get(in_addr_t addr, in_port_t port)
{
	auto hash = ip_port_hashf(addr, port);
	log::debug("udp_hlookup {:#x}:{} = {:#x}", addr, port, hash);

	mutex_lock(udp_table->lock);
	socket_t* entry = udp_table->hash_table[hash];
	while(entry)
	{
		auto* priv_data = (ipv4_sdata*)entry->data;
		log::debug("udp_table_get hashcmp {:#x} == {:#x} && {} == {}", priv_data->addr, addr, priv_data->port, port);
		if(priv_data->addr == addr && priv_data->port == port)
			break;

		entry = entry->next;
	}
	mutex_unlock(udp_table->lock);
	return entry;
}

static void udp_table_insert(socket_t* socket, in_addr_t addr, in_port_t port)
{
	auto hash = ip_port_hashf(addr, port);
	log::debug("udp_hinsert: {:#x}:{} = {:#x}", addr, port, hash);

	mutex_lock(udp_table->lock);
	socket->next = udp_table->hash_table[hash];
	udp_table->hash_table[hash] = socket;
	mutex_unlock(udp_table->lock);
}

static void udp_table_remove(socket_t* socket, in_addr_t addr, in_port_t port)
{
	auto hash = ip_port_hashf(addr, port);

	mutex_lock(udp_table->lock);
	socket_t** cur = &udp_table->hash_table[hash];
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
	mutex_unlock(udp_table->lock);
}

static bool udp_register(socket_t* socket, ipv4_sdata* priv_data)
{
	auto* query = udp_table_get(priv_data->addr, priv_data->port);
	if(query)
		return false;

	log::debug("UDP: registered socket {} for {:#x}:{}", socket, be32_to_native(priv_data->addr), be16_to_native(priv_data->port));
	udp_table_insert(socket, priv_data->addr, priv_data->port);
	return true;
}

static uint16_t udp_get_ephemeral_port()
{
	uint16_t port = 0;

	for(;;)
	{
		auto next_port = atomic_fetch_add(&udp_table->next_eph_port, 1);
		auto nport = native_to_be16(next_port);
		auto query = udp_table_get(0, nport);
		
		if(!query)
		{
			port = nport;
			break;
		}
	}

	return port;
}

void udp_release(socket_t* socket)
{
	if(!socket->bound)
		return;

	auto* priv_data = (ipv4_sdata*)socket->data;
	if(!udp_table_get(priv_data->addr, priv_data->port))
	{
		log::debug("UDP: tried to release unregistered socket");
		return;
	}

	udp_table_remove(socket, priv_data->addr, priv_data->port);
	kfree(priv_data);
}

int udp_bind(socket_t* socket, const sockaddr* addr, socklen_t addrlen)
{       
        sockaddr_in* in = (sockaddr_in*)addr;
        if(addrlen < sizeof(sockaddr_in))
                return -EFAULT;

        if(in->sin_family != AF_INET)
                return -EINVAL;
        
        auto* priv_data = (ipv4_sdata*)socket->data;
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
       
ssize_t udp_sendto(socket_t* socket, const byte* buf, size_t len, int flags, const sockaddr* dest_addr, socklen_t addrlen)
{       
        if(!len)
                return 0;

        sockaddr_in* in = (sockaddr_in*)dest_addr;
                
        if(addrlen < sizeof(sockaddr_in))
                return -EFAULT;
                
        if(in->sin_family != AF_INET)
                return -EINVAL; 
                
        auto addr = in->sin_addr.s_addr;
        auto port = in->sin_port;
        
        netdev_t* netdev = nullptr;
        auto* priv_data = (ipv4_sdata*)socket->data;
                
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
		auto* route = ip_route_get(addr);
		if(!route)
			return -ENETUNREACH;

		netdev = route->device;
		route_addr = route->gateway;
	}

        if(!netdev)
                return -ENETUNREACH;

        auto* packet = (udp_packet*)kmalloc(sizeof(udp_packet) + len);
        packet->src_port = priv_data->port;
        packet->dst_port = port;
        packet->length = native_to_be16(sizeof(udp_packet) + len);
        packet->checksum = 0;
        memcpy((byte*)packet + sizeof(udp_packet), buf, len);

        auto sent = ipv4_tx_packet(netdev, route_addr, addr, IPPROTO_UDP, (const byte*)packet, len + sizeof(udp_packet));
        kfree(packet);
        return sent;
}

static socket_ops udp_ops =
{
	.release = udp_release,
	.bind = udp_bind,
	.sendto = udp_sendto
};

void udp_rx_packet(netdev_t* netdev, const ipv4_packet* packet, size_t length)
{
	if(length < sizeof(ipv4_packet) + sizeof(udp_packet))
	{
		log::debug("UDP: RX packet size {:#x} < IP + UDP headers, dropping!", length);
		return;
	}

	auto* udp_data = (const udp_packet*)((byte*)packet + sizeof(ipv4_packet));
	auto* socket = udp_table_get(packet->dst_addr, udp_data->dst_port); 
	if(!socket)
		socket = udp_table_get(0, udp_data->dst_port);

	if(!socket)
	{
		log::debug("UDP: RX packet routed to addr {:#x} port {} but nobody is listening", be32_to_native(packet->dst_addr), be16_to_native(udp_data->dst_port)); 
		return;
	}

	sockaddr_in source;
	source.sin_family = AF_INET;
	source.sin_port = udp_data->src_port;
	source.sin_addr.s_addr = packet->src_addr;

	socket_rx_packet(socket, (const sockaddr*)&source, sizeof(sockaddr_in), (byte*)udp_data + sizeof(udp_packet), length - sizeof(ipv4_packet) - sizeof(udp_packet));	
}

void udp_create_socket(socket_t* socket, ipv4_sdata* priv_data)
{
	socket->ops = &udp_ops;
}

void ipv4_init_udp()
{
	udp_table = (udp_table_t*)kmalloc(sizeof(udp_table_t));
	memset(udp_table->hash_table, 0, sizeof(socket_t*) * 256);
	mutex_init(udp_table->lock);
	
	udp_table->next_eph_port = ephemeral_port_start;
}
