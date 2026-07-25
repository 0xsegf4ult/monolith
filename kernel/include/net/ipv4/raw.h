#pragma once

#include <net/ipv4/ipv4.h>

bool ipv4_raw_rx_packet(struct netdev* netdev, const struct ipv4_packet* packet, size_t length);
void ipv4_raw_create_socket(struct socket* socket, struct ipv4_sdata* priv_data);
void ipv4_init_raw();
