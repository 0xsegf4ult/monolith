#pragma once

#include <net/ipv4/ipv4.hpp>
#include <types.hpp>

struct __attribute__((packed)) udp_packet
{
        in_port_t src_port;
        in_port_t dst_port;
        uint16_t length;
        uint16_t checksum;
};

void udp_rx_packet(netdev_t* netdev, const ipv4_packet* packet, size_t length);
void udp_create_socket(socket_t* socket, ipv4_sdata* priv_data);
void ipv4_init_udp();
