#include <dev/pcie.hpp>
#include <dev/pci_class.hpp>
#include <dev/nvme/nvme.hpp>
#include <dev/net/e1000/e1000.hpp>

#include <mm/layout.hpp>
#include <mm/vmm.hpp>
#include <lib/types.hpp>
#include <lib/klog.hpp>

namespace pcie
{

static virtaddr_t base_address = 0;

void set_base(physaddr_t base)
{
	base_address = base + mm::direct_mapping_offset;
	vm_map_range(base, base_address, 256 * 8 * 8 * 4096, vm_write | vm_mmio);
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

uint64_t pcie_device::read_bar() const
{
	return read_config64(0x10) & 0xFFFFFFFFFFFFFFF0;
}

size_t pcie_device::get_bar_size()
{
	auto old_value = read_bar();

	write_config64(0x10, 0xFFFFFFFFFFFFFFFF);

	auto val = read_bar();

	log::debug("readback lines {:#x}", val);

	size_t len = ~val;

	write_config64(0x10, old_value); 

	return len + 1;
}

uint32_t pcie_device::read_bar32() const
{
	return read_config32(0x10) & 0xFFFFFFF0;
}

size_t pcie_device::get_bar32_size()
{
	auto old_value = read_bar32();
	write_config32(0x10, 0xFFFFFFFF);

	auto val = read_bar32();
	uint32_t len = ~val;
	write_config32(0x10, old_value);
	return len + 1;
}

namespace pcie
{

void device_dispatch_driver(pcie_device& dev)
{
	if(dev.class_code() == 1 && dev.subclass_code() == 8)
		nvme_init_controller(dev);
	if(dev.class_code() == 2 && dev.subclass_code() == 0)
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
