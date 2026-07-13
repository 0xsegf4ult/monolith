#include <dev/pcie.hpp>
#include <dev/pci_class.hpp>
#include <dev/nvme/nvme.hpp>
#include <dev/net/e1000/e1000.hpp>

#include <arch/x86_64/mmu.hpp>
#include <mm/vmm.hpp>
#include <types.hpp>
#include <klog.hpp>

namespace pcie
{

static virtaddr_t base_address = 0;

void set_base(physaddr_t base)
{
	base_address = base + VM_DMAP_BASE;
	mmu_map_range(vm_get_kernel_space()->mmu_root, base, base_address, 256 * 8 * 8 * 4096, PROT_READ | PROT_WRITE | PROT_UNCACHED, 0);
}

}

uint32_t pcie_device::read_config32(uint32_t offset) const
{
	return *reinterpret_cast<const volatile uint32_t*>(pcie::base_address + (((bus * 256) + (device * 8) + function) * 4096) + offset);
}

uint64_t pcie_device::read_config64(uint32_t offset) const
{
	return *reinterpret_cast<const volatile uint64_t*>(pcie::base_address + (((bus * 256) + (device * 8) + function) * 4096) + offset);
}

void pcie_device::write_config32(uint32_t offset, uint32_t data)
{
	*(reinterpret_cast<volatile uint32_t*>(pcie::base_address + (((bus * 256) + (device * 8) + function) * 4096) + offset)) = data;
}

void pcie_device::write_config64(uint32_t offset, uint64_t data)
{
	*(reinterpret_cast<volatile uint64_t*>(pcie::base_address + (((bus * 256) + (device * 8) + function) * 4096) + offset)) = data;
}

uint16_t pcie_device::vendor_id() const
{
	return read_config32(0) & 0xFFFF;
}

uint16_t pcie_device::device_id() const
{
	return read_config32(0) >> 16;
}	

uint8_t pcie_device::class_code() const
{
	return (read_config32(0x8) >> 24) & 0xFF;
}

uint8_t pcie_device::subclass_code() const
{
	return (read_config32(0x8) >> 16) & 0xFF;
}

uint8_t pcie_device::header_type() const
{
	return ((read_config32(0xc) >> 16) & 0xFF) & ~(1 << 7);
}

bool pcie_device::is_valid() const
{
	return vendor_id() != 0xFFFF && device_id() != 0xFFFF;
}

bool pcie_device::is_bridge() const
{
	return header_type() == 0x1 && class_code() == 0x6;
}

uint8_t pcie_device::sub_bus() const
{
	return ((read_config32(0xc) >> 16) & 0xFF) & (1 << 7);
}

bool pcie_device::has_multiple_functions() const
{
	return sub_bus();
}

uint8_t pcie_device::get_bar_width(uint32_t bir)
{
	auto bs = read_config32(0x10 + (bir * 4)) >> 1;

	if(bs == 0x2)
		return 64;

	return 32;
}

uint64_t pcie_device::read_bar(uint32_t bir) const
{
	return read_config64(0x10 + (bir * 4)) & 0xFFFFFFFFFFFFFFF0;
}

size_t pcie_device::get_bar_size(uint32_t bir)
{
	auto old_value = read_bar(bir);

	write_config64(0x10 + (bir * 4), 0xFFFFFFFFFFFFFFFF);

	auto val = read_bar(bir);

	size_t len = ~val;

	write_config64(0x10 + (bir * 4), old_value); 

	return len + 1;
}

uint32_t pcie_device::read_bar32(uint32_t bir) const
{
	return read_config32(0x10 + (bir * 4)) & 0xFFFFFFF0;
}

size_t pcie_device::get_bar32_size(uint32_t bir)
{
	auto old_value = read_bar32(bir);
	write_config32(0x10 + (bir * 4), 0xFFFFFFFF);

	auto val = read_bar32(bir);
	uint32_t len = ~val;
	write_config32(0x10 + (bir * 4), old_value);
	return len + 1;
}

bool pcie_device::enable_msix(msix_descriptor_t& out_descriptor)
{
	uint8_t pcap_ptr = read_config32(0x34) & 0xFC;
	uint8_t msix_ptr = 0;

	while(pcap_ptr)
	{
		auto cap = read_config32(pcap_ptr);

		if((cap & 0xFF) == 0x11)
		{
			msix_ptr = pcap_ptr;
			break;
		}

		pcap_ptr = (cap >> 8) & 0xFC;
	}

	if(!msix_ptr)
		return false;

	auto msix_reg0 = read_config32(msix_ptr);
	uint16_t msix_mctr = msix_reg0 >> 16;
	auto msix_reg1 = read_config32(msix_ptr + 4);

	auto bir = msix_reg1 & 0b111;
	auto table_offset = (msix_reg1 & ~(0b111));
	auto table_size = msix_mctr & 0b11111111111;
	out_descriptor.table_size = table_size;
	log::debug("pcie {:02x}:{:02x}.{} MSI-X using BIR {}, table at {:#x} size {:#x}", bus, device, function, bir, table_offset, table_size);

	auto bar_width = get_bar_width(bir);
	physaddr_t tbl_base = bar_width == 64 ? read_bar(bir) : physaddr_t(read_bar32(bir));
	size_t tbl_barsize = bar_width == 64 ? get_bar_size(bir) : get_bar32_size(bir);

	out_descriptor.table = (uint32_t*)(vm_space_map(vm_get_kernel_space(),
	{
		.length = tbl_barsize, 
		.prot = PROT_READ | PROT_WRITE | PROT_UNCACHED,
		.flags = VM_FLAG_DEVICE,
		.phys_base = tbl_base
	}) + table_offset);

	if(!out_descriptor.table)
		return false;

	msix_reg0 &= ~(1 << 30); // clear FUNCTION_MASK
	msix_reg0 |= (1 << 31); // set ENABLE
	write_config32(msix_ptr, msix_reg0);

	return true;
}

namespace pcie
{

void device_dispatch_driver(pcie_device& dev)
{
	if(dev.class_code() == 1 && dev.subclass_code() == 8)
		nvme_init_controller(dev);
	else if(dev.class_code() == 2 && dev.subclass_code() == 0)
	{
		if(dev.vendor_id() == 0x8086 && dev.device_id() == 0x10d3)
			e1000_init(dev);
	}	
}

void scan_bus(uint8_t bus);

void scan_device(uint8_t bus, uint8_t device)
{
	pcie_device dev{bus, device, 0};

	if(!dev.is_valid())
		return;

	auto* cname = class_to_string(dev.class_code(), dev.subclass_code());

	if(dev.is_bridge())
	{
		log::info("pcie: {:02x}:{:02x}.0 {}", bus, device, cname != nullptr ? cname : "Bridge");
		scan_bus(dev.sub_bus());
		return;
	}

	if(cname)
		log::info("pcie: {:02x}:{:02x}.0 {}", bus, device, cname);
	else	
		log::info("pcie: {:02x}:{:02x}.0 {:#x}:{:#x}", bus, device, dev.vendor_id(), dev.device_id());

	if(dev.has_multiple_functions())
	{
		for(uint8_t i = 1; i < 8; i++)
		{
			pcie_device sub_dev{bus, device, i};
			if(!sub_dev.is_valid())
				continue;

			auto* s_cname = class_to_string(sub_dev.class_code(), sub_dev.subclass_code());

			if(sub_dev.is_bridge())
			{
				scan_bus(sub_dev.sub_bus());
			}
			
			if(s_cname)
				log::info("pcie: {:02x}:{:02x}.{:x} {}", bus, device, i, s_cname);
			else
				log::info("pcie: {:02x}:{:02x}.{:x} {:#x}:{:#x}", bus, device, i, sub_dev.vendor_id(), sub_dev.device_id());

			device_dispatch_driver(sub_dev); 
		}
	}
	
	device_dispatch_driver(dev);
}

void scan_bus(uint8_t bus)
{
	for(uint8_t dev = 0; dev < 32; dev++)
	{
		scan_device(bus, dev);
	}
}

void enumerate()
{
	scan_bus(0);
}

}
