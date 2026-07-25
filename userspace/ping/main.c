#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <time.h>
#include <arpa/inet.h>

typedef struct icmp_header
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t id;
	uint16_t seq;
	uint8_t payload[56];
} icmp_header_t;

static uint16_t icmp_checksum(char* payload, size_t len)
{
	uint16_t* p = (uint16_t*)payload;
	uint32_t checksum = 0;

	for(size_t i = 0; i < len / 2; i++)
		checksum += ntohs(p[i]);

	if(checksum > 0xFFFF)
		checksum = (checksum >> 16) + (checksum & 0xFFFF);

	return ~(checksum & 0xFFFF) & 0xFFFF;
}

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("ping: Destination address required\n");
		return 1;
	}

	int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if(fd < 0)
	{
		printf("ping: socket: %s\n", strerror(errno));
		return 1;
	}

	struct sockaddr dest_addr;
	struct sockaddr_in* in = (struct sockaddr_in*)&dest_addr;
	in->sin_family = AF_INET;

	int parsed = inet_pton(AF_INET, argv[1], &in->sin_addr);
	if(parsed < 0)
	{
		printf("ping: inet_pton: %s\n", strerror(errno));
		return 1;
	}
	else if(parsed == 0)
	{
		printf("ping: Invalid destination address\n");
		return 1;
	}

	char strbuf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &in->sin_addr, strbuf, INET_ADDRSTRLEN);

	icmp_header_t packet;
	packet.type = 8;
	packet.code = 0;
	packet.id = 0;
	packet.seq = 0;
	for(int i = 0; i < 56; i++)
		packet.payload[i] = i;

	uint16_t seq = 0;
	printf("PING %s (%s) 56 bytes of data.\n", argv[1], strbuf);

	void* recv_buf = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	while(1)
	{
		seq++;

		packet.seq = htons(seq);
		packet.checksum = 0;
		packet.checksum = htons(icmp_checksum((char*)&packet, 64));

		ssize_t s = sendto(fd, &packet, 64, 0, (const struct sockaddr*)&dest_addr, sizeof(struct sockaddr_in));
		if(s < 0)
		{
			printf("ping: sendto: %s\n", strerror(errno));
			return 1;
		}
		
		struct timespec tv;
		clock_gettime(CLOCK_REALTIME, &tv);
		uint64_t send_time = tv.tv_sec * 1000000000ULL + tv.tv_nsec;

		struct sockaddr_in from;
		socklen_t from_len = sizeof(struct sockaddr_in);
		ssize_t r = recvfrom(fd, recv_buf, 0x1000, 0, (struct sockaddr*)&from, &from_len);
		
		clock_gettime(CLOCK_REALTIME, &tv);
		uint64_t recv_time = tv.tv_sec * 1000000000ULL + tv.tv_nsec;
		
		if(r)
		{
			icmp_header_t* echo = (icmp_header_t*)recv_buf;

			if(echo->type == 0)
			{
				uint64_t t = (recv_time - send_time);
				inet_ntop(AF_INET, &from.sin_addr, strbuf, INET_ADDRSTRLEN);
				printf("%d bytes from %s: icmp_seq=%d time=%d.%dms\n", r, strbuf, ntohs(echo->seq), t / 1000000, t % 1000000);
			}
		}

		struct timespec sleep_tv;
		sleep_tv.tv_sec = 1;
		sleep_tv.tv_nsec = 0;
		clock_nanosleep(CLOCK_REALTIME, 0, &sleep_tv, nullptr);
	}

	close(fd);
	return 0;
}
