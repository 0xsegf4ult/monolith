#pragma once

#include <libk/list.h>
#include <types.h>

typedef uint32_t hwirq_t;

typedef struct irq_domain irq_domain_t;

typedef void (*irq_handler_t)(void*);

typedef struct virt_irq
{
	uint32_t id;
	hwirq_t hwirq;
	irq_domain_t* domain;
	void* payload;
	irq_handler_t handler;
} virq_t;

typedef struct irq_chip
{
	const char* name;
	void (*enable)(virq_t* irq);
	void (*disable)(virq_t* irq);
	void (*eoi)(virq_t* irq);
} irq_chip_t;

typedef struct irq_domain
{
	const char* name;
	irq_chip_t* chip;
	hwirq_t begin;
	hwirq_t end;
	virtaddr_t msi_address;
	list_node_t list_node;
} irq_domain_t;

void irq_domain_register(irq_domain_t* domain);
irq_domain_t* msix_domain_register(const char* name, hwirq_t num_irqs, virtaddr_t msi_address);
virq_t* irq_register(hwirq_t hwirq, irq_handler_t handler, void* payload);
virq_t* irq_register_msi(irq_domain_t* msi_domain, hwirq_t hwirq, irq_handler_t handler, void* payload);
void irq_dispatch(hwirq_t hwirq);
