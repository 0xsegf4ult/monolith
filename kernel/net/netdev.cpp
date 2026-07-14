#include <net/netdev.hpp>
#include <mm/slab.hpp>
#include <sys/mutex.hpp>
#include <kstd.hpp>
#include <list.hpp>

struct netdev_list_t
{
	netdev_t* head;
	netdev_t* tail;
	mutex_t lock;
};

static list_head_t netdev_list;
static mutex_t netdev_list_lock;

netdev_t* netdev_lookup(const char* name)
{
	netdev_t* query = nullptr;

	mutex_lock(netdev_list_lock);
	netdev_t* cur;
	list_for_each_entry(cur, netdev_list, list_node)
	{
		if(strncmp(cur->name, name, 24) == 0)
		{
			query = cur;
			break;
		}
	}
	mutex_unlock(netdev_list_lock);
	return query;
}

netdev_t* netdev_create()
{
	auto* dev = (netdev_t*)kmalloc(sizeof(netdev_t));
	dev->ip_addr = 0;
	dev->ip_subnet = 0;
	dev->ip_broadcast = 0xFFFFFFFF;
	dev->ip_tx_id = 0;
	dev->data = nullptr;
	dev->ops = nullptr;
	list_node_init(dev->list_node);

	mutex_lock(netdev_list_lock);
	list_add_tail(netdev_list, dev->list_node);
	mutex_unlock(netdev_list_lock);

	return dev;
}

void netdev_init()
{
	list_node_init(netdev_list);
	mutex_init(netdev_list_lock);
}
