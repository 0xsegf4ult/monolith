#include <sys/irq.h>
#include <mm/slab.h>
#include <libk/list.h>
#include <klog.h>
#include <types.h>
#include <stdatomic.h>
#include <lapic.h>

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

virq_t* irq_register(hwirq_t hwirq, irq_handler_t handler, void* payload)
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
	virq->payload = payload;
	virq->handler = handler;

	hwirq_t offset = hwirq - domain->begin;
	//TODO: check if already mapped	

	klog("irq: registered hwirq %u on domain %s\n", hwirq, domain->name);
	domain->chip->enable(virq);
	return virq;
}

static virtaddr_t msi_target_address = 0xfee00000;
static void msix_enable(virq_t* virq)
{
	klog("enable virq %u -> hwirq %u on domain %s\n", virq->id, virq->hwirq, virq->domain->name);
	uint32_t* msi_table = (uint32_t*)(virq->domain->msi_address) + virq->hwirq;
	*(msi_table++) = (msi_target_address & ~0x3);
	*(msi_table++) = (msi_target_address >> 32);
	*(msi_table++) = (virq->id & 0xFF);
	*(msi_table++) = 0;
}

static void msix_eoi(virq_t* virq)
{
	lapic_eoi();
}

static irq_chip_t msix_chip =
{
	.name = "MSI-X",
	.enable = msix_enable,
	.disable = nullptr,
	.eoi = msix_eoi,
};

irq_domain_t* msix_domain_register(const char* name, hwirq_t num_irqs, virtaddr_t msi_address)
{
	irq_domain_t* domain = kmalloc(sizeof(irq_domain_t));
	domain->name = name;
	domain->chip = &msix_chip;
	domain->begin = 0;
	domain->end = num_irqs;
	domain->msi_address = msi_address;
	return domain;
}	

virq_t* irq_register_msi(irq_domain_t* domain, hwirq_t hwirq, irq_handler_t handler, void* payload)
{
	if(!domain->msi_address)
	{
		klog("irq_register_msi: invalid domain %s\n", domain->name);
		return nullptr;
	}

	if(hwirq >= domain->end)
	{
		klog("irq_register_msi: hwirq %u out of range for domain %s\n", hwirq, domain->name);
		return nullptr;
	}

	uint32_t virqid = atomic_fetch_add_explicit(&virq_alloc, 1, memory_order_relaxed);
	virq_t* virq = &virq_table[virqid - 32];
	virq->id = virqid;
	virq->hwirq = hwirq;
	virq->domain = domain;
	virq->payload = payload;
	virq->handler = handler;

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
		virq->handler(virq->payload);

	if(virq->domain->chip->eoi)
		virq->domain->chip->eoi(virq);
}
