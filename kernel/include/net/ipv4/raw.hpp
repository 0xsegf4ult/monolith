#pragma once

#include <net/ipv4/ipv4.hpp>

bool ipv4_raw_rx_packet(netdev_t* netdev, const ipv4_packet* packet, size_t length);
void ipv4_raw_create_socket(socket_t* socket, ipv4_sdata* priv_data);
void ipv4_init_raw();
