#include <dev/pcie.hpp>
#include <dev/pci_class.hpp>

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


struct pcie_device
{
	uint8_t bus;
	uint8_t device;
	uint8_t function;

	const byte* read_config(uint32_t offset) const
	{
		return reinterpret_cast<const byte*>(base_address + (((bus * 256) + (device * 8) + function) * 4096) + offset);
	}

	uint16_t vendor_id() const
	{
		return *reinterpret_cast<const uint16_t*>(read_config(0));
	}

	uint16_t device_id() const
	{
		return *reinterpret_cast<const uint16_t*>(read_config(2));
	}	
	
	uint8_t class_code() const
	{
		return *reinterpret_cast<const uint8_t*>(read_config(0xb));
	}

	uint8_t subclass_code() const
	{
		return *reinterpret_cast<const uint8_t*>(read_config(0xa));
	}

	uint8_t header_type() const
	{
		return *reinterpret_cast<const uint8_t*>(read_config(0xe)) & ~(1 << 7);
	}

	bool is_valid() const
	{
		return vendor_id() != 0xFFFF && device_id() != 0xFFFF;
	}

	bool is_bridge() const
	{
		return header_type() == 0x1 && class_code() == 0x6;
	}

	uint8_t sub_bus() const
	{
		return *reinterpret_cast<const uint8_t*>(read_config(0xe)) & (1 << 7);
	}

	bool has_multiple_functions() const
	{
		return *reinterpret_cast<const uint8_t*>(read_config(0xe)) & (1 << 7);
	}
};	

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
		}
	}
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
