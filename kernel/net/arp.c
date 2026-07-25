#include <net/arp.h>
#include <net/netdev.h>
#include <net/ether.h>
#include <mm/slab.h>
#include <sched/task.h>
#include <sched/scheduler.h>
#include <sched/signal.h>
#include <sched/waitqueue.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <libk/string.h>
#include <cpu.h>
#include <errno.h>
#include <types.h>

struct arp_waittable_entry
{
	uint32_t address;
	wait_queue queue;
	struct arp_waittable_entry* next;
};

static struct arp_entry* arp_table[32] = {0};
static struct arp_waittable_entry* arp_waittable[32] = {0};
static mutex_t lock;

struct arp_entry* arp_get_entry(uint32_t address)
{
	uint32_t hash = address % 32;

	mutex_lock(&lock);
	struct arp_entry* entry = arp_table[hash];
	while(entry)
	{
		if(entry->address == address)
			break;

		entry = entry->next;
	}
	mutex_unlock(&lock);

	return entry;
}

void arp_add_entry(uint32_t address, uint8_t* mac, uint16_t htype, struct netdev* netdev)
{
	struct arp_entry* entry = kmalloc(sizeof(struct arp_entry));
	entry->address = address;
	entry->htype = htype;
	memcpy(&entry->mac, mac, 6);
	entry->netdev = netdev;
	entry->next = nullptr;

	uint32_t hash = address % 32;

	mutex_lock(&lock);
	entry->next = arp_table[hash];
	arp_table[hash] = entry;

	struct arp_waittable_entry* wait_entry = arp_waittable[hash];
	while(wait_entry)
	{
		if(wait_entry->address == address)
		{
			wait_queue_wake(&wait_entry->queue);

			arp_waittable[hash] = wait_entry->next;
			kfree(wait_entry);
			break;
		}
		wait_entry = wait_entry->next;
	}
	
	mutex_unlock(&lock);
}

void arp_send_reply(struct netdev* netdev, struct arp_packet* packet)
{
	struct arp_packet reply;
	reply.htype = native_to_be16(arp_htype_ethernet);
	reply.ptype = native_to_be16(arp_ptype_ipv4);
	reply.hlen = 6;
	reply.plen = 4;
	reply.opcode = native_to_be16(ARP_OPCODE_REPLY);
	
	memcpy(&reply.src_hw, netdev->mac_addr, 6);
	reply.src_proto = netdev->ip_addr;
	memcpy(&reply.dst_hw, packet->src_hw, 6);
	reply.dst_proto = packet->src_proto; 

	ether_tx_packet(netdev, reply.dst_hw, ETHER_TYPE_ARP, (byte*)&reply, sizeof(struct arp_packet)); 
}

void arp_send_request(struct netdev* netdev, uint32_t address)
{
	struct arp_packet request;
	request.htype = native_to_be16(arp_htype_ethernet);
	request.ptype = native_to_be16(arp_ptype_ipv4);
	request.hlen = 6;
	request.plen = 4;
	request.opcode = native_to_be16(ARP_OPCODE_REQUEST);

	memcpy(&request.src_hw, netdev->mac_addr, 6);
	request.src_proto = netdev->ip_addr;
	request.dst_proto = address;

	ether_tx_packet(netdev, ether_mac_broadcast, ETHER_TYPE_ARP, (byte*)&request, sizeof(struct arp_packet));
}	

struct arp_entry* arp_search(struct netdev* netdev, uint32_t address)
{
	struct arp_entry* lookup = arp_get_entry(address);
	if(lookup)
		return lookup;

	arp_send_request(netdev, address);

	uint32_t hash = address % 32;
	struct task* task = smp_current_task();
	mutex_lock(&lock);
	
	struct arp_waittable_entry* wait_entry = arp_waittable[hash];
	while(wait_entry)
	{
		if(wait_entry->address == address)
			break;

		wait_entry = wait_entry->next;
	}

	if(!wait_entry)
	{
		wait_entry = kmalloc(sizeof(struct arp_waittable_entry));
		wait_entry->address = address;
		wait_queue_init(&wait_entry->queue);
		wait_entry->next = arp_waittable[hash];
		arp_waittable[hash] = wait_entry;
	}

	int wret = 0;

	task_status exp_status = TASK_RUNNING;
	atomic_compare_exchange_strong(&task->status, &exp_status, TASK_INTR_SLEEPING);
	wait_queue_register(&wait_entry->queue, &task->wait);

	mutex_unlock(&lock);
	sched_yield();

	wait_queue_unregister(&task->wait);
	exp_status = TASK_INTR_SLEEPING;
	atomic_compare_exchange_strong(&task->status, &exp_status, TASK_RUNNING);

	if(signal_pending(task))
		return (struct arp_entry*)-EINTR;

	return arp_get_entry(address);
}

void arp_rx_packet(struct netdev* netdev, struct arp_packet* packet, size_t length)
{
	if(be16_to_native(packet->htype) != arp_htype_ethernet)
		return;

	if(be16_to_native(packet->ptype) != arp_ptype_ipv4)
		return;

	uint16_t op = be16_to_native(packet->opcode);
	switch(op)
	{
	case ARP_OPCODE_REQUEST:
	{
		uint32_t src_ip = packet->src_proto;
		uint32_t dst_ip = packet->dst_proto;

		if(!arp_get_entry(src_ip))
			arp_add_entry(src_ip, packet->src_hw, arp_htype_ethernet, netdev);

		if(netdev->ip_addr && netdev->ip_addr == dst_ip)
			arp_send_reply(netdev, packet);

		break;
	}
	case ARP_OPCODE_REPLY:
		arp_add_entry(packet->src_proto, packet->src_hw, arp_htype_ethernet, netdev);
		break;
	default:
		break;
	}
}

void arp_init()
{
	mutex_init(&lock);
}
