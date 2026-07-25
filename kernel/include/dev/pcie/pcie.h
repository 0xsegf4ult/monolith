#pragma once

#include <types.h>

typedef struct irq_domain irq_domain_t;

struct pcie_device
{
	uint8_t bus;
	uint8_t device;
	uint8_t function;
	irq_domain_t* msi_domain;
};

struct pcie_bar
{
	physaddr_t phys_base;
	virtaddr_t address;
	size_t size;
};

void pcie_write64(struct pcie_device* dev, uint32_t offset, uint64_t data);
void pcie_write32(struct pcie_device* dev, uint32_t offset, uint32_t data);
uint64_t pcie_read64(struct pcie_device* dev, uint32_t offset);
uint32_t pcie_read32(struct pcie_device* dev, uint32_t offset);
uint16_t pcie_read16(struct pcie_device* dev, uint32_t offset);
uint8_t pcie_read8(struct pcie_device* dev, uint32_t offset);
bool pcie_is_valid(struct pcie_device* dev);
bool pcie_is_bridge(struct pcie_device* dev);
uint8_t pcie_sub_bus(struct pcie_device* dev);
struct pcie_bar pcie_get_bar(struct pcie_device* dev, uint8_t bir);
bool pcie_enable_msix(struct pcie_device* device);
void pcie_init();
