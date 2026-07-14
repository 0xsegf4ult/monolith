#include <net/ipv4/route.hpp>
#include <net/ipv4/ipv4.hpp>
#include <sys/mutex.hpp>

struct ip_route_table_t
{
	list_head_t list;
	mutex_t lock;
};

static ip_route_table_t ip_route_table;

void ip_route_add(ip_route_entry* entry)
{
	list_node_init(entry->list_node);

        mutex_lock(ip_route_table.lock);
        list_add_tail(ip_route_table.list, entry->list_node);
        mutex_unlock(ip_route_table.lock);
}

ip_route_entry* ip_route_get(in_addr_t ip)
{
        ip_route_entry* best = nullptr;
        uint16_t best_metric = 0;
        
	mutex_lock(ip_route_table.lock);

	ip_route_entry* cur;
        list_for_each_entry(cur, ip_route_table.list, list_node)
	{
                if((ip & cur->mask) == (cur->dest & cur->mask))
                {
                        if(cur->metric > best_metric)
                        {
                                best_metric = cur->metric;
                                best = cur;
                        }
                }
        }

        mutex_unlock(ip_route_table.lock);
        return best;
}

void ipv4_init_route()
{
        list_node_init(ip_route_table.list);
	mutex_init(ip_route_table.lock);
}
