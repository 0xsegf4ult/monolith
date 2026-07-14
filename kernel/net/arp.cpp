#include <net/arp.hpp>
#include <net/netdev.hpp>
#include <net/ether.hpp>

#include <arch/generic.hpp>

#include <mm/slab.hpp>
#include <sys/err.hpp>
#include <sys/mutex.hpp>
#include <sys/signal.hpp>
#include <sys/scheduler.hpp>
#include <sys/smp.hpp>
#include <sys/task.hpp>
#include <sys/waitqueue.hpp>

#include <klog.hpp>
#include <kstd.hpp>
#include <types.hpp>

struct arp_waittable_entry
{
	uint32_t address;
	wait_queue queue;
	arp_waittable_entry* next;
};

struct arp_table_t
{
	mutex_t lock;
	arp_entry* hash_table[32];
	arp_waittable_entry* wait_table[32];
};

static arp_table_t* arp_table = nullptr;

arp_entry* arp_get_entry(uint32_t address)
{
	auto hash = address % 32;

	mutex_lock(arp_table->lock);
	arp_entry* entry = arp_table->hash_table[hash];
	while(entry)
	{
		if(entry->address == address)
			break;

		entry = entry->next;
	}
	mutex_unlock(arp_table->lock);

	return entry;
}

void arp_add_entry(uint32_t address, uint8_t* mac, uint16_t htype, netdev_t* netdev)
{
	auto* entry = (arp_entry*)kmalloc(sizeof(arp_entry));
	entry->address = address;
	entry->htype = htype;
	memcpy(&entry->mac, mac, 6);
	entry->netdev = netdev;
	entry->next = nullptr;

	auto hash = address % 32;

	mutex_lock(arp_table->lock);
	entry->next = arp_table->hash_table[hash];
	arp_table->hash_table[hash] = entry;

	auto* wait_entry = arp_table->wait_table[hash];
	while(wait_entry)
	{
		if(wait_entry->address == address)
		{
			wait_queue_wake(wait_entry->queue);

			arp_table->wait_table[hash] = wait_entry->next;
			kfree(wait_entry);
			break;
		}
		wait_entry = wait_entry->next;
	}
	
	mutex_unlock(arp_table->lock);
}

void arp_send_reply(netdev_t* netdev, arp_packet* packet)
{
	arp_packet reply;
	reply.htype = native_to_be16(arp_htype_ethernet);
	reply.ptype = native_to_be16(arp_ptype_ipv4);
	reply.hlen = 6;
	reply.plen = 4;
	reply.opcode = native_to_be16(ARP_OPCODE_REPLY);
	
	memcpy(&reply.src_hw, netdev->mac_addr, 6);
	reply.src_proto = netdev->ip_addr;
	memcpy(&reply.dst_hw, packet->src_hw, 6);
	reply.dst_proto = packet->src_proto; 

	ether_send_packet(netdev, reply.dst_hw, ETHER_TYPE_ARP, (byte*)&reply, sizeof(arp_packet)); 
}

void arp_send_request(netdev_t* netdev, uint32_t address)
{
	arp_packet request;
	request.htype = native_to_be16(arp_htype_ethernet);
	request.ptype = native_to_be16(arp_ptype_ipv4);
	request.hlen = 6;
	request.plen = 4;
	request.opcode = native_to_be16(ARP_OPCODE_REQUEST);

	memcpy(&request.src_hw, netdev->mac_addr, 6);
	request.src_proto = netdev->ip_addr;
	request.dst_proto = address;

	ether_send_packet(netdev, ether_mac_broadcast, ETHER_TYPE_ARP, (byte*)&request, sizeof(arp_packet));
}	

arp_entry* arp_search(netdev_t* netdev, uint32_t address)
{
	auto* lookup = arp_get_entry(address);
	if(lookup)
		return lookup;

	arp_send_request(netdev, address);

	auto hash = address % 32;
	auto* task = smp_current_task();
	mutex_lock(arp_table->lock);
	
	auto* wait_entry = arp_table->wait_table[hash];
	while(wait_entry)
	{
		if(wait_entry->address == address)
			break;

		wait_entry = wait_entry->next;
	}

	if(!wait_entry)
	{
		wait_entry = (arp_waittable_entry*)kmalloc(sizeof(arp_waittable_entry));
		wait_entry->address = address;
		wait_queue_init(wait_entry->queue);
		wait_entry->next = arp_table->wait_table[hash];
		arp_table->wait_table[hash] = wait_entry;
	}

	int wret = 0;

	task_status exp_status = TASK_RUNNING;
	atomic_compare_exchange_strong(&task->status, &exp_status, TASK_INTR_SLEEPING);
	wait_queue_register(wait_entry->queue, task->wait);

	mutex_unlock(arp_table->lock);
	sched_yield();

	wait_queue_unregister(task->wait);
	exp_status = TASK_INTR_SLEEPING;
	atomic_compare_exchange_strong(&task->status, &exp_status, TASK_RUNNING);

	if(signal_pending(task))
		return (arp_entry*)-EINTR;

	return arp_get_entry(address);
}

void arp_recv_packet(netdev_t* netdev, arp_packet* packet, size_t length)
{
	if(be16_to_native(packet->htype) != arp_htype_ethernet)
		return;

	if(be16_to_native(packet->ptype) != arp_ptype_ipv4)
		return;

	auto op = be16_to_native(packet->opcode);
	switch(op)
	{
	case ARP_OPCODE_REQUEST:
	{
		uint32_t src_ip = packet->src_proto;
		uint32_t dst_ip = packet->dst_proto;

		if(!arp_get_entry(src_ip))
		{
			log::debug("arp: mapping IP {:#x} to {:x}:{:x}:{:x}:{:x}:{:x}:{:x}", src_ip, packet->src_hw[0], packet->src_hw[1], packet->src_hw[2], packet->src_hw[3], packet->src_hw[4], packet->src_hw[5]);
			arp_add_entry(src_ip, packet->src_hw, arp_htype_ethernet, netdev);
		}

		if(netdev->ip_addr && netdev->ip_addr == dst_ip)
			arp_send_reply(netdev, packet);

		break;
	}
	case ARP_OPCODE_REPLY:
		log::debug("arp: mapping IP {:#x} to {:x}:{:x}:{:x}:{:x}:{:x}:{:x}", packet->src_proto, packet->src_hw[0], packet->src_hw[1], packet->src_hw[2], packet->src_hw[3], packet->src_hw[4], packet->src_hw[5]);
		arp_add_entry(packet->src_proto, packet->src_hw, arp_htype_ethernet, netdev);
		break;
	default:
		break;
	}
}

void arp_init()
{
	arp_table = (arp_table_t*)kmalloc(sizeof(arp_table_t));
	mutex_init(arp_table->lock);
	memset(arp_table->hash_table, 0, sizeof(arp_entry*) * 32);
	memset(arp_table->wait_table, 0, sizeof(arp_waittable_entry*) * 32);
}
