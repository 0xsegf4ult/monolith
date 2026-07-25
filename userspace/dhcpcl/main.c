#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <sys/mman.h>

#define DHCP_OPCODE_REQUEST 1
#define DHCP_OPCODE_REPLY 2

#define DHCP_HTYPE_ETHER 1

#define DHCP_MAGIC 0x63825363

#define DHCP_OPT_PADDING	0
#define DHCP_OPT_SUBNET_MASK	1
#define DHCP_OPT_ROUTER		3
#define DHCP_OPT_DNS		6
#define DHCP_OPT_REQUESTED_IP	50
#define DHCP_OPT_LEASE_TIME	51
#define DHCP_OPT_MESSAGE_TYPE	53
#define DHCP_OPT_SERVER_ID	54
#define DHCP_OPT_PARAMETER_REQ	55
#define DHCP_OPT_END		255

#define DHCP_DISCOVER	1
#define DHCP_OFFER	2
#define DHCP_REQUEST	3
#define DHCP_DECLINE	4
#define DHCP_ACK	5
#define DHCP_NAK	6
#define DHCP_RELEASE	7

typedef struct __attribute__((packed)) dhcp_packet
{
	uint8_t opcode;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t chaddr[6];
	uint8_t reserved[10];
	char server_name[64];
	char boot_file_name[128];
	uint32_t magic;
	uint8_t options[256];
} dhcp_packet_t;

typedef struct __attribute__((packed)) dhcp_options
{
	in_addr_t subnet_mask;
	in_addr_t server_addr;
	in_addr_t* dns_start;
	in_addr_t* dns_end;
	in_addr_t* router_start;
	in_addr_t* router_end;
	uint32_t lease_time;
	uint8_t msg_type;
} dhcp_options_t;

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("dhcpcl: No interface specified\n");
		return 1;
	}

	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(fd < 0)
	{
		printf("dhcpcl: failed to open socket: %s\n", strerror(errno));
		return 1;
	}

	struct sockaddr_in in;
	in.sin_family = AF_INET;
	in.sin_port = htons(68);
	in.sin_addr.s_addr = INADDR_ANY;

	int bind_s = bind(fd, (const struct sockaddr*)&in, sizeof(struct sockaddr_in));
	if(bind_s < 0)
	{
		printf("dhcpcl: failed to bind to address: %s\n", strerror(errno));
		return 1;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(67);
	server.sin_addr.s_addr = INADDR_BROADCAST;
	
	#define SIO_BINDDEVICE 1
	int bd_s = ioctl(fd, SIO_BINDDEVICE, argv[1]);
	if(bd_s < 0)
	{
		printf("dhcpcl: failed to bind to %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	#define SIO_GETHWADDR 2
	uint8_t hwaddr[6];
	int hw_s = ioctl(fd, SIO_GETHWADDR, &hwaddr[0]);
	if(hw_s < 0)
	{
		printf("dhcpcl: failed to read %s hwaddr: %s\n", argv[1], strerror(errno));
		return 1;
	}

	printf("DHCP_DISCOVER interface %s HWADDR %x:%x:%x:%x:%x:%x\n", argv[1], hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);	

	dhcp_packet_t d_packet;
	d_packet.opcode = DHCP_OPCODE_REQUEST;
	d_packet.htype = DHCP_HTYPE_ETHER;
	d_packet.hlen = 6;
	d_packet.hops = 0;
	d_packet.xid = 0xDEADBEEF;
	d_packet.secs = 0;
	d_packet.flags = 0;
	memcpy(d_packet.chaddr, hwaddr, 6);
	d_packet.magic = htonl(DHCP_MAGIC);

	size_t opt_idx = 0;
	d_packet.options[opt_idx++] = DHCP_OPT_MESSAGE_TYPE;
	d_packet.options[opt_idx++] = 1;
	d_packet.options[opt_idx++] = DHCP_DISCOVER;
	d_packet.options[opt_idx++] = DHCP_OPT_PARAMETER_REQ;
	d_packet.options[opt_idx++] = 3;
	d_packet.options[opt_idx++] = DHCP_OPT_DNS;
	d_packet.options[opt_idx++] = DHCP_OPT_SUBNET_MASK;
	d_packet.options[opt_idx++] = DHCP_OPT_ROUTER;
	d_packet.options[opt_idx++] = DHCP_OPT_END;

	void* recv_buf = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	ssize_t sd_s = sendto(fd, &d_packet, sizeof(dhcp_packet_t), 0, (const struct sockaddr*)&server, sizeof(struct sockaddr_in));
	if(sd_s < 0)
	{
		printf("dhcpcl: failed to send DHCP_DISCOVER: %s\n", strerror(errno));
		return 1;
	}

	struct sockaddr_in from;
	socklen_t from_len = sizeof(struct sockaddr_in);
	ssize_t ofr_s = recvfrom(fd, recv_buf, 0x1000, 0, (struct sockaddr*)&from, &from_len); 
	if(ofr_s < 0)
	{
		printf("dhcpcl: failed to receive DHCP_OFFER: %s\n", strerror(errno));
		return 1;
	}

	dhcp_packet_t* o_packet = (dhcp_packet_t*)recv_buf;
	if(ofr_s < sizeof(dhcp_packet_t) - 256)
	{
		printf("DHCP packet too small\n");
		return 1;
	}	

	if(o_packet->magic != htonl(DHCP_MAGIC))
	{
		printf("DHCP packet magic invalid\n");
		return 1;
	}

	if(o_packet->xid != d_packet.xid)
	{
		printf("DHCP XID mismatch\n");
	}

	dhcp_options_t options;
	size_t optidx = 0;
	while(optidx < 256)
	{
		if(o_packet->options[optidx] == DHCP_OPT_PADDING) continue;
		if(o_packet->options[optidx] == DHCP_OPT_END) break;

		switch(o_packet->options[optidx])
		{
		case DHCP_OPT_MESSAGE_TYPE:
			options.msg_type = o_packet->options[optidx + 2];
			break;
		case DHCP_OPT_SUBNET_MASK:
			options.subnet_mask = *(uint32_t*)(&o_packet->options[optidx + 2]);
			break;
		case DHCP_OPT_ROUTER:
			options.router_start = (in_addr_t*)(&o_packet->options[optidx + 2]);
			options.router_end = (in_addr_t*)(&o_packet->options[optidx + 2] + o_packet->options[optidx + 1]);
			break;
		case DHCP_OPT_SERVER_ID:
			options.server_addr = *(in_addr_t*)(&o_packet->options[optidx + 2]);
			break;
		case DHCP_OPT_LEASE_TIME:
			options.lease_time = *(uint32_t*)(&o_packet->options[optidx + 2]);
			break;
		case DHCP_OPT_DNS:
			options.dns_start = (in_addr_t*)(&o_packet->options[optidx + 2]);
			options.dns_end = (in_addr_t*)(&o_packet->options[optidx + 2] + o_packet->options[optidx + 1]);
			break;
		}

		optidx += (2 + o_packet->options[optidx + 1]);
	}

	if(options.msg_type != DHCP_OFFER)
	{
		printf("unexpected DHCP message!\n");
		return 1;
	}

	char strbuf[INET_ADDRSTRLEN];
	struct in_addr i;
	i.s_addr = options.server_addr;
	inet_ntop(AF_INET, &i, strbuf, INET_ADDRSTRLEN);
	printf("DHCP_OFFER from server %s\n", strbuf);
	i.s_addr = o_packet->yiaddr;
	inet_ntop(AF_INET, &i, strbuf, INET_ADDRSTRLEN);
	printf("DHCP_OFFER IP %s\n", strbuf);
	i.s_addr = options.subnet_mask;
	inet_ntop(AF_INET, &i, strbuf, INET_ADDRSTRLEN);
	printf("DHCP_OFFER SUBNET %s\n", strbuf);
	i.s_addr = *options.router_start;
	inet_ntop(AF_INET, &i, strbuf, INET_ADDRSTRLEN);
	printf("DHCP_OFFER GATEWAY %s\n", strbuf);

	in_addr_t req_address = o_packet->yiaddr;
	in_addr_t req_mask = options.subnet_mask;
	in_addr_t req_gateway = *options.router_start;

	dhcp_packet_t r_packet;
	r_packet.opcode = DHCP_OPCODE_REQUEST;
	r_packet.htype = DHCP_HTYPE_ETHER;
	r_packet.hlen = 6;
	r_packet.hops = 0;
	r_packet.xid = 0xDEADBEEF;
	r_packet.secs = 0;
	r_packet.flags = 0;
	memcpy(r_packet.chaddr, hwaddr, 6);
	r_packet.magic = htonl(DHCP_MAGIC);

	optidx = 0;
	r_packet.options[optidx++] = DHCP_OPT_MESSAGE_TYPE;
	r_packet.options[optidx++] = 1;
	r_packet.options[optidx++] = DHCP_REQUEST;
	r_packet.options[optidx++] = DHCP_OPT_REQUESTED_IP;
	r_packet.options[optidx++] = 4;
	*(in_addr_t*)(&r_packet.options[optidx]) = o_packet->yiaddr;
	optidx += 4;
	d_packet.options[optidx++] = DHCP_OPT_END;

	printf("DHCP_REQUEST\n");
	sd_s = sendto(fd, &r_packet, sizeof(dhcp_packet_t), 0, (const struct sockaddr*)&server, sizeof(struct sockaddr_in));
	if(sd_s < 0)
	{
		printf("dhcpcl: failed to send DHCP_REQUEST: %s\n", strerror(errno));
		return 1;
	}

	ofr_s = recvfrom(fd, recv_buf, 0x1000, 0, (struct sockaddr*)&from, &from_len);
	if(ofr_s < 0)
	{
		printf("dhcpcl: failed to receive DHCP_ACK: %s\n", strerror(errno));
		return 1;
	}
	
	dhcp_packet_t* a_packet = (dhcp_packet_t*)recv_buf;
	if(ofr_s < sizeof(dhcp_packet_t) - 256)
	{
		printf("DHCP packet too small\n");
		return 1;
	}	

	if(a_packet->magic != htonl(DHCP_MAGIC))
	{
		printf("DHCP packet magic invalid\n");
		return 1;
	}

	if(a_packet->xid != d_packet.xid)
	{
		printf("DHCP XID mismatch\n");
	}

	dhcp_options_t ack_options;
	optidx = 0;
	
	#define SIO_SETIFADDR 3
	#define SIO_SETIFMASK 4
	
	struct sockaddr_in in_ifa;
	in_ifa.sin_family = AF_INET;
	in_ifa.sin_addr.s_addr = req_address; 
	
	int io_s = ioctl(fd, SIO_SETIFADDR, &in_ifa);
	if(io_s < 0)
	{
		printf("dhcpcl: ioctl SIO_SETIFADDR failed: %s\n", strerror(errno));
		return 1;
	}

	in_ifa.sin_addr.s_addr = req_mask;
	io_s = ioctl(fd, SIO_SETIFMASK, &in_ifa);
	if(io_s < 0)
	{
		printf("dhcpcl: ioctl SIO_SETIFMASK failed: %s\n", strerror(errno));
		return 1;
	}

	struct rtentry route_gw = {};
	struct sockaddr_in* rdst = (struct sockaddr_in*)&route_gw.rt_dst;
	struct sockaddr_in* rgw = (struct sockaddr_in*)&route_gw.rt_gateway;
	struct sockaddr_in* rgm = (struct sockaddr_in*)&route_gw.rt_genmask;

	rdst->sin_family = AF_INET;
	rdst->sin_addr.s_addr = 0;
	rgw->sin_family = AF_INET;
	rgw->sin_addr.s_addr = req_gateway;
	rgm->sin_family = AF_INET;
	rgm->sin_addr.s_addr = 0;
	route_gw.rt_metric = 1000;
	route_gw.rt_mtu = 1500;

	#define SIO_ADDROUTE 5
	io_s = ioctl(fd, SIO_ADDROUTE, &route_gw);
	if(io_s < 0)
	{
		printf("dhcpcl: ioctl SIO_ADDROUTE failed: %s\n", strerror(errno));
		return errno;
	}

	struct rtentry route_local = {};
	rdst = (struct sockaddr_in*)&route_local.rt_dst;
	rgw = (struct sockaddr_in*)&route_local.rt_gateway;
	rgm = (struct sockaddr_in*)&route_local.rt_genmask;

	rdst->sin_family = AF_INET;
	rdst->sin_addr.s_addr = req_address & req_mask;
	rgw->sin_family = AF_INET;
	rgw->sin_addr.s_addr = 0;
	rgm->sin_family = AF_INET;
	rgm->sin_addr.s_addr = req_mask;
	route_local.rt_metric = 1000;
	route_local.rt_mtu = 1500;

	io_s = ioctl(fd, SIO_ADDROUTE, &route_local);
	if(io_s < 0)
	{
		printf("dhcpcl: ioctl SIO_ADDROUTE failed: %s\n", strerror(errno));
		return 1;
	}

	close(fd);
	return 0;
}
