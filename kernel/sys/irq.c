#include <sys/irq.h>
#include <libk/list.h>
#include <klog.h>
#include <stdatomic.h>

static list_head_t irq_domain_list = {&irq_domain_list, &irq_domain_list};

//FIXME: directly wired to CPU vectors 
static virq_t virq_table[128];
static atomic_uint virq_alloc = 32;

static irq_domain_t* find_hwirq_domain(hwirq_t hwirq)
{
	irq_domain_t* cur;
	list_for_each_entry(cur, &irq_domain_list, list_node)
	{
		if(hwirq >= cur->begin && hwirq <= cur->end)
			return cur;
	}

	return nullptr;
}

void irq_domain_register(irq_domain_t* domain)
{
	list_node_init(&domain->list_node);

	if(domain->end <= domain->begin)
	{
		klog("irq_domain_register: invalid hwirq range for domain %s\n", domain->name); 
		return;
	}

	list_add_tail(&irq_domain_list, &domain->list_node);
}

virq_t* irq_register(hwirq_t hwirq, irq_handler_t handler)
{
	irq_domain_t* domain = find_hwirq_domain(hwirq);
	if(!domain)
	{
		klog("irq_allocate: could not find valid domain for hwirq %u\n", hwirq);
		return nullptr;
	}

	uint32_t virqid = atomic_fetch_add_explicit(&virq_alloc, 1, memory_order_relaxed);
	virq_t* virq = &virq_table[virqid - 32];
	virq->id = virqid;
	virq->hwirq = hwirq;
	virq->domain = domain;
	virq->handler = handler;

	hwirq_t offset = hwirq - domain->begin;
	//TODO: check if already mapped	

	klog("irq: registered hwirq %u on domain %s\n", hwirq, domain->name);
	domain->chip->enable(virq);
	return virq;
}

void irq_dispatch(uint32_t id)
{
	virq_t* virq = &virq_table[id - 32];
	if(!virq->domain)
	{
		klog("irq_dispatch: virq %u is not mapped to hardware!\n", id);
		return;
	}

	if(virq->handler)
		virq->handler();

	if(virq->domain->chip->eoi)
		virq->domain->chip->eoi(virq);
}
