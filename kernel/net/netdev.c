#include <net/netdev.h>
#include <mm/slab.h>
#include <sys/mutex.h>
#include <libk/list.h>
#include <libk/string.h>
#include <types.h>

static list_head_t netdev_list;
static mutex_t netdev_list_lock;

struct netdev* netdev_lookup(const char* name)
{
	struct netdev* query = nullptr;

	mutex_lock(&netdev_list_lock);
	struct netdev* cur;
	list_for_each_entry(cur, &netdev_list, list_node)
	{
		if(strncmp(cur->name, name, 24) == 0)
		{
			query = cur;
			break;
		}
	}
	mutex_unlock(&netdev_list_lock);
	return query;
}

struct netdev* netdev_create()
{
	struct netdev* dev = kmalloc(sizeof(struct netdev));
	dev->ip_addr = 0;
	dev->ip_subnet = 0;
	dev->ip_broadcast = 0xFFFFFFFF;
	dev->ip_tx_id = 0;
	dev->data = nullptr;
	dev->ops = nullptr;
	list_node_init(&dev->list_node);

	mutex_lock(&netdev_list_lock);
	list_add_tail(&netdev_list, &dev->list_node);
	mutex_unlock(&netdev_list_lock);

	return dev;
}

void netdev_init()
{
	list_node_init(&netdev_list);
	mutex_init(&netdev_list_lock);
}
