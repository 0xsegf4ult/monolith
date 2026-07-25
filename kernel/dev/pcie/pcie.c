#include <dev/pcie/pcie.h>
#include <dev/pcie/pci_class.h>
#include <dev/nvme/nvme.h>
#include <dev/net/e1000/e1000.h>
#include <mm/mmu.h>
#include <mm/vmm.h>
#include <sys/irq.h>
#include <acpi.h>
#include <types.h>
#include <klog.h>

static virtaddr_t base_address = 0;

static virtaddr_t pcie_device_address(struct pcie_device* dev)
{
	return base_address + (((dev->bus * 256) + (dev->device * 8) + dev->function) * 4096);
}

void pcie_write64(struct pcie_device* dev, uint32_t offset, uint64_t data)
{
	*(volatile uint64_t*)(pcie_device_address(dev) + offset) = data;
}

void pcie_write32(struct pcie_device* dev, uint32_t offset, uint32_t data)
{
	*(volatile uint32_t*)(pcie_device_address(dev) + offset) = data;
}

uint64_t pcie_read64(struct pcie_device* dev, uint32_t offset)
{
	return *(const volatile uint64_t*)(pcie_device_address(dev) + offset);
}

uint32_t pcie_read32(struct pcie_device* dev, uint32_t offset)
{
	return *(const volatile uint32_t*)(pcie_device_address(dev) + offset);
}

uint16_t pcie_read16(struct pcie_device* dev, uint32_t offset)
{
	uint32_t reg32 = pcie_read32(dev, align_down(offset, 0x4));
	reg32 >>= ((offset & 0x2) * 8);
	return (uint16_t)(reg32 & 0xFFFF);
}

uint8_t pcie_read8(struct pcie_device* dev, uint32_t offset)
{
	uint32_t reg32 = pcie_read32(dev, align_down(offset, 0x4));
	reg32 >>= ((offset & 0x3) * 8);
	return (uint8_t)(reg32 & 0xFF);
}

bool pcie_is_valid(struct pcie_device* dev)
{
	return pcie_read16(dev, 0) != 0xFFFF && pcie_read16(dev, 0x2) != 0xFFFF;
}

bool pcie_is_bridge(struct pcie_device* dev)
{
	return pcie_read8(dev, 0xe) == 0x1 && pcie_read8(dev, 0xb) == 0x6;
}

uint8_t pcie_sub_bus(struct pcie_device* dev)
{
	return pcie_read8(dev, 0xe) & (1 << 7);
}

struct pcie_bar pcie_get_bar(struct pcie_device* device, uint8_t bir)
{
	struct pcie_bar ret = {};

	uint32_t bar_low = pcie_read32(device, 0x10 + bir * 4);
	pcie_write32(device, 0x10 + bir * 4, 0xFFFFFFFF);
	uint32_t bar_size = pcie_read32(device, 0x10 + bir * 4) & 0xFFFFFFFF0;
	pcie_write32(device, 0x10 + bir * 4, bar_low & 0xFFFFFFF0);
	ret.size = (~bar_size) + 1;

	if(bar_low & 0x4 && !(bar_low & 0x2))
	{
		uint32_t bar_high = pcie_read32(device, 0x10 + bir * 4 + 4);
		ret.phys_base = (((uint64_t)bar_high & 0xFFFFFFFF) << 32) | (bar_low & 0xFFFFFFF0);
	}
	else if(!(bar_low & 0x2) && !(bar_low & 0x1))
	{
		ret.phys_base = bar_low & 0xFFFFFFF0;
	}
	else
	{
		ret.address = 0;
		return ret;
	}
	
	ret.address = vm_space_map(vm_get_kernel_space(), 
	(vm_mapping_info)
	{
		.length = ret.size,
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_DEVICE,
		.phys_base = ret.phys_base
	});

	return ret;
}

bool pcie_enable_msix(struct pcie_device* device)
{
	uint8_t pcap_ptr = pcie_read8(device, 0x34) & 0xFC;
	uint8_t msix_ptr = 0;

	while(pcap_ptr)
	{
		uint32_t cap = pcie_read32(device, pcap_ptr);
		if((cap & 0xFF) == 0x11)
		{
			msix_ptr = pcap_ptr;
			break;
		}

		pcap_ptr = (cap >> 8) & 0xFC;
	}

	if(!msix_ptr)
		return false;

	uint32_t msix_reg0 = pcie_read32(device, msix_ptr);
	uint32_t msix_reg1 = pcie_read32(device, msix_ptr + 4);

	uint16_t msix_mctr = msix_reg0 >> 16;
	uint8_t bir = msix_reg1 & 0b111;
	
	uint32_t table_offset = (msix_reg1 & ~(0b111));
	uint32_t table_size = msix_mctr & 0b11111111111;
	klog("pcie: %x:%x.%d MSI-X using BIR%d at %p size %p\n", device->bus, device->device, device->function, bir, table_offset, table_size);

	virtaddr_t table = pcie_get_bar(device, bir).address + table_offset;
	if(!table)
		return false;

	msix_reg0 &= ~(1 << 30);
	msix_reg0 |= (1 << 31);
	pcie_write32(device, msix_ptr, msix_reg0);

	device->msi_domain = msix_domain_register("pcie_msix", table_size + 1, table);

	return true;
}

static void pcie_dispatch_driver(struct pcie_device* device)
{
	uint8_t classid = pcie_read8(device, 0xb);
	uint8_t sclassid = pcie_read8(device, 0xa);
	uint16_t vid = pcie_read16(device, 0);
	uint16_t devid = pcie_read16(device, 0x2);

	if(classid == 1 && sclassid == 8)
		nvme_controller_init(device);
	else if(classid == 2 && sclassid == 0)
	{
		if(vid == 0x8086 && devid == 0x10d3)
			e1000_init(device);
	}
}

static void scan_bus(uint8_t bus);

static void scan_device(uint8_t bus, uint8_t device)
{
	struct pcie_device dev =
	{
		.bus = bus,
		.device = device,
		.function = 0
	};

	if(!pcie_is_valid(&dev))
		return;

	const char* cname = class_to_string(pcie_read8(&dev, 0xb), pcie_read8(&dev, 0xa));

	if(pcie_is_bridge(&dev))
	{
		klog("pcie: %x:%x.0 %s\n", bus, device, cname != nullptr ? cname : "Bridge");
		scan_bus(pcie_sub_bus(&dev));
		return;
	}

	uint16_t vid = pcie_read16(&dev, 0);
	uint16_t devid = pcie_read16(&dev, 0x2);
	if(cname)
		klog("pcie: %x:%x.0 %s\n", bus, device, cname);
	else
		klog("pcie: %x:%x.0 %p:%p", bus, device, vid, devid);

	if(pcie_sub_bus(&dev))
	{
		for(uint8_t i = 1; i < 8; i++)
		{
			struct pcie_device subdev =
			{
				.bus = bus,
				.device = device,
				.function = i,
			};

			if(!pcie_is_valid(&subdev))
				continue;

			const char* s_cname = class_to_string(pcie_read8(&subdev, 0xb), pcie_read8(&subdev, 0xa));
			if(pcie_is_bridge(&subdev))
				scan_bus(pcie_sub_bus(&subdev));

			if(s_cname)
				klog("pcie: %x:%x.%x %s\n", bus, device, i, s_cname);
			else
				klog("pcie: %x:%x.%x %p:%p\n", bus, device, i, pcie_read16(&subdev, 0), pcie_read16(&subdev, 0x2));

			pcie_dispatch_driver(&subdev);
		}
	}

	pcie_dispatch_driver(&dev);
}

static void scan_bus(uint8_t bus)
{
	for(uint8_t dev = 0; dev < 32; dev++)
		scan_device(bus, dev);
}

void pcie_init()
{
	const mcfg* mcfg = acpi_get_tables()->mcfg;
	const byte* raw = (const byte*)mcfg;
	raw += sizeof(mcfg);

	size_t len = mcfg->header.length - sizeof(mcfg);
	size_t ecam_count = len / sizeof(mcfg_ecam);

	if(ecam_count > 1)
		klog("pcie: MCFG has multiple ECAM entries\n");

	const mcfg_ecam* ecam = (const mcfg_ecam*)raw;
	klog("pcie: ECAM base %p segment %u bus %u-%u\n", ecam->base_address, ecam->segment_group, ecam->start_bus, ecam->end_bus);

	physaddr_t base = ecam->base_address;
	base_address = base + VM_DMAP_BASE;
	mmu_map_range(vm_get_kernel_space()->mmu_root, base, base_address, 256 * 8 * 8 * 4096, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);

	scan_bus(0);
}
